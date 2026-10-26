#include "actionfeedbackoverlay.h"
#include "actionfeedbackgeometry.h"
#include "appearancetokens.h"
#include "uimotionduration.h"

#include <QAbstractAnimation>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QPaintEvent>
#include <QPainter>
#include <QStyleHints>

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
#include <QAccessibilityHints>
#endif

namespace {
int currentMotionDuration() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    const auto preference = QGuiApplication::styleHints()->accessibility()->motionPreference();
    return uiMotionDuration(appearanceTokens(AppearancePreferences::Theme::Dark).motionDurationMs,
                            preference == Qt::MotionPreference::ReducedMotion);
#else
    return uiMotionDuration(appearanceTokens(AppearancePreferences::Theme::Dark).motionDurationMs,
                            false);
#endif
}
} // namespace

ActionFeedbackOverlay::ActionFeedbackOverlay(QWidget *parent)
    : QWidget(parent),
      animation_(new QPropertyAnimation(this, QByteArrayLiteral("contentOpacity"), this)) {
    setObjectName(QStringLiteral("actionFeedbackOverlay"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);
    timer_.setSingleShot(true);
    animation_->setDuration(currentMotionDuration());
    animation_->setEasingCurve(QEasingCurve::OutCubic);
    connect(&timer_, &QTimer::timeout, this, &ActionFeedbackOverlay::clearMessage);
    connect(animation_, &QPropertyAnimation::finished, this, [this] {
        if (qFuzzyIsNull(contentOpacity_)) {
            message_.clear();
            setAccessibleDescription(QString());
            QWidget::hide();
        }
    });
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    connect(QGuiApplication::styleHints()->accessibility(),
            &QAccessibilityHints::motionPreferenceChanged, this,
            [this](Qt::MotionPreference preference) {
                if (preference != Qt::MotionPreference::ReducedMotion ||
                    animation_->state() != QAbstractAnimation::Running)
                    return;
                const qreal target = animation_->endValue().toReal();
                animation_->stop();
                animateContentTo(target);
            });
#endif
    hide();
}

void ActionFeedbackOverlay::showMessage(const QString &message, int durationMs) {
    message_ = message.trimmed();
    setAccessibleDescription(message_);
    if (message_.isEmpty()) {
        clearMessage();
        return;
    }
    animation_->stop();
    timer_.start(qMax(1, durationMs));
    show();
    raise();
    animateContentTo(1.0);
    update();
}

void ActionFeedbackOverlay::clearMessage() {
    timer_.stop();
    if (message_.isEmpty() && !isVisible())
        return;
    animateContentTo(0.0);
}

qreal ActionFeedbackOverlay::contentOpacity() const { return contentOpacity_; }

void ActionFeedbackOverlay::setContentOpacity(qreal opacity) {
    contentOpacity_ = qBound(0.0, opacity, 1.0);
    update();
}

void ActionFeedbackOverlay::animateContentTo(qreal opacity) {
    animation_->stop();
    animation_->setDuration(currentMotionDuration());
    if (animation_->duration() == 0 || qFuzzyCompare(contentOpacity_ + 1, opacity + 1)) {
        setContentOpacity(opacity);
        if (qFuzzyIsNull(opacity)) {
            message_.clear();
            setAccessibleDescription(QString());
            QWidget::hide();
        }
        return;
    }
    animation_->setStartValue(contentOpacity_);
    animation_->setEndValue(opacity);
    animation_->start();
}

void ActionFeedbackOverlay::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    if (message_.isEmpty())
        return;

    QFont font = this->font();
    font.setPixelSize(18);
    const QFontMetrics metrics(font);
    const int maxTextWidth = qMax(1, width() - 64);
    const QRect textBounds = metrics.boundingRect(QRect(0, 0, maxTextWidth, height()),
                                                  Qt::AlignCenter | Qt::TextWordWrap, message_);
    const auto layout = ActionFeedbackGeometry::arrange(size(), textBounds.size());
    if (!layout.panel.isValid())
        return;

    QPainter painter(this);
    painter.setOpacity(contentOpacity_);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(font);
    painter.setPen(Qt::NoPen);
    auto panelColor = palette().color(QPalette::AlternateBase);
    panelColor.setAlpha(218);
    painter.setBrush(panelColor);
    painter.drawRoundedRect(layout.panel, 6, 6);
    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(layout.text, Qt::AlignCenter | Qt::TextWordWrap, message_);
}
