#include "ui/theme/platformappearance.h"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QWidget>
#include <QtTest>

namespace {
void processEvents() { QCoreApplication::processEvents(QEventLoop::AllEvents, 50); }
} // namespace

class PlatformAppearanceTest final : public QObject {
    Q_OBJECT
  private slots:
    void applyBeforeShowIsSafe() {
        QWidget window;
        PlatformAppearance::apply(window);
        // apply() must not make an ordinary window translucent on any platform.
        QVERIFY(!window.testAttribute(Qt::WA_TranslucentBackground));
    }

    void applyAfterShowIsSafe() {
        QWidget window;
        window.show();
        QVERIFY(window.isVisible());
        processEvents();
        PlatformAppearance::apply(window);
        processEvents();
#ifdef Q_OS_WIN
        // Windows applies the DWM title-bar state; the widget stays opaque.
        QVERIFY(!window.testAttribute(Qt::WA_TranslucentBackground));
#endif
        window.hide();
    }

    void repeatedApplyIsSafe() {
        QWidget window;
        for (int i = 0; i < 3; ++i)
            PlatformAppearance::apply(window);
        window.show();
        processEvents();
        for (int i = 0; i < 3; ++i)
            PlatformAppearance::apply(window);
        processEvents();
        QVERIFY(window.isVisible());
    }

    void applyPreservesTranslucencyOnNonWindows() {
        QWidget opaque;
        PlatformAppearance::apply(opaque);
#ifndef Q_OS_WIN
        QVERIFY(!opaque.testAttribute(Qt::WA_TranslucentBackground));
#endif

        QWidget translucent;
        translucent.setAttribute(Qt::WA_TranslucentBackground, true);
        PlatformAppearance::apply(translucent);
#ifndef Q_OS_WIN
        QVERIFY(translucent.testAttribute(Qt::WA_TranslucentBackground));
#endif
    }

    void applyFollowsApplicationPalette() {
        const auto original = qApp->palette();
        QPalette palette = original;
        for (const auto color : {QColor(28, 28, 30), QColor(245, 245, 247)}) {
            palette.setColor(QPalette::Window, color);
            qApp->setPalette(palette);
            QWidget window;
            PlatformAppearance::apply(window);
            window.show();
            processEvents();
            window.hide();
        }
        qApp->setPalette(original);
    }

    void glassSurfaceFollowsPlatform() {
        QWidget window;
        PlatformAppearance::enableGlassSurface(window);
#ifdef Q_OS_WIN
        // Windows keeps the surface opaque so QSS base colors render cleanly.
        QVERIFY(!window.testAttribute(Qt::WA_TranslucentBackground));
#else
        // The compositor-backed glass path is unchanged on Linux.
        QVERIFY(window.testAttribute(Qt::WA_TranslucentBackground));
#endif
    }
};

QTEST_MAIN(PlatformAppearanceTest)
#include "platform_appearance_test.moc"
