#include "playback/mpv/mpvcontroller.h"
#include "playback/playlist/playlistcontroller.h"
#include "playback/playlist/playlistmodel.h"

#include <QFile>
#include <QPersistentModelIndex>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class PlaylistShuffleTest final : public QObject {
    Q_OBJECT
  private slots:
    void visitsEachEntryOnceWithoutChangingDisplayOrder() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.restoreFiles({media, media, media, media}, 3);
        queue.setProperty("shuffleEnabled", true);
        queue.playAt(3);
        QTRY_COMPARE(loaded.count(), 1);
        QVERIFY(queue.hasNext()); // Even at the last displayed row.
        QVERIFY(!queue.hasPrevious());
        QSet<int> visited{3};
        for (int i = 0; i < 3; ++i) {
            queue.playNext();
            QTRY_COMPARE(loaded.count(), i + 2);
            const auto row = queue.model()->currentIndex();
            QVERIFY(!visited.contains(row));
            visited.insert(row);
        }
        QCOMPARE(visited, QSet<int>({0, 1, 2, 3}));
        QVERIFY(!queue.hasNext());
        QCOMPARE(queue.model()->paths(), QStringList({media, media, media, media}));
    }

    void previousUsesHistoryAcrossMovesAndRemovals() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.restoreFiles({media, media, media, media}, 3);
        queue.setProperty("shuffleEnabled", true);
        queue.playAt(3);
        QTRY_COMPARE(loaded.count(), 1);
        QPersistentModelIndex first(queue.model()->index(3));
        queue.playNext();
        QTRY_COMPARE(loaded.count(), 2);
        QPersistentModelIndex second(queue.model()->index(queue.model()->currentIndex()));
        queue.playNext();
        QTRY_COMPARE(loaded.count(), 3);
        QPersistentModelIndex third(queue.model()->index(queue.model()->currentIndex()));
        queue.moveItem(first.row(), 0);
        queue.playPrevious();
        QTRY_COMPARE(loaded.count(), 4);
        QCOMPARE(queue.model()->currentIndex(), second.row());
        queue.removeAt(first.row());
        QVERIFY(!queue.hasPrevious());
        queue.playNext();
        QTRY_COMPARE(loaded.count(), 5);
        QCOMPARE(queue.model()->currentIndex(), third.row());
    }

    void repeatAllStartsNewCyclesWithoutImmediateRepeat() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.restoreFiles({media, media, media}, 2);
        queue.setProperty("shuffleEnabled", true);
        queue.setRepeatMode(PlaylistController::RepeatMode::All);
        queue.playAt(2);
        QTRY_COMPARE(loaded.count(), 1);
        QSet<int> cycle{2};
        int last = 2;
        // The initial cycle includes the anchor; later cycles include every row.
        for (int i = 1; i <= 8; ++i) {
            if (cycle.size() == 3)
                cycle.clear();
            QVERIFY(queue.hasNext());
            queue.playNext();
            QTRY_COMPARE(loaded.count(), i + 1);
            const int row = queue.model()->currentIndex();
            QVERIFY(row != last);
            QVERIFY(!cycle.contains(row));
            cycle.insert(row);
            last = row;
        }
    }

    void togglingShuffleKeepsPauseAndPosition() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.restoreFiles({media, media}, 1);
        queue.playAt(1);
        QTRY_COMPARE(loaded.count(), 1);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused && player.state().seekable);
        player.seekAbsolute(1.5);
        QTRY_VERIFY(qAbs(player.state().position - 1.5) < 0.08);
        queue.setProperty("shuffleEnabled", true);
        QVERIFY(queue.hasNext() && !queue.hasPrevious());
        queue.setProperty("shuffleEnabled", false);
        QVERIFY(!queue.hasNext() && queue.hasPrevious());
        QVERIFY(player.state().paused);
        QVERIFY(qAbs(player.state().position - 1.5) < 0.08);
        QCOMPARE(loaded.count(), 1);
    }

    void appendedEntriesJoinCycleAndRemovingCurrentStaysIdle() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.setProperty("shuffleEnabled", true);
        queue.addFiles({media});
        QTRY_COMPARE(loaded.count(), 1);
        QVERIFY(!queue.hasNext());
        queue.addFiles({media});
        QVERIFY(queue.hasNext());
        queue.playNext();
        QTRY_COMPARE(loaded.count(), 2);
        QCOMPARE(queue.model()->currentIndex(), 1);
        queue.removeAt(1);
        QTRY_VERIFY(!player.state().active);
        QVERIFY(!queue.hasPrevious() && !queue.hasNext());
        QCOMPARE(loaded.count(), 2);
        queue.playOrPause();
        QTRY_COMPARE(loaded.count(), 3);
        queue.clear();
        QVERIFY(!queue.hasNext() && !queue.hasPrevious());
        queue.addFiles({media, media});
        QTRY_COMPARE(loaded.count(), 4);
        QVERIFY(queue.hasNext() && !queue.hasPrevious());
    }

    void eofTraversesRemainingEntriesThenFinishes() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.restoreFiles({media, media}, 1);
        queue.setProperty("shuffleEnabled", true);
        queue.playAt(1);
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(player.state().seekable);
        player.seekAbsolute(3.8);
        QTRY_COMPARE(loaded.count(), 2);
        QCOMPARE(queue.model()->currentIndex(), 0);
        QTRY_VERIFY(player.state().seekable);
        player.seekAbsolute(3.8);
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(!player.state().active);
    }

    void repeatOneKeepsNativeLoopAndManualShuffle() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.restoreFiles({media, media}, 1);
        queue.setProperty("shuffleEnabled", true);
        queue.setRepeatMode(PlaylistController::RepeatMode::One);
        queue.playAt(1);
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(player.state().seekable);
        player.seekAbsolute(3.8);
        QTRY_VERIFY(player.state().position < 1.0);
        QCOMPARE(loaded.count(), 1);
        queue.playNext();
        QTRY_COMPARE(loaded.count(), 2);
        QCOMPARE(queue.model()->currentIndex(), 0);
        queue.playPrevious();
        QTRY_COMPARE(loaded.count(), 3);
        QCOMPARE(queue.model()->currentIndex(), 1);
    }

    void allFailuresAreBounded_data() {
        QTest::addColumn<bool>("missing");
        QTest::newRow("missing-files") << true;
        QTest::newRow("undecodable-files") << false;
    }

    void allFailuresAreBounded() {
        QFETCH(bool, missing);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto first = temporary.filePath("first.mkv");
        const auto second = temporary.filePath("second.mkv");
        if (!missing) {
            for (const auto &path : {first, second}) {
                QFile file(path);
                QVERIFY(file.open(QIODevice::WriteOnly));
                QCOMPARE(file.write("invalid media"), 13);
            }
        }
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        QSignalSpy errors(&queue, &PlaylistController::queueError);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        queue.setProperty("shuffleEnabled", true);
        queue.setRepeatMode(PlaylistController::RepeatMode::All);
        queue.addFiles({first, second});
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(errors.count(), 2);
        queue.addFiles({QString::fromUtf8(TEST_MEDIA_PATH)});
        QTRY_COMPARE(loaded.count(), 1);
        QCOMPARE(queue.model()->currentIndex(), 2);
    }

    void errorCallbackCanCancelRandomNavigation() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.restoreFiles({temporary.filePath("missing.mkv"), media}, 1);
        queue.setProperty("shuffleEnabled", true);
        queue.playAt(1);
        QTRY_COMPARE(loaded.count(), 1);
        connect(&queue, &PlaylistController::queueError, &queue, &PlaylistController::clear);
        queue.playNext();
        QCOMPARE(queue.model()->rowCount(), 0);
        QVERIFY(finished.isEmpty());
        QCOMPARE(loaded.count(), 1);
    }

    void failedBackwardHistoryCanStartANewCycle() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        const auto removed = temporary.filePath("removed.mkv");
        QVERIFY(QFile::copy(media, removed));
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&queue, &PlaylistController::queueError);
        queue.restoreFiles({removed, media}, 0);
        queue.setProperty("shuffleEnabled", true);
        queue.playAt(0);
        QTRY_COMPARE(loaded.count(), 1);
        queue.playNext();
        QTRY_COMPARE(loaded.count(), 2);
        QVERIFY(QFile::remove(removed));
        queue.playPrevious();
        QCOMPARE(errors.count(), 1);
        QCOMPARE(queue.model()->currentIndex(), 1);
        QVERIFY(!queue.hasPrevious());
        queue.setRepeatMode(PlaylistController::RepeatMode::All);
        queue.playNext();
        QTRY_COMPARE(loaded.count(), 3);
        QCOMPARE(queue.model()->currentIndex(), 1);
    }

    void asynchronousBackwardFailureAfterRemovingForwardHistoryIsSafe() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        const auto broken = temporary.filePath("broken.mkv");
        QVERIFY(QFile::copy(media, broken));
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        queue.restoreFiles({broken, media}, 0);
        queue.setProperty("shuffleEnabled", true);
        queue.playAt(0);
        QTRY_COMPARE(loaded.count(), 1);
        queue.playNext();
        QTRY_COMPARE(loaded.count(), 2);
        QFile file(broken);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write("invalid media"), 13);
        file.close();
        queue.playPrevious();
        queue.removeAt(1); // Remove the forward entry while the backward load is pending.
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(!queue.hasPrevious());
        QVERIFY(QFile::remove(broken));
        QVERIFY(QFile::copy(media, broken));
        queue.setRepeatMode(PlaylistController::RepeatMode::All);
        queue.playNext();
        QTRY_COMPARE(loaded.count(), 3);
    }

    void exhaustedManualScanNotifiesNavigationWhilePaused() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy navigation(&queue, &PlaylistController::navigationChanged);
        queue.restoreFiles({QString::fromUtf8(TEST_MEDIA_PATH), temporary.filePath("missing.mkv")},
                           0);
        queue.setShuffleEnabled(true);
        queue.playAt(0);
        QTRY_COMPARE(loaded.count(), 1);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QVERIFY(queue.hasNext());
        navigation.clear();
        queue.playNext();
        QVERIFY(!queue.hasNext());
        QVERIFY(!navigation.isEmpty());
        QVERIFY(player.state().paused);
        QCOMPARE(loaded.count(), 1);
    }

    void errorCallbackCanAppendAndMoveTheRemainingCandidate() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&queue, &PlaylistController::queueError);
        queue.restoreFiles({media, temporary.filePath("missing.mkv")}, 0);
        queue.setShuffleEnabled(true);
        queue.playAt(0);
        QTRY_COMPARE(loaded.count(), 1);
        connect(&queue, &PlaylistController::queueError, this, [&] {
            queue.addFiles({media});
            queue.moveItem(2, 0);
        });
        queue.playNext();
        QTRY_COMPARE(loaded.count(), 2);
        QCOMPARE(queue.model()->currentIndex(), 0);
        QCOMPARE(errors.count(), 1);
    }

    void longSessionHistoryRemainsBounded() {
        PlaylistModel model;
        model.addFiles({"a.mkv", "b.mkv"});
        ShuffleOrder order;
        order.reset(model, 0);
        for (int i = 0; i < 4096; ++i)
            QVERIFY(order.next(model, true).isValid());
        int traversed = 0;
        while (order.hasPrevious()) {
            QVERIFY(order.previous().isValid());
            QVERIFY(++traversed < 1024);
        }
        QVERIFY(traversed > 0);
    }

    void asynchronousBackwardFailureKeepsSurvivingForwardHistory() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        const auto broken = temporary.filePath("broken.mkv");
        QVERIFY(QFile::copy(media, broken));
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        queue.restoreFiles({broken, media}, 0);
        queue.setShuffleEnabled(true);
        queue.playAt(0);
        QTRY_COMPARE(loaded.count(), 1);
        queue.playNext();
        QTRY_COMPARE(loaded.count(), 2);
        QFile file(broken);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write("invalid media"), 13);
        file.close();
        queue.playPrevious();
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(!queue.hasPrevious());
        QVERIFY(queue.hasNext());
        queue.playNext();
        QTRY_COMPARE(loaded.count(), 3);
        QCOMPARE(queue.model()->currentIndex(), 1);
    }
};

QTEST_GUILESS_MAIN(PlaylistShuffleTest)
#include "playlist_shuffle_test.moc"
