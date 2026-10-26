#include "ui/windowopacitytransition.h"

#include <QTest>
#include <QWidget>

class WindowOpacityTransitionTest final : public QObject {
    Q_OBJECT
  private slots:
    void stopAtCancelsPendingFadeAndRestoresExactOpacity() {
        QWidget window;
        window.setWindowOpacity(0.35);
        WindowOpacityTransition transition(window);
        transition.fadeTo(0.0);
        transition.stopAt(1.0);
        QVERIFY(!transition.isRunning());
        QCOMPARE(window.windowOpacity(), 1.0);
    }
};

QTEST_MAIN(WindowOpacityTransitionTest)
#include "window_opacity_transition_test.moc"
