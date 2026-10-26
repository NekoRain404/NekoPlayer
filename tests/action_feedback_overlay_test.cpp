#include "ui/actionfeedbackoverlay.h"
#include "ui/appearancepreferences.h"
#include "ui/appearancetokens.h"

#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QPropertyAnimation>
#include <QStyleHints>
#include <QtTest>

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
#include <QAccessibilityHints>
#endif

namespace {
int expectedMotionDuration() {
    const int duration = appearanceTokens(AppearancePreferences::Theme::Dark).motionDurationMs;
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    if (QGuiApplication::styleHints()->accessibility()->motionPreference() ==
        Qt::MotionPreference::ReducedMotion)
        return 0;
#endif
    return duration;
}
} // namespace

class ActionFeedbackOverlayTest final : public QObject {
    Q_OBJECT
  private slots:
    void feedbackUsesSharedMotionWithoutGraphicsEffect() {
        QWidget video;
        ActionFeedbackOverlay overlay(&video);
        auto *animation = overlay.findChild<QPropertyAnimation *>();
        QVERIFY(animation);
        QCOMPARE(animation->duration(), expectedMotionDuration());
        QCOMPARE(animation->easingCurve().type(), QEasingCurve::OutCubic);
        QVERIFY(!qobject_cast<QGraphicsOpacityEffect *>(overlay.graphicsEffect()));

        video.resize(640, 360);
        video.show();
        overlay.resize(video.size());
        overlay.showMessage(QStringLiteral("前进 5 秒"), 5000);
        QVERIFY(overlay.isVisible());
        QCOMPARE(overlay.property("contentOpacity").toReal(), 0.0);
        QCOMPARE(animation->endValue().toReal(), 1.0);
        animation->setCurrentTime(animation->duration());
        QCOMPARE(overlay.property("contentOpacity").toReal(), 1.0);

        overlay.clearMessage();
        QCOMPARE(animation->endValue().toReal(), 0.0);
        animation->setCurrentTime(animation->duration());
        QVERIFY(overlay.isHidden());
        QCOMPARE(overlay.property("contentOpacity").toReal(), 0.0);
    }

    void replacingFeedbackDuringFadeDoesNotHideNewMessage() {
        QWidget video;
        ActionFeedbackOverlay overlay(&video);
        auto *animation = overlay.findChild<QPropertyAnimation *>();
        QVERIFY(animation);

        video.resize(640, 360);
        video.show();
        overlay.resize(video.size());
        overlay.showMessage(QStringLiteral("前进 5 秒"), 5000);
        if (animation->duration() == 0) {
            QCOMPARE(overlay.contentOpacity(), 1.0);
            overlay.clearMessage();
            QVERIFY(overlay.isHidden());
            overlay.showMessage(QStringLiteral("后退 5 秒"), 5000);
            QCOMPARE(overlay.accessibleDescription(), QStringLiteral("后退 5 秒"));
            QVERIFY(overlay.isVisible());
            QCOMPARE(overlay.contentOpacity(), 1.0);
            return;
        }

        animation->setCurrentTime(animation->duration() / 2);
        const qreal partialOpacity = overlay.contentOpacity();
        QVERIFY(partialOpacity > 0.0);
        QVERIFY(partialOpacity < 1.0);

        overlay.clearMessage();
        QCOMPARE(animation->endValue().toReal(), 0.0);
        animation->setCurrentTime(animation->duration() / 2);
        QVERIFY(overlay.isVisible());

        overlay.showMessage(QStringLiteral("后退 5 秒"), 5000);
        QCOMPARE(overlay.accessibleDescription(), QStringLiteral("后退 5 秒"));
        QCOMPARE(animation->endValue().toReal(), 1.0);
        animation->setCurrentTime(animation->duration());
        QVERIFY(overlay.isVisible());
        QCOMPARE(overlay.contentOpacity(), 1.0);
    }
};

QTEST_MAIN(ActionFeedbackOverlayTest)
#include "action_feedback_overlay_test.moc"
