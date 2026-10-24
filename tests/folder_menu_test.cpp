#include "input/folderscanjob.h"
#include "ui/foldermenu.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QWidget>
#include <QtTest>

class FolderMenuTest final : public QObject {
    Q_OBJECT
  private slots:
    void disabledScanningBlocksActionsAndRequests() {
        QWidget window;
        FolderMenu folders(&window, [] { return QString{}; });
        const auto open = window.findChild<QAction *>(QStringLiteral("openFolderAction"));
        const auto append = window.findChild<QAction *>(QStringLiteral("appendFolderAction"));
        const auto recursive = window.findChild<QAction *>(QStringLiteral("recursiveFolderAction"));
        QVERIFY(open && append && recursive);
        QVERIFY(open->isEnabled() && append->isEnabled());

        QSignalSpy feedback(&folders, &FolderMenu::feedback);
        QSignalSpy files(&folders, &FolderMenu::filesReady);
        folders.setMediaLibraryPreferences({false, true});
        QVERIFY(!open->isEnabled() && !append->isEnabled() && !recursive->isEnabled());
        QVERIFY(recursive->isChecked());
        folders.start(QStringLiteral("/tmp"), false, true);
        QVERIFY(!folders.findChild<FolderScanJob *>()->isScanning());
        QVERIFY(files.isEmpty());
        QCOMPARE(feedback.count(), 1);
    }

    void enabledRecursivePreferenceIsUsedForManualScans() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QDir(directory.path()).mkpath(QStringLiteral("nested")));
        QFile media(directory.filePath(QStringLiteral("nested/episode.mkv")));
        QVERIFY(media.open(QIODevice::WriteOnly));

        QWidget window;
        FolderMenu folders(&window, [] { return QString{}; });
        folders.setMediaLibraryPreferences({true, true});
        QVERIFY(folders.recursiveEnabled());
        QSignalSpy files(&folders, &FolderMenu::filesReady);
        folders.start(directory.path(), false, folders.recursiveEnabled());
        QTRY_COMPARE(files.count(), 1);
        QCOMPARE(files.constFirst().at(0).toStringList(),
                 QStringList({directory.filePath(QStringLiteral("nested/episode.mkv"))}));
    }
};

QTEST_MAIN(FolderMenuTest)
#include "folder_menu_test.moc"
