#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class PlaylistTest final : public QObject {
    Q_OBJECT
  private slots:
    void restoredQueueWaitsForExplicitPlay() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto first = QString::fromUtf8(TEST_MEDIA_PATH);
        const auto second = temporary.filePath(QStringLiteral("second.mkv"));
        QVERIFY(QFile::copy(first, second));
        queue.restoreFiles({first, second, first}, 1);
        QCOMPARE(queue.model()->paths(), QStringList({first, second, first}));
        QCOMPARE(queue.model()->currentIndex(), 1);
        QTest::qWait(100);
        QVERIFY(!player.state().active);
        QVERIFY(loaded.isEmpty());
        queue.playOrPause();
        QTRY_COMPARE(loaded.count(), 1);
        QCOMPARE(player.state().path, second);
        queue.restoreFiles({}, -1);
        QTRY_VERIFY(!player.state().active);
        QVERIFY(queue.model()->paths().isEmpty());
        QCOMPARE(queue.model()->currentIndex(), -1);
    }

    void modelMaintainsOrderAndSelection() {
        PlaylistModel model;
        QCOMPARE(model.addFiles({QStringLiteral("a.mkv"), QStringLiteral("b.mkv")}), 2);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0)).toString(), QStringLiteral("a.mkv"));
        QVERIFY(model.setCurrentIndex(1));
        QCOMPARE(model.currentPath(), QDir::current().absoluteFilePath(QStringLiteral("b.mkv")));
        QVERIFY(model.removeAt(0));
        QCOMPARE(model.currentIndex(), 0);
        QCOMPARE(model.rowCount(), 1);
    }

    void modelIgnoresEmptyPaths() {
        PlaylistModel model;
        QCOMPARE(model.addFiles({QString{}, QStringLiteral("a.mkv"), QString{}}), 1);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0)).toString(), QStringLiteral("a.mkv"));
    }

    void previousSkipsMissingItemsInReverse() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.addFiles({media, QStringLiteral("/not/a/file.mkv"), media});
        QTRY_COMPARE(loaded.count(), 1);
        queue.playAt(2);
        QTRY_COMPARE(loaded.count(), 2);
        queue.playPrevious();
        QCOMPARE(queue.model()->currentIndex(), 0);
        QTRY_COMPARE(loaded.count(), 3);
    }

    void invalidSelectionDoesNotReloadCurrentFile() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&queue, &PlaylistController::queueError);
        queue.addFiles({QString::fromUtf8(TEST_MEDIA_PATH)});
        QTRY_COMPARE(loaded.count(), 1);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        queue.playAt(-1);
        queue.playAt(queue.model()->rowCount());
        QVERIFY(errors.isEmpty());
        QCOMPARE(queue.model()->currentIndex(), 0);
        QTest::qWait(100);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(player.state().paused);
    }

    void missingTailStillFinishesQueue() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        queue.addFiles({QString::fromUtf8(TEST_MEDIA_PATH), QStringLiteral("/not/a/file.mkv")});
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.seekAbsolute(3.5);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
        QVERIFY(!player.state().active);
    }

    void appendAfterEofStartsNewItem() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.addFiles({media});
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.seekAbsolute(3.5);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
        queue.addFiles({media});
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 2, 3000);
        QCOMPARE(queue.model()->currentIndex(), 1);
        QVERIFY(player.state().active);
    }

    void appendingWhilePausedPreservesCurrentPlayback() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.addFiles({media});
        QTRY_COMPARE(loaded.count(), 1);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        queue.addFiles({media});
        QCOMPARE(queue.model()->rowCount(), 2);
        QVERIFY(queue.hasNext());
        QTest::qWait(100);
        QCOMPARE(loaded.count(), 1);
        QCOMPARE(queue.model()->currentIndex(), 0);
        QVERIFY(player.state().paused);
    }

    void playRestartsStoppedItemAndThenTogglesPause() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        queue.addFiles({QString::fromUtf8(TEST_MEDIA_PATH)});
        QTRY_COMPARE(loaded.count(), 1);
        player.stop();
        QTRY_VERIFY(!player.state().active);
        queue.playOrPause();
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_VERIFY(player.state().active);
        queue.playOrPause();
        QTRY_VERIFY(player.state().paused);
        queue.playOrPause();
        QTRY_VERIFY(!player.state().paused);
        QCOMPARE(loaded.count(), 2);
    }

    void controllerStartsFirstAndAdvancesAtEof() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        const auto first = QString::fromUtf8(TEST_MEDIA_PATH);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto second = temporary.filePath(QStringLiteral("second.mkv"));
        QVERIFY(QFile::copy(first, second));
        queue.addFiles({first, second});
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 5000);
        QCOMPARE(queue.model()->currentIndex(), 0);
        QVERIFY(queue.hasNext());
        player.seekAbsolute(3.5);
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 2, 5000);
        QCOMPARE(queue.model()->currentIndex(), 1);
    }

    void invalidFirstItemReportsAndSkipsToNextFile() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy errors(&queue, &PlaylistController::queueError);
        queue.addFiles({QStringLiteral("/not/a/file.mkv"), QString::fromUtf8(TEST_MEDIA_PATH)});
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 5000);
        QCOMPARE(queue.model()->currentIndex(), 1);
        QVERIFY(!errors.isEmpty());
    }
};

QTEST_GUILESS_MAIN(PlaylistTest)
#include "playlist_test.moc"
