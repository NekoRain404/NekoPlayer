#include "windowopacitytransition.h"

#include "appearancepreferences.h"
#include "appearancetokens.h"
#include "uimotionduration.h"

#include <QGuiApplication>
#include <QPropertyAnimation>
#include <QStyleHints>
#include <QWidget>

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
#include <QAccessibilityHints>
#endif

namespace {
int motionDuration() {
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

WindowOpacityTransition::WindowOpacityTransition(QWidget &window, QObject *parent)
    : QObject(parent), window_(window),
      animation_(new QPropertyAnimation(&window, QByteArrayLiteral("windowOpacity"), this)) {
    animation_->setEasingCurve(uiMotionEasingCurve());
    connect(animation_, &QPropertyAnimation::finished, this,
            [this] { emit reached(window_.windowOpacity()); });
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    connect(QGuiApplication::styleHints()->accessibility(),
            &QAccessibilityHints::motionPreferenceChanged, this,
            [this](Qt::MotionPreference preference) {
                if (preference != Qt::MotionPreference::ReducedMotion || !isRunning())
                    return;
                const auto target = targetOpacity();
                animation_->stop();
                fadeTo(target);
            });
#endif
}

void WindowOpacityTransition::fadeTo(qreal opacity) {
    animation_->stop();
    const auto duration = motionDuration();
    if (duration == 0 || qFuzzyCompare(window_.windowOpacity() + 1.0, opacity + 1.0)) {
        window_.setWindowOpacity(opacity);
        emit reached(opacity);
        return;
    }
    animation_->setDuration(duration);
    animation_->setStartValue(window_.windowOpacity());
    animation_->setEndValue(opacity);
    animation_->start();
}

void WindowOpacityTransition::stopAt(qreal opacity) {
    animation_->stop();
    window_.setWindowOpacity(opacity);
}

bool WindowOpacityTransition::isRunning() const {
    return animation_->state() == QAbstractAnimation::Running;
}

qreal WindowOpacityTransition::targetOpacity() const { return animation_->endValue().toReal(); }
