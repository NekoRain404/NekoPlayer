#include "ui/window/interactionpolicy.h"
#include "ui/window/uimotionduration.h"

#include <QEasingCurve>
#include <QTest>

class InteractionPolicyTest final : public QObject {
    Q_OBJECT
  private slots:
    void wheelNotchesUsesOneHundredTwentyUnitsPerStep() {
        QCOMPARE(wheelNotches(0), 0);
        QCOMPARE(wheelNotches(120), 1);
        QCOMPARE(wheelNotches(240), 2);
        QCOMPARE(wheelNotches(360), 3);
        QCOMPARE(wheelNotches(-120), -1);
        QCOMPARE(wheelNotches(-240), -2);
    }

    void nonzeroWheelDeltaHasAtLeastOneNotch() {
        QCOMPARE(wheelNotches(1), 1);
        QCOMPARE(wheelNotches(60), 1);
        QCOMPARE(wheelNotches(119), 1);
        QCOMPARE(wheelNotches(180), 1);
        QCOMPARE(wheelNotches(-1), -1);
        QCOMPARE(wheelNotches(-60), -1);
        QCOMPARE(wheelNotches(-359), -2);
    }

    void volumeAfterWheelAppliesStepPerNotch() {
        QCOMPARE(volumeAfterWheel(50, 120, 5), 55.0);
        QCOMPARE(volumeAfterWheel(50, -120, 5), 45.0);
        QCOMPARE(volumeAfterWheel(50, 240, 5), 60.0);
        QCOMPARE(volumeAfterWheel(50, -240, 5), 40.0);
        QCOMPARE(volumeAfterWheel(50, 0, 5), 50.0);
    }

    void volumeAfterWheelClampsToValidRange() {
        QCOMPARE(volumeAfterWheel(98, 120, 5), 100.0);
        QCOMPARE(volumeAfterWheel(100, 120, 5), 100.0);
        QCOMPARE(volumeAfterWheel(2, -120, 5), 0.0);
        QCOMPARE(volumeAfterWheel(0, -120, 5), 0.0);
        QCOMPARE(volumeAfterWheel(0, 240, 20), 40.0);
    }

    void reducedMotionDisablesOptionalUiAnimation() {
        QCOMPARE(uiMotionDuration(160, true), 0);
        QCOMPARE(uiMotionDuration(160, false), 160);
        QCOMPARE(uiMotionDuration(-5, false), 0);
    }

    void optionalUiAnimationsShareAnEaseOutCurve() {
        QCOMPARE(uiMotionEasingCurve().type(), QEasingCurve::OutCubic);
    }
};

QTEST_GUILESS_MAIN(InteractionPolicyTest)
#include "interaction_policy_test.moc"
