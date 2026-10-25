#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "ui/playlistmodemenu.h"

#include <QAction>
#include <QMenu>
#include <QWidget>
#include <QtTest>

class PlaylistModeMenuTest final : public QObject {
    Q_OBJECT
  private slots:
    void stopAfterCurrentActionReflectsControllerAndRetranslates() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QWidget host;
        PlaylistModeMenu modeMenu(queue, &host);

        auto *stop =
            modeMenu.menu()->findChild<QAction *>(QStringLiteral("stopAfterCurrentAction"));
        QVERIFY(stop);
        QVERIFY(stop->isCheckable());
        QVERIFY(!stop->isChecked());
        QCOMPARE(stop->text(), QStringLiteral("播放完当前停止"));

        queue.setStopAfterCurrent(true);
        QVERIFY(stop->isChecked());
        stop->trigger();
        QVERIFY(!queue.stopAfterCurrent());
        QVERIFY(!stop->isChecked());
        stop->trigger();
        QVERIFY(queue.stopAfterCurrent());
        QVERIFY(stop->isChecked());

        auto *repeatAll = modeMenu.menu()->findChild<QAction *>(QStringLiteral("repeatAllAction"));
        auto *shuffle = modeMenu.menu()->findChild<QAction *>(QStringLiteral("shuffleAction"));
        QVERIFY(repeatAll);
        QVERIFY(shuffle);
        stop->setText(QStringLiteral("stale"));
        repeatAll->setText(QStringLiteral("stale repeat"));
        shuffle->setText(QStringLiteral("stale shuffle"));
        modeMenu.retranslate();
        QCOMPARE(stop->text(), QStringLiteral("播放完当前停止"));
        QCOMPARE(repeatAll->text(), QStringLiteral("列表循环"));
        QCOMPARE(shuffle->text(), QStringLiteral("随机播放"));
        QVERIFY(stop->isChecked());
        QVERIFY(modeMenu.menu()->title().contains(QStringLiteral("播放模式")));
    }
};

QTEST_MAIN(PlaylistModeMenuTest)
#include "playlist_mode_menu_test.moc"
