#include "fullscreencontrols.h"
#include "appearancetokens.h"
#include "uimotionduration.h"

#include <QAbstractSlider>
#include <QApplication>
#include <QEvent>
#include <QFocusEvent>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QPropertyAnimation>
#include <QStyleHints>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
#include <QAccessibilityHints>
#endif

namespace {
int currentMotionDuration(int requestedDurationMs) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    const auto preference = QGuiApplication::styleHints()->accessibility()->motionPreference();
    return uiMotionDuration(requestedDurationMs, preference == Qt::MotionPreference::ReducedMotion);
#else
    return uiMotionDuration(requestedDurationMs, false);
#endif
}
} // namespace

FullscreenControls::FullscreenControls(QWidget &transport, QWidget &video,
                                       QVBoxLayout &normalLayout, QObject *parent)
    : QObject(parent), transport_(transport), video_(video),
      normalParent_(transport.parentWidget()), normalLayout_(normalLayout),
      opacity_(new QGraphicsOpacityEffect(&transport)),
      animation_(new QPropertyAnimation(opacity_, QByteArrayLiteral("opacity"), this)) {
    setObjectName(QStringLiteral("fullscreenControls"));
    opacity_->setOpacity(1);
    opacity_->setEnabled(false);
    transport_.setGraphicsEffect(opacity_);
    animation_->setDuration(currentMotionDuration(
        appearanceTokens(AppearancePreferences::Theme::Dark).motionDurationMs));
    animation_->setEasingCurve(QEasingCurve::OutCubic);
    connect(animation_, &QPropertyAnimation::finished, this, &FullscreenControls::finishFadeOut);
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    connect(QGuiApplication::styleHints()->accessibility(),
            &QAccessibilityHints::motionPreferenceChanged, this,
            [this](Qt::MotionPreference preference) {
                if (preference != Qt::MotionPreference::ReducedMotion ||
                    animation_->state() != QAbstractAnimation::Running)
                    return;
                const qreal target = animation_->endValue().toReal();
                animation_->stop();
                animateTo(target);
            });
#endif
    video_.installEventFilter(this);
    // Other overlays can raise themselves after a seek or Tab toggle.
    for (auto *child : video_.findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
        child->installEventFilter(this);
    transport_.installEventFilter(this);
    for (auto *child : transport_.findChildren<QWidget *>())
        child->installEventFilter(this);
}

void FullscreenControls::enter() {
    if (active_)
        return;
    active_ = true;
    keyboardFocus_ = false;
    animation_->stop();
    normalIndex_ = normalLayout_.indexOf(&transport_);
    normalLayout_.removeWidget(&transport_);
    transport_.hide();
    transport_.setParent(&video_);
    transport_.setAttribute(Qt::WA_TransparentForMouseEvents);
    opacity_->setEnabled(true);
    opacity_->setOpacity(0);
    normalLayout_.activate();
    arrange();
}

void FullscreenControls::leave() {
    if (!active_)
        return;
    active_ = false;
    keyboardFocus_ = false;
    animation_->stop();
    transport_.hide();
    transport_.setParent(normalParent_);
    normalLayout_.insertWidget(normalIndex_, &transport_);
    opacity_->setOpacity(1);
    opacity_->setEnabled(false);
    transport_.setAttribute(Qt::WA_TransparentForMouseEvents, false);
    transport_.show();
}

void FullscreenControls::reveal() {
    if (!active_)
        return;
    transport_.setAttribute(Qt::WA_TransparentForMouseEvents, false);
    transport_.show();
    arrange();
    animateTo(1);
}

void FullscreenControls::fadeOut() {
    if (active_ && transport_.isVisible())
        animateTo(0);
}

bool FullscreenControls::interactionActive() const {
    if (!transport_.isVisible())
        return false;
    const auto belongsToTransport = [this](QWidget *widget) {
        return widget && (widget == &transport_ || transport_.isAncestorOf(widget));
    };
    if (transport_.underMouse() || belongsToTransport(QWidget::mouseGrabber()) ||
        (keyboardFocus_ && belongsToTransport(QApplication::focusWidget())))
        return true;
    for (auto *slider : transport_.findChildren<QAbstractSlider *>()) {
        if (slider->isSliderDown())
            return true;
    }
    return false;
}

void FullscreenControls::arrange() {
    if (!active_)
        return;
    const auto height = qMin(video_.height(), qMax(1, transport_.sizeHint().height()));
    transport_.setGeometry(0, video_.height() - height, video_.width(), height);
    transport_.raise();
}

void FullscreenControls::animateTo(qreal opacity) {
    if (animation_->state() == QAbstractAnimation::Running &&
        animation_->endValue().toReal() == opacity)
        return;
    animation_->stop();
    if (opacity_->opacity() == opacity)
        return;
    animation_->setDuration(currentMotionDuration(
        appearanceTokens(AppearancePreferences::Theme::Dark).motionDurationMs));
    if (animation_->duration() == 0) {
        opacity_->setOpacity(opacity);
        finishFadeOut();
        return;
    }
    animation_->setStartValue(opacity_->opacity());
    animation_->setEndValue(opacity);
    animation_->start();
}

void FullscreenControls::finishFadeOut() {
    if (active_ && opacity_->opacity() == 0) {
        transport_.setAttribute(Qt::WA_TransparentForMouseEvents);
        transport_.hide();
    }
}

bool FullscreenControls::eventFilter(QObject *watched, QEvent *event) {
    if (!active_)
        return QObject::eventFilter(watched, event);
    auto *widget = qobject_cast<QWidget *>(watched);
    const auto inTransport = widget && (widget == &transport_ || transport_.isAncestorOf(widget));
    if (inTransport) {
        if (event->type() == QEvent::FocusIn) {
            const auto reason = static_cast<QFocusEvent *>(event)->reason();
            keyboardFocus_ = reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason ||
                             reason == Qt::ShortcutFocusReason;
            if (keyboardFocus_)
                reveal();
        } else if (event->type() == QEvent::KeyPress) {
            keyboardFocus_ = true;
            reveal();
        } else if (event->type() == QEvent::MouseButtonPress)
            keyboardFocus_ = false;
    }
    if (watched == &video_ && event->type() == QEvent::Resize)
        arrange();
    else if (!inTransport && watched != &video_ && transport_.isVisible() &&
             (event->type() == QEvent::Show || event->type() == QEvent::ZOrderChange)) {
        QTimer::singleShot(0, &transport_, [this] {
            if (active_ && transport_.isVisible())
                transport_.raise();
        });
    }
    return QObject::eventFilter(watched, event);
}
