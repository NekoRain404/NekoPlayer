#include "input/folderscanjob.h"
#include "playback/mpv/mpvcontroller.h"
#include "playback/playlist/playlistcontroller.h"
#include "playback/playlist/playlistmodel.h"
#include "playback/render/videowidget.h"
#include "session/sessionstore.h"
#include "ui/menus/foldermenu.h"
#include "ui/window/mainwindow.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QMenuBar>
#include <QProgressDialog>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtTest>

namespace {
bool touch(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly);
}

// Finish actual filesystem work before delivering queued completion events.
// No elapsed-time assumption or large directory is needed for cancellation tests.
bool settleWorkers() {
    if (!QThreadPool::globalInstance()->waitForDone(5000))
        return false;
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents();
    return true;
}
} // namespace

class FolderWindowTest final : public QObject {
    Q_OBJECT
  private slots:
    void actionsShareFilePlaylistAndContextSubmenu() {
        MainWindow window(true);
        auto *folders = window.findChild<FolderMenu *>();
        auto *open = window.findChild<QAction *>(QStringLiteral("openFolderAction"));
        auto *append = window.findChild<QAction *>(QStringLiteral("appendFolderAction"));
        auto *recursive = window.findChild<QAction *>(QStringLiteral("recursiveFolderAction"));
        auto *cancel = window.findChild<QAction *>(QStringLiteral("cancelFolderScanAction"));
        auto *playlist = window.findChild<QMenu *>(QStringLiteral("playlistMenu"));
        // The progress window is created on the first scan; an empty scanning
        // dialog must not appear while the player is idle.
        auto *progress = window.findChild<QProgressDialog *>(QStringLiteral("folderScanProgress"));
        QVERIFY(folders && open && append && recursive && cancel && playlist);
        QVERIFY(!progress);
        auto *submenu = folders->menu();
        QVERIFY(window.actions().contains(open) && window.actions().contains(append));
        QVERIFY(submenu->actions().contains(open) && submenu->actions().contains(append));
        QVERIFY(submenu->actions().contains(recursive) && submenu->actions().contains(cancel));
        QCOMPARE(open->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_D));
        QCOMPARE(append->shortcut(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
        QVERIFY(!open->autoRepeat() && !append->autoRepeat());
        QVERIFY(recursive->isCheckable() && !recursive->isChecked());
        QVERIFY(!cancel->isEnabled());
        auto *file = window.menuBar()->actions().constFirst()->menu();
        QVERIFY(file && file->actions().contains(submenu->menuAction()));
        QVERIFY(playlist->actions().contains(submenu->menuAction()));
        int hosts = 0;
        for (auto *menu : window.findChildren<QMenu *>()) {
            if (menu->actions().contains(submenu->menuAction()))
                ++hosts;
        }
        QCOMPARE(hosts, 3); // File, playlist, and the video context menu share one submenu.
    }

    void successfulScanUpdatesPendingQueue_data() {
        QTest::addColumn<bool>("append");
        QTest::addColumn<bool>("recursive");
        QTest::newRow("open-direct") << false << false;
        QTest::newRow("append-direct") << true << false;
        QTest::newRow("open-recursive") << false << true;
        QTest::newRow("append-recursive") << true << true;
    }

    void successfulScanUpdatesPendingQueue() {
        QFETCH(bool, append);
        QFETCH(bool, recursive);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QVERIFY(QDir(temporary.path()).mkpath(QStringLiteral("media/nested")));
        const auto prior = temporary.filePath(QStringLiteral("prior.mkv"));
        const auto second = temporary.filePath(QStringLiteral("media/episode2.mkv"));
        const auto tenth = temporary.filePath(QStringLiteral("media/episode10.mkv"));
        const auto nested = temporary.filePath(QStringLiteral("media/nested/episode1.mkv"));
        for (const auto &path : {prior, tenth, second, nested})
            QVERIFY(touch(path));
        const auto sessionPath = temporary.filePath(QStringLiteral("session.ini"));
        MainWindow window(true, sessionPath);
        window.openFiles({prior, prior});
        QVERIFY(!window.videoWidget()->isReady());
        auto *folders = window.findChild<FolderMenu *>();
        QVERIFY(folders);
        auto *job = folders->findChild<FolderScanJob *>();
        auto *cancel = folders->findChild<QAction *>(QStringLiteral("cancelFolderScanAction"));
        QVERIFY(job && cancel);
        QSignalSpy ready(folders, &FolderMenu::filesReady);
        QSignalSpy errors(&window, &MainWindow::userError);
        window.openFolder(temporary.filePath(QStringLiteral("media")), append, recursive);
        QVERIFY(job->isScanning() && cancel->isEnabled());
        auto *progress = window.findChild<QProgressDialog *>(QStringLiteral("folderScanProgress"));
        // The test window itself is not mapped, so check the explicit show
        // state rather than QWidget::isVisible().
        QVERIFY(progress && !progress->isHidden());
        QCOMPARE(progress->windowModality(), Qt::NonModal);
        QTRY_COMPARE(ready.count(), 1);
        QVERIFY(!job->isScanning() && !cancel->isEnabled());
        QVERIFY(errors.isEmpty());
        QVERIFY(!window.videoWidget()->isReady());
        QCOMPARE(window.playlist()->model()->rowCount(), 0);
        QVERIFY(!window.controller()->state().active);
        QVERIFY(window.close());
        const auto stored = SessionStore(sessionPath).load();
        QVERIFY2(stored.error.isEmpty(), qPrintable(stored.error));
        QStringList expected;
        if (append)
            expected = {prior, prior};
        expected.append({second, tenth});
        if (recursive)
            expected.append(nested);
        QCOMPARE(stored.state.paths, expected);
        QCOMPARE(stored.state.window.mediaDirectory, temporary.filePath(QStringLiteral("media")));
    }

    void unsuccessfulScanPreservesPendingQueue_data() {
        QTest::addColumn<QString>("outcome");
        QTest::newRow("empty") << QStringLiteral("empty");
        QTest::newRow("missing") << QStringLiteral("missing");
        QTest::newRow("cancel") << QStringLiteral("cancel");
    }

    void unsuccessfulScanPreservesPendingQueue() {
        QFETCH(QString, outcome);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QVERIFY(QDir(temporary.path()).mkdir(QStringLiteral("scan")));
        const auto prior = temporary.filePath(QStringLiteral("prior.mkv"));
        QVERIFY(touch(prior));
        if (outcome == QStringLiteral("cancel"))
            QVERIFY(touch(temporary.filePath(QStringLiteral("scan/replacement.mkv"))));
        const auto sessionPath = temporary.filePath(QStringLiteral("session.ini"));
        MainWindow window(true, sessionPath);
        window.openFile(prior);
        auto *folders = window.findChild<FolderMenu *>();
        QVERIFY(folders);
        auto *job = folders->findChild<FolderScanJob *>();
        auto *cancel = folders->findChild<QAction *>(QStringLiteral("cancelFolderScanAction"));
        QVERIFY(job && cancel);
        QSignalSpy completed(job, &FolderScanJob::completed);
        QSignalSpy ready(folders, &FolderMenu::filesReady);
        QSignalSpy errors(&window, &MainWindow::userError);
        window.openFolder(temporary.filePath(outcome == QStringLiteral("missing")
                                                 ? QStringLiteral("missing")
                                                 : QStringLiteral("scan")));
        if (outcome == QStringLiteral("cancel")) {
            cancel->trigger();
            QVERIFY(!job->isScanning() && !cancel->isEnabled());
            QVERIFY(settleWorkers());
        } else if (outcome == QStringLiteral("missing")) {
            QTRY_COMPARE(errors.count(), 1);
        } else {
            QTRY_COMPARE(completed.count(), 1);
        }
        QVERIFY(!job->isScanning() && !cancel->isEnabled());
        QVERIFY(ready.isEmpty());
        QCOMPARE(errors.count(), outcome == QStringLiteral("missing") ? 1 : 0);
        QVERIFY(window.close());
        const auto stored = SessionStore(sessionPath).load();
        QVERIFY2(stored.error.isEmpty(), qPrintable(stored.error));
        QCOMPARE(stored.state.paths, QStringList({prior}));
        QCOMPARE(stored.state.window.mediaDirectory, temporary.path());
    }

    void newerExplicitSourceInvalidatesScan_data() {
        QTest::addColumn<bool>("append");
        QTest::newRow("open") << false;
        QTest::newRow("append") << true;
    }

    void newerExplicitSourceInvalidatesScan() {
        QFETCH(bool, append);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QVERIFY(QDir(temporary.path()).mkdir(QStringLiteral("scan")));
        const auto prior = temporary.filePath(QStringLiteral("prior.mkv"));
        const auto newer = temporary.filePath(QStringLiteral("newer.mkv"));
        QVERIFY(touch(prior) && touch(newer));
        QVERIFY(touch(temporary.filePath(QStringLiteral("scan/stale.mkv"))));
        const auto sessionPath = temporary.filePath(QStringLiteral("session.ini"));
        MainWindow window(true, sessionPath);
        window.openFile(prior);
        auto *folders = window.findChild<FolderMenu *>();
        QVERIFY(folders);
        auto *job = folders->findChild<FolderScanJob *>();
        QVERIFY(job);
        QSignalSpy ready(folders, &FolderMenu::filesReady);
        window.openFolder(temporary.filePath(QStringLiteral("scan")));
        if (append)
            window.appendFiles({newer});
        else
            window.openFile(newer);
        QVERIFY(!job->isScanning());
        QVERIFY(settleWorkers());
        QVERIFY(ready.isEmpty());
        QVERIFY(window.close());
        const auto stored = SessionStore(sessionPath).load();
        QVERIFY2(stored.error.isEmpty(), qPrintable(stored.error));
        QCOMPARE(stored.state.paths, append ? QStringList({prior, newer}) : QStringList({newer}));
    }

    void directQueueEditsCancelScan_data() {
        QTest::addColumn<QString>("edit");
        QTest::newRow("insert") << QStringLiteral("insert");
        QTest::newRow("remove") << QStringLiteral("remove");
        QTest::newRow("move") << QStringLiteral("move");
        QTest::newRow("clear") << QStringLiteral("clear");
    }

    void directQueueEditsCancelScan() {
        QFETCH(QString, edit);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QVERIFY(touch(temporary.filePath(QStringLiteral("stale.mkv"))));
        MainWindow window(true);
        auto *model = window.playlist()->model();
        model->addFiles({QStringLiteral("a.mkv"), QStringLiteral("b.mkv")});
        auto *folders = window.findChild<FolderMenu *>();
        QVERIFY(folders);
        auto *job = folders->findChild<FolderScanJob *>();
        QVERIFY(job);
        QSignalSpy ready(folders, &FolderMenu::filesReady);
        window.openFolder(temporary.path());
        if (edit == QStringLiteral("insert"))
            QCOMPARE(model->addFiles({QStringLiteral("c.mkv")}), 1);
        else if (edit == QStringLiteral("remove"))
            QVERIFY(model->removeAt(0));
        else if (edit == QStringLiteral("move"))
            QVERIFY(model->moveRow({}, 0, {}, 2));
        else
            model->clear();
        QVERIFY(!job->isScanning());
        const auto expected = model->paths();
        QVERIFY(settleWorkers());
        QVERIFY(ready.isEmpty());
        QCOMPARE(model->paths(), expected);
    }

    void newerFolderScanKeepsItsOwnAppendMode() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QVERIFY(QDir(temporary.path()).mkdir(QStringLiteral("first")));
        QVERIFY(QDir(temporary.path()).mkdir(QStringLiteral("second")));
        const auto prior = temporary.filePath(QStringLiteral("prior.mkv"));
        const auto newer = temporary.filePath(QStringLiteral("second/newer.mkv"));
        QVERIFY(touch(prior) && touch(newer));
        QVERIFY(touch(temporary.filePath(QStringLiteral("first/stale.mkv"))));
        const auto sessionPath = temporary.filePath(QStringLiteral("session.ini"));
        MainWindow window(true, sessionPath);
        window.openFile(prior);
        auto *folders = window.findChild<FolderMenu *>();
        QVERIFY(folders);
        QSignalSpy ready(folders, &FolderMenu::filesReady);
        QSignalSpy errors(&window, &MainWindow::userError);
        window.openFolder(temporary.filePath(QStringLiteral("first")), false);
        window.openFolder(temporary.filePath(QStringLiteral("second")), true);
        QTRY_COMPARE(ready.count(), 1);
        QVERIFY(settleWorkers());
        QCOMPARE(ready.count(), 1);
        QVERIFY(ready.constFirst().at(1).toBool());
        QVERIFY(errors.isEmpty());
        QVERIFY(window.close());
        const auto stored = SessionStore(sessionPath).load();
        QVERIFY2(stored.error.isEmpty(), qPrintable(stored.error));
        QCOMPARE(stored.state.paths, QStringList({prior, newer}));
    }

