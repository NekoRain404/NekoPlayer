#include "transportbar.h"
#include "playback/mpvcontroller.h"
#include "seekslidergeometry.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace {
class SeekSlider final : public QSlider {
  public:
    explicit SeekSlider(QWidget *parent) : QSlider(Qt::Horizontal, parent) {}

    std::function<void(int)> onDirectSeek;

  protected:
    void mousePressEvent(QMouseEvent *event) override {
        const auto option = sliderOption();
        if (event->button() != Qt::LeftButton ||
            style()->hitTestComplexControl(QStyle::CC_Slider, &option, event->pos(), this) ==
                QStyle::SC_SliderHandle) {
            QSlider::mousePressEvent(event);
            return;
        }

        event->accept();
        absoluteSeekActive_ = true;
        movedAfterClick_ = false;
        setSliderDown(true);
        seekAt(event->pos());
        if (onDirectSeek)
            onDirectSeek(value());
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (!absoluteSeekActive_) {
            QSlider::mouseMoveEvent(event);
            return;
        }
        event->accept();
        const int previous = sliderPosition();
        seekAt(event->pos());
        movedAfterClick_ = movedAfterClick_ || sliderPosition() != previous;
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (!absoluteSeekActive_) {
            QSlider::mouseReleaseEvent(event);
            return;
        }
        event->accept();
        absoluteSeekActive_ = false;
        suppressReleaseSeek_ = !movedAfterClick_;
        setSliderDown(false);
        // A press already seeks immediately; only a subsequent drag needs a release seek.
    }

  public:
    bool takeSuppressedReleaseSeek() { return std::exchange(suppressReleaseSeek_, false); }

  private:
    QStyleOptionSlider sliderOption() const {
        QStyleOptionSlider option;
        initStyleOption(&option);
        return option;
    }

    void seekAt(const QPoint &point) {
        const auto option = sliderOption();
        const auto groove =
            style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderGroove, this);
        const auto handle =
            style()
                ->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this)
                .size();
        const int target = SeekSliderGeometry::valueAtPosition(option, groove, handle, point);
        setSliderPosition(target);
        setValue(target);
    }

    bool absoluteSeekActive_ = false;
    bool movedAfterClick_ = false;
    bool suppressReleaseSeek_ = false;
};

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
    : QWidget(parent), timeline_(new SeekSlider(this)), volume_(new QSlider(Qt::Horizontal, this)),
      time_(new QLabel(this)) {
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
    connect(timeline_, &QSlider::sliderReleased, this, [this] {
        auto *seekSlider = static_cast<SeekSlider *>(timeline_);
        if (!seekSlider->takeSuppressedReleaseSeek())
            emit seekRequested(duration_ * timeline_->value() / 10000.0);
    });
    static_cast<SeekSlider *>(timeline_)->onDirectSeek = [this](int value) {
        emit seekRequested(duration_ * value / 10000.0);
    };
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
