#include "playback/mpv/mpvcontroller.h"
#include "ui/menus/screenshotmenu.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QSignalSpy>
#include <QWidget>
#include <QtTest>

class ScreenshotMenuTest final : public QObject {
    Q_OBJECT
  private slots:
    void copyActionPublishesTheCurrentFrame() {
        MpvController player(true, true);
        QWidget window;
        ScreenshotMenu menu(player, &window);
        auto *copy = window.findChild<QAction *>(QStringLiteral("copyScreenshotAction"));
        QVERIFY(copy);
        QSignalSpy feedback(&menu, &ScreenshotMenu::feedback);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(player.canTakeScreenshot());
        player.setPaused(true);
        QVERIFY(copy->isEnabled());
        copy->trigger();
        QTRY_VERIFY(!feedback.isEmpty());
        QVERIFY(!QApplication::clipboard()->image().isNull());
    }
};

QTEST_MAIN(ScreenshotMenuTest)
#include "screenshot_menu_test.moc"
