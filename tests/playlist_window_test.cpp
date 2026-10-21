#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"
#include "ui/mainwindow.h"
#include "ui/playlistpane.h"

#include <QAction>
#include <QListView>
#include <QMenu>
#include <QtTest>

class PlaylistWindowTest final : public QObject {
    Q_OBJECT
  private slots:
    void editSelectionIsIndependentFromPlayback() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QAction append, previous, next, clear;
        PlaylistPane pane(queue, &append, &previous, &next, &clear);
        queue.restoreFiles({"a.mkv", "b.mkv", "c.mkv"}, 1);
        auto *view = pane.view();
        QCOMPARE(view->currentIndex().row(), 1);
        view->setCurrentIndex(queue.model()->index(0));
        queue.moveItem(1, 2);
        QCOMPARE(view->currentIndex().row(), 0);
        QCOMPARE(queue.model()->currentIndex(), 2);
        auto *down = pane.findChild<QAction *>(QStringLiteral("movePlaylistItemDownAction"));
        auto *up = pane.findChild<QAction *>(QStringLiteral("movePlaylistItemUpAction"));
        auto *remove = pane.findChild<QAction *>(QStringLiteral("removePlaylistItemAction"));
        QVERIFY(down && up && remove);
        QVERIFY(down->isEnabled() && !up->isEnabled() && remove->isEnabled());
        QCOMPARE(up->shortcutContext(), Qt::WidgetShortcut);
        QCOMPARE(up->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_Up));
        QCOMPARE(remove->shortcutContext(), Qt::WidgetShortcut);
        QCOMPARE(remove->shortcut(), QKeySequence(Qt::Key_Delete));
        QCOMPARE(view->dragDropMode(), QAbstractItemView::InternalMove);
        down->trigger();
        QCOMPARE(view->currentIndex().row(), 1);
        QCOMPARE(queue.model()->data(view->currentIndex()).toString(), QStringLiteral("a.mkv"));
        QCOMPARE(queue.model()->currentIndex(), 2);
        remove->trigger();
        QCOMPARE(queue.model()->rowCount(), 2);
        QCOMPARE(queue.model()->currentIndex(), 1);
        QVERIFY(!player.state().active);
        queue.clear();
        QVERIFY(!remove->isEnabled() && !up->isEnabled() && !down->isEnabled());
    }

    void progressDoesNotStealSelectionAndMenusShareActions() {
        MainWindow window(true);
        window.playlist()->restoreFiles({"a.mkv", "b.mkv", "c.mkv"}, 1);
        auto *pane = window.findChild<PlaylistPane *>();
        auto *menu = window.findChild<QMenu *>(QStringLiteral("playlistMenu"));
        QVERIFY(pane && menu);
        pane->view()->setCurrentIndex(window.playlist()->model()->index(0));
        QVERIFY(
            QMetaObject::invokeMethod(window.controller(), "stateChanged", Qt::DirectConnection));
        QCOMPARE(pane->view()->currentIndex().row(), 0);
        auto *remove = pane->findChild<QAction *>(QStringLiteral("removePlaylistItemAction"));
        QVERIFY(remove && menu->actions().contains(remove));
        QVERIFY(pane->view()->actions().contains(remove));
        QCOMPARE(pane->view()->contextMenuPolicy(), Qt::ActionsContextMenu);
    }
};

QTEST_MAIN(PlaylistWindowTest)
#include "playlist_window_test.moc"
