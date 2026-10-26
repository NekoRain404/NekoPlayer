#include "transportbar.h"
#include "appearancetokens.h"
#include "interactionpolicy.h"
#include "playback/mpvcontroller.h"
#include "seekslidergeometry.h"
#include "transportbarmetrics.h"

#include <QAction>
#include <QCursor>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace {
class SeekSlider final : public QSlider {
  public:
    explicit SeekSlider(QWidget *parent) : QSlider(Qt::Horizontal, parent) {}

    std::function<void(int)> onDirectSeek;
    std::function<void(int, QPoint)> onHover;
    std::function<void()> onHoverEnd;
    std::function<void(int)> onWheel;

  protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (onHoverEnd)
            onHoverEnd();
        lastHoverValue_ = -1;
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
            const int hoverValue = valueAtPosition(event->pos());
            if (onHover && hoverValue != lastHoverValue_) {
                lastHoverValue_ = hoverValue;
                onHover(hoverValue, event->globalPosition().toPoint());
            }
            return;
        }
        event->accept();
        const int previous = sliderPosition();
        seekAt(event->pos());
        movedAfterClick_ = movedAfterClick_ || sliderPosition() != previous;
        if (onHover)
            onHover(value(), event->globalPosition().toPoint());
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

    void leaveEvent(QEvent *event) override {
        QToolTip::hideText();
        lastHoverValue_ = -1;
        if (onHoverEnd)
            onHoverEnd();
        QSlider::leaveEvent(event);
    }

    void wheelEvent(QWheelEvent *event) override {
        // Never fall through to QSlider's hidden single-step seek; the window
        // decides whether timeline wheel seeking is enabled.
        event->accept();
        if (onWheel)
            onWheel(event->angleDelta().y());
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
        const int target = valueAtPosition(point);
        setSliderPosition(target);
        setValue(target);
    }

    int valueAtPosition(const QPoint &point) const {
        const auto option = sliderOption();
        const auto groove =
            style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderGroove, this);
        const auto handle =
            style()
                ->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this)
                .size();
        return SeekSliderGeometry::valueAtPosition(option, groove, handle, point);
    }

    bool absoluteSeekActive_ = false;
    bool movedAfterClick_ = false;
    bool suppressReleaseSeek_ = false;
    int lastHoverValue_ = -1;
};

class ClickableTimeLabel final : public QLabel {
  public:
    explicit ClickableTimeLabel(QWidget *parent) : QLabel(parent) {
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
    }

    std::function<void()> onActivate;

  protected:
    bool event(QEvent *event) override {
        if (event->type() == QEvent::ShortcutOverride && hasFocus()) {
            const auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (isActivationKey(keyEvent->key())) {
                event->accept();
                return true;
            }
        }
        return QLabel::event(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
            event->accept();
            activate();
            return;
        }
        QLabel::mouseReleaseEvent(event);
    }

    void paintEvent(QPaintEvent *event) override {
        QLabel::paintEvent(event);
        if (!hasFocus())
            return;
        QPainter painter(this);
        painter.setPen(palette().color(QPalette::Highlight));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }

    void keyPressEvent(QKeyEvent *event) override {
        if (!isActivationKey(event->key())) {
            QLabel::keyPressEvent(event);
            return;
        }
        event->accept();
        if (!event->isAutoRepeat())
            activate();
    }

  private:
    static bool isActivationKey(int key) {
        return key == Qt::Key_Space || key == Qt::Key_Return || key == Qt::Key_Enter;
    }

    void activate() {
        if (onActivate)
            onActivate();
    }
};

