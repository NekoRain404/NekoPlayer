#include "transportbar.h"
#include "playback/mpvcontroller.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {
QString timeText(double seconds) {
    const auto total = static_cast<qint64>(std::max(0.0, seconds));
    return QStringLiteral("%1:%2:%3")
        .arg(total / 3600, 2, 10, QLatin1Char('0'))
        .arg((total / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(total % 60, 2, 10, QLatin1Char('0'));
}
QToolButton *button(QAction *action, QWidget *parent) {
    auto *result = new QToolButton(parent);
    result->setDefaultAction(action);
    result->setToolButtonStyle(Qt::ToolButtonIconOnly);
    result->setIconSize(QSize(22, 22));
    result->setMinimumSize(36, 32);
    result->setFocusPolicy(Qt::NoFocus);
    return result;
}
} // namespace

TransportBar::TransportBar(QAction *open, QAction *previous, QAction *pause, QAction *next,
                           QAction *stop, QAction *mute, QAction *fullscreen, QWidget *parent)
    : QWidget(parent), timeline_(new QSlider(Qt::Horizontal, this)),
      volume_(new QSlider(Qt::Horizontal, this)), time_(new QLabel(this)) {
    setObjectName(QStringLiteral("transportBar"));
    timeline_->setObjectName(QStringLiteral("timelineSlider"));
    timeline_->setRange(0, 10000);
    timeline_->setAccessibleName(tr("播放进度"));
    volume_->setObjectName(QStringLiteral("volumeSlider"));
    volume_->setRange(0, 100);
    volume_->setValue(100);
    volume_->setFixedWidth(110);
    volume_->setAccessibleName(tr("音量"));
    time_->setMinimumWidth(155);
    time_->setObjectName(QStringLiteral("timeLabel"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 4, 12, 8);
    layout->setSpacing(4);
    layout->addWidget(timeline_);
    auto *row = new QHBoxLayout;
    row->setSpacing(4);
    row->addWidget(button(previous, this));
    row->addWidget(button(pause, this));
    row->addWidget(button(next, this));
    row->addWidget(button(stop, this));
    row->addWidget(button(open, this));
    row->addSpacing(10);
    row->addWidget(time_);
    row->addStretch();
    row->addWidget(button(mute, this));
    row->addWidget(volume_);
    row->addSpacing(8);
    row->addWidget(button(fullscreen, this));
    layout->addLayout(row);
    connect(timeline_, &QSlider::sliderReleased, this,
            [this] { emit seekRequested(duration_ * timeline_->value() / 10000.0); });
    connect(timeline_, &QSlider::actionTriggered, this, [this](int action) {
        if (action != QAbstractSlider::SliderMove)
            emit seekRequested(duration_ * timeline_->sliderPosition() / 10000.0);
    });
    connect(volume_, &QSlider::valueChanged, this,
            [this](int value) { emit volumeRequested(value); });
    connect(timeline_, &QSlider::sliderMoved, this, [this](int value) {
        time_->setText(timeText(duration_ * value / 10000.0) + QStringLiteral(" / ") +
                       timeText(duration_));
    });
}

void TransportBar::present(const PlaybackState &state) {
    duration_ = state.duration;
    timeline_->setEnabled(state.active && state.seekable && duration_ > 0);
    if (!timeline_->isSliderDown()) {
        const QSignalBlocker block(timeline_);
        timeline_->setValue(duration_ > 0 ? qRound(10000 * state.position / duration_) : 0);
    }
    const QSignalBlocker block(volume_);
    if (!volume_->isSliderDown())
        volume_->setValue(qRound(state.volume));
    volume_->setToolTip(tr("音量 %1%").arg(qRound(state.volume)));
    const auto position = timeline_->isSliderDown()
                              ? duration_ * timeline_->sliderPosition() / 10000.0
                              : state.position;
    time_->setText(timeText(position) + QStringLiteral(" / ") + timeText(duration_));
}
