#include "playback/mpv/mpvcontroller.h"
#include "playback/playlist/playlistcontroller.h"
#include "playback/playlist/playlistmodel.h"
#include "ui/menus/playlistmaintenancemenu.h"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QSignalSpy>
#include <QWidget>
#include <QtTest>
#include <array>

namespace {
QString literal(const char *text) { return QString::fromLatin1(text); }
QStringList sortPaths() {
    return {literal("/a/zeta.mkv"), literal("/b/alpha.mkv"), literal("/c/Movie 2.mkv"),
            literal("/d/movie 10.mkv")};
}
QStringList pathOrder() {
    return {literal("/a/zeta.mkv"), literal("/b/alpha.mkv"), literal("/c/Movie 2.mkv"),
            literal("/d/movie 10.mkv")};
}
QStringList titleAscendingOrder() {
    return {literal("/b/alpha.mkv"), literal("/c/Movie 2.mkv"), literal("/d/movie 10.mkv"),
            literal("/a/zeta.mkv")};
}
QStringList titleDescendingOrder() {
    return {literal("/a/zeta.mkv"), literal("/d/movie 10.mkv"), literal("/c/Movie 2.mkv"),
            literal("/b/alpha.mkv")};
}
QStringList reverse(const QStringList &paths) {
    QStringList reversed;
    reversed.reserve(paths.size());
    for (auto iterator = paths.crbegin(); iterator != paths.crend(); ++iterator)
        reversed.append(*iterator);
    return reversed;
}
} // namespace

class PlaylistMaintenanceMenuTest final : public QObject {
    Q_OBJECT
  private slots:
    void sortActionsHaveStableNamesTextsAndOrder() {
        MpvController player(true, true);
        PlaylistController playlist(player);
        QWidget dialogParent;
        PlaylistMaintenanceMenu maintenance(playlist, &dialogParent);
        playlist.restoreFiles(sortPaths(), 0);

        auto *sortMenu = maintenance.sortMenu();
        QVERIFY(sortMenu);
        QCOMPARE(sortMenu->title(), QStringLiteral("排序"));
        const auto names = std::array{
            "sortByTitleAscendingAction",
            "sortByTitleDescendingAction",
            "sortByPathAscendingAction",
            "sortByPathDescendingAction",
        };
        const auto labels = std::array{
            QStringLiteral("标题升序"),
            QStringLiteral("标题降序"),
            QStringLiteral("路径升序"),
            QStringLiteral("路径降序"),
        };
        QCOMPARE(sortMenu->actions().size(), static_cast<qsizetype>(names.size()));
        for (size_t index = 0; index < names.size(); ++index) {
            auto *action = sortMenu->findChild<QAction *>(literal(names[index]));
            QVERIFY(action);
            QCOMPARE(action->text(), labels[index]);
            QVERIFY(action->shortcut().isEmpty());
            QVERIFY(!action->isCheckable());
            QVERIFY(sortMenu->actions().contains(action));
        }

        for (auto *action : sortMenu->actions())
            action->setText(QStringLiteral("stale"));
        sortMenu->setTitle(QStringLiteral("stale"));
        maintenance.retranslate();
        QCOMPARE(sortMenu->title(), QStringLiteral("排序"));
        for (size_t index = 0; index < names.size(); ++index)
            QCOMPARE(sortMenu->findChild<QAction *>(literal(names[index]))->text(), labels[index]);
    }

    void sortActionsReorderModelAndKeepCurrentEntry() {
        MpvController player(true, true);
        PlaylistController playlist(player);
        QWidget dialogParent;
        PlaylistMaintenanceMenu maintenance(playlist, &dialogParent);
        playlist.restoreFiles(sortPaths(), 0);
        auto *model = playlist.model();
        auto *sortMenu = maintenance.sortMenu();

        const auto checkSort = [model, sortMenu](const char *name, const QStringList &expected,
                                                 int expectedCurrentRow) {
            auto *action = sortMenu->findChild<QAction *>(literal(name));
            if (!action)
                return false;
            action->trigger();
            return model->paths() == expected && model->currentIndex() == expectedCurrentRow &&
                   model->currentPath() == literal("/a/zeta.mkv");
        };

        QVERIFY(checkSort("sortByTitleAscendingAction", titleAscendingOrder(), 3));
        QVERIFY(checkSort("sortByTitleDescendingAction", titleDescendingOrder(), 0));
        QVERIFY(checkSort("sortByPathAscendingAction", pathOrder(), 0));
        QVERIFY(checkSort("sortByPathDescendingAction", reverse(pathOrder()), 3));
    }

    void missingCleanupReportsNothingToRemoveWithoutDialog() {
        MpvController player(true, true);
        PlaylistController playlist(player);
        QWidget dialogParent;
        PlaylistMaintenanceMenu maintenance(playlist, &dialogParent);
        playlist.restoreFiles({literal("https://example.com/video.mp4")}, 0);
        QVERIFY(playlist.missingFileRows().isEmpty());

        auto *cleanup = maintenance.removeMissingAction();
        QVERIFY(cleanup);
        QCOMPARE(cleanup->objectName(), QStringLiteral("removeMissingFilesAction"));
        QCOMPARE(cleanup->text(), QStringLiteral("移除缺失文件…"));
        QVERIFY(cleanup->shortcut().isEmpty());

        QSignalSpy feedback(&maintenance, &PlaylistMaintenanceMenu::feedback);
        cleanup->trigger();
        QCOMPARE(feedback.count(), 1);
        QCOMPARE(feedback.at(0).at(0).toString(), QStringLiteral("没有缺失文件"));
        QVERIFY(!QApplication::activeModalWidget());
        QCOMPARE(playlist.model()->rowCount(), 1);
        QCOMPARE(playlist.model()->currentPath(), literal("https://example.com/video.mp4"));
    }
};

QTEST_MAIN(PlaylistMaintenanceMenuTest)
#include "playlist_maintenance_menu_test.moc"