QString timeText(double seconds) {
    const auto total = static_cast<qint64>(std::max(0.0, seconds));
    return QStringLiteral("%1:%2:%3")
        .arg(total / 3600, 2, 10, QLatin1Char('0'))
        .arg((total / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(total % 60, 2, 10, QLatin1Char('0'));
}
QToolButton *button(QAction *action, QWidget *parent, bool primary = false) {
    auto *result = new QToolButton(parent);
    result->setDefaultAction(action);
    if (primary) {
        result->setObjectName(QStringLiteral("transportPrimaryButton"));
        result->setProperty("transportPrimaryAction", true);
    }
    result->setToolButtonStyle(Qt::ToolButtonIconOnly);
    result->setIconSize(primary ? QSize(26, 26) : QSize(22, 22));
    const auto tokens = appearanceTokens(AppearancePreferences::Theme::Dark);
    result->setMinimumSize(tokens.controlHeight, tokens.rowHeight);
    result->setFocusPolicy(Qt::StrongFocus);
    const auto updateDescription = [result, action] {
        result->setAccessibleName(action->text());
        result->setToolTip(action->text());
    };
    updateDescription();
    QObject::connect(action, &QAction::changed, result, updateDescription);
    return result;
}

QFrame *divider(QWidget *parent) {
    auto *result = new QFrame(parent);
    result->setObjectName(QStringLiteral("transportDivider"));
    result->setFrameShape(QFrame::VLine);
    result->setFrameShadow(QFrame::Plain);
    result->setFixedWidth(1);
    const auto tokens = appearanceTokens(AppearancePreferences::Theme::Dark);
    result->setMinimumHeight(tokens.spaceUnit * 4);
    result->setMaximumHeight(tokens.spaceUnit * 6);
    return result;
}
} // namespace

TransportBar::TransportBar(QAction *open, QAction *previous, QAction *pause, QAction *next,
                           QAction *stop, QAction *mute, QAction *fullscreen, QWidget *parent)
    : QWidget(parent), timeline_(new SeekSlider(this)), volume_(new QSlider(Qt::Horizontal, this)),
      time_(new ClickableTimeLabel(this)), timeDivider_(divider(this)),
      audioDivider_(divider(this)) {
    setObjectName(QStringLiteral("transportBar"));
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    timeline_->setObjectName(QStringLiteral("timelineSlider"));
    timeline_->setRange(0, 10000);
    timeline_->setAccessibleName(tr("播放进度"));
    timeline_->setMouseTracking(true);
    volume_->setObjectName(QStringLiteral("volumeSlider"));
    volume_->setRange(0, 100);
    volume_->setValue(100);
    volume_->setFixedWidth(110);
    volume_->setAccessibleName(tr("音量"));
    time_->setMinimumWidth(155);
    time_->setObjectName(QStringLiteral("timeLabel"));
    time_->setAccessibleName(tr("播放时间"));
    static_cast<ClickableTimeLabel *>(time_)->onActivate = [this] {
        timeShowsRemaining_ = !timeShowsRemaining_;
        updateTimeLabel();
    };
    const auto tokens = appearanceTokens(AppearancePreferences::Theme::Dark);
    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetNoConstraint);
    layout->setContentsMargins(tokens.spaceUnit * 3, tokens.spaceUnit * 2, tokens.spaceUnit * 3,
                               tokens.spaceUnit * 2);
    layout->setSpacing(tokens.spaceUnit * 2);
    layout->addWidget(timeline_);
    auto *row = new QHBoxLayout;
    row->setSpacing(tokens.spaceUnit);
    row->addWidget(button(previous, this));
    row->addWidget(button(pause, this, true));
    row->addWidget(button(next, this));
    row->addWidget(button(stop, this));
    row->addWidget(button(open, this));
    row->addSpacing(tokens.spaceUnit * 2);
    row->addWidget(timeDivider_);
    row->addSpacing(tokens.spaceUnit);
    row->addWidget(time_);
    row->addStretch();
    row->addWidget(audioDivider_);
    row->addSpacing(tokens.spaceUnit);
    row->addWidget(button(mute, this));
    row->addWidget(volume_);
    row->addSpacing(tokens.spaceUnit * 2);
    row->addWidget(button(fullscreen, this));
    layout->addLayout(row);
    setMinimumSize(0, 0);
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
        position_ = duration_ * value / 10000.0;
        updateTimeLabel();
        const auto text = timeText(position_) + QStringLiteral(" / ") + timeText(duration_);
        timeline_->setToolTip(text);
        timeline_->setAccessibleDescription(text);
    });
    static_cast<SeekSlider *>(timeline_)->onHover = [this](int value,
                                                           const QPoint &globalPosition) {
        if (duration_ <= 0)
            return;
        const auto text =
            timeText(duration_ * value / 10000.0) + QStringLiteral(" / ") + timeText(duration_);
        timeline_->setToolTip(text);
        timeline_->setAccessibleDescription(text);
        QToolTip::showText(QCursor::pos(), text, timeline_);
        emit timelineHovered(duration_ * value / 10000.0, globalPosition);
    };
    static_cast<SeekSlider *>(timeline_)->onHoverEnd = [this] { emit timelineHoverEnded(); };
    static_cast<SeekSlider *>(timeline_)->onWheel = [this](int deltaY) {
        const int notches = wheelNotches(deltaY);
        if (notches != 0)
            emit wheelSeekRequested(notches > 0 ? 1 : -1);
    };
    updateTimeLabel();
    applyResponsiveMetrics();
}