    void closeCancelsScanBeforeSavingPendingQueue() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QVERIFY(QDir(temporary.path()).mkdir(QStringLiteral("scan")));
        const auto prior = temporary.filePath(QStringLiteral("prior.mkv"));
        QVERIFY(touch(prior));
        QVERIFY(touch(temporary.filePath(QStringLiteral("scan/stale.mkv"))));
        const auto sessionPath = temporary.filePath(QStringLiteral("session.ini"));
        MainWindow window(true, sessionPath);
        window.openFile(prior);
        auto *folders = window.findChild<FolderMenu *>();
        QVERIFY(folders);
        auto *job = folders->findChild<FolderScanJob *>();
        QVERIFY(job);
        QSignalSpy ready(folders, &FolderMenu::filesReady);
        QSignalSpy errors(&window, &MainWindow::userError);
        window.openFolder(temporary.filePath(QStringLiteral("scan")));
        QVERIFY(job->isScanning());
        QVERIFY(window.close());
        QVERIFY(!job->isScanning());
        QVERIFY(settleWorkers());
        QVERIFY(ready.isEmpty() && errors.isEmpty());
        const auto stored = SessionStore(sessionPath).load();
        QVERIFY2(stored.error.isEmpty(), qPrintable(stored.error));
        QCOMPARE(stored.state.paths, QStringList({prior}));
        QVERIFY(!window.controller()->state().active);
    }
};

QTEST_MAIN(FolderWindowTest)
#include "folder_window_test.moc"
