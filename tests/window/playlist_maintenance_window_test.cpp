#include "ui/window/mainwindow.h"

#include <QAction>
#include <QMenu>
#include <QtTest>
#include <array>

class PlaylistMaintenanceWindowTest final : public QObject {
    Q_OBJECT
  private slots:
    void playlistMenuContainsSortSubmenuAndCleanupAction() {
        MainWindow window(true);
        auto *playlistMenu = window.findChild<QMenu *>(QStringLiteral("playlistMenu"));
        auto *sortMenu = window.findChild<QMenu *>(QStringLiteral("playlistSortMenu"));
        auto *cleanup = window.findChild<QAction *>(QStringLiteral("removeMissingFilesAction"));
        QVERIFY(playlistMenu);
        QVERIFY(sortMenu);
        QVERIFY(cleanup);
        QCOMPARE(sortMenu->title(), QStringLiteral("排序"));
        QVERIFY(playlistMenu->actions().contains(sortMenu->menuAction()));
        QVERIFY(playlistMenu->actions().contains(cleanup));
        QCOMPARE(cleanup->text(), QStringLiteral("移除缺失文件…"));

        const auto sortActionNames = std::array{
            "sortByTitleAscendingAction",
            "sortByTitleDescendingAction",
            "sortByPathAscendingAction",
            "sortByPathDescendingAction",
        };
        for (const auto *name : sortActionNames) {
            auto *action = window.findChild<QAction *>(QString::fromLatin1(name));
            QVERIFY(action);
            QVERIFY(sortMenu->actions().contains(action));
        }
    }
};

QTEST_MAIN(PlaylistMaintenanceWindowTest)
#include "playlist_maintenance_window_test.moc"