void TransportBar::setTimeShowsRemaining(bool remaining) {
    if (timeShowsRemaining_ == remaining)
        return;
    timeShowsRemaining_ = remaining;
    updateTimeLabel();
}

bool TransportBar::timeShowsRemaining() const { return timeShowsRemaining_; }

QRect TransportBar::timelineGlobalGeometry() const {
    return QRect(timeline_->mapToGlobal(QPoint()), timeline_->size());
}

void TransportBar::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    applyResponsiveMetrics();
}

void TransportBar::applyResponsiveMetrics() {
    const auto metrics = TransportBarMetrics::forWidth(width());
    const int buttonHeight = metrics.buttonExtent == 36 ? 32 : metrics.buttonExtent;
    for (auto *toolButton : findChildren<QToolButton *>(QString(), Qt::FindDirectChildrenOnly))
        toolButton->setFixedSize(metrics.buttonExtent, buttonHeight);

    volume_->setVisible(metrics.showVolume);
    volume_->setFixedWidth(metrics.showVolume ? metrics.volumeWidth : 0);
    time_->setVisible(metrics.showTime);
    timeDivider_->setVisible(metrics.showTime);
    time_->setMinimumWidth(metrics.showTime ? metrics.timeWidth : 0);
    time_->setMaximumWidth(metrics.showTime ? metrics.timeWidth : 0);
    if (layout()) {
        layout()->invalidate();
        layout()->activate();
    }
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
    position_ = position;
    updateTimeLabel();
    const auto timelineText = timeText(position) + QStringLiteral(" / ") + timeText(duration_);
    timeline_->setToolTip(timelineText);
    timeline_->setAccessibleDescription(timelineText);
}

void TransportBar::updateTimeLabel() {
    const auto total = timeText(duration_);
    const auto remaining = timeText(std::max(0.0, duration_ - position_));
    const QString reading = timeShowsRemaining_
                                ? QStringLiteral("-%1 / %2").arg(remaining, total)
                                : QStringLiteral("%1 / %2").arg(timeText(position_), total);
    const auto alternate = timeShowsRemaining_ ? tr("已播放 %1").arg(timeText(position_))
                                               : tr("剩余 %1").arg(remaining);
    const auto action = timeShowsRemaining_ ? tr("按空格键或回车键切换为已播放时间")
                                            : tr("按空格键或回车键切换为剩余时间");
    const auto description = reading + QLatin1Char('\n') + alternate + QLatin1Char('\n') + action;
    const auto toolTip = description + QLatin1Char('\n') + tr("点击切换时间显示");
    time_->setText(reading);
    time_->setToolTip(toolTip);
    time_->setAccessibleDescription(description);
}

void TransportBar::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() != QEvent::LanguageChange)
        return;
    timeline_->setAccessibleName(tr("播放进度"));
    volume_->setAccessibleName(tr("音量"));
    time_->setAccessibleName(tr("播放时间"));
    updateTimeLabel();
    volume_->setToolTip(tr("音量 %1%").arg(volume_->value()));
}
