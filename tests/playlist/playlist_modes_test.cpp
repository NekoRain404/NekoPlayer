#include "playback/mpv/mpvcontroller.h"
#include "playback/playlist/playlistcontroller.h"
#include "playback/playlist/playlistmodel.h"

#include <QAbstractItemModelTester>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class PlaylistModesTest final : public QObject {
    Q_OBJECT
  private slots:
    void emptyFrameContainersDoNotRetryForever() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto emptyWave = temporary.filePath(QStringLiteral("empty.wav"));
        QFile file(emptyWave);
        QVERIFY(file.open(QIODevice::WriteOnly));
        // Valid PCM WAV header, one audio track, zero samples.
        const auto header = QByteArray::fromHex(
            "524946462400000057415645666d7420100000000100010080bb00000077010002001000"
            "6461746100000000");
        QCOMPARE(file.write(header), header.size());
        file.close();
        MpvController player(true, true);
        PlaylistController queue(player);
        queue.setRepeatMode(PlaylistController::RepeatMode::All);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&queue, &PlaylistController::queueError);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        queue.addFiles({emptyWave, emptyWave});
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() > 0 || errors.count() > 2, 3000);
        QVERIFY(!loaded.isEmpty()); // Exercise failure after FILE_LOADED, not header rejection.
        QCOMPARE(errors.count(), 2);
        QCOMPARE(finished.count(), 1);
        QVERIFY(!player.state().active);
    }

    void repeatOneHandlesEofAlreadyQueuedDuringModeChange() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.addFiles({media, media});
        QTRY_COMPARE(loaded.count(), 1);
        queue.setRepeatMode(PlaylistController::RepeatMode::One);
        // Model EOF delivery after changing mode, before the async loop setter takes effect.
        QVERIFY(QMetaObject::invokeMethod(&player, "playbackFinished", Qt::DirectConnection));
        QTRY_COMPARE(loaded.count(), 2);
        QCOMPARE(queue.model()->currentIndex(), 0);
        QVERIFY(finished.isEmpty());
    }

    void repeatOneReplaysOnlyAtEof() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        QVERIFY(queue.setProperty("repeatMode", 1));
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.addFiles({media, media});
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.seekAbsolute(3.8);
        QTRY_VERIFY(player.state().position > 3.0);
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active && player.state().position < 1.0, 3000);
        QCOMPARE(loaded.count(), 1); // Native looping preserves decoder and track resources.
        QCOMPARE(queue.model()->currentIndex(), 0);
        QVERIFY(finished.isEmpty());
        queue.playNext();
        QTRY_COMPARE(loaded.count(), 2);
        QCOMPARE(queue.model()->currentIndex(), 1);
        QVERIFY(!queue.hasNext());
        queue.playPrevious();
        QTRY_COMPARE(loaded.count(), 3);
        QCOMPARE(queue.model()->currentIndex(), 0);
        player.stop();
        QTRY_VERIFY(!player.state().active);
        QTest::qWait(150);
        QCOMPARE(loaded.count(), 3);
        QVERIFY(finished.isEmpty());
    }

    void repeatAllWrapsInBothDirectionsAndAtEof() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        QVERIFY(queue.setProperty("repeatMode", 2));
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.addFiles({media, QStringLiteral("/missing/neko.mkv"), media});
        QTRY_COMPARE(loaded.count(), 1);
        QVERIFY(queue.hasPrevious());
        queue.playPrevious();
        QTRY_COMPARE(loaded.count(), 2);
        QCOMPARE(queue.model()->currentIndex(), 2);
        QVERIFY(queue.hasNext());
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.seekAbsolute(3.8);
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 3, 3000);
        QCOMPARE(queue.model()->currentIndex(), 0);
        queue.playNext();
        QTRY_COMPARE(loaded.count(), 4);
        QCOMPARE(queue.model()->currentIndex(), 2);
        queue.playNext();
        QTRY_COMPARE(loaded.count(), 5);
        QCOMPARE(queue.model()->currentIndex(), 0);
        QVERIFY(finished.isEmpty());
    }

    void repeatAllSupportsOneItemAndModeChangeWithoutReload() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        queue.addFiles({QString::fromUtf8(TEST_MEDIA_PATH)});
        QTRY_COMPARE(loaded.count(), 1);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QVERIFY(queue.setProperty("repeatMode", 2));
        QVERIFY(queue.hasNext() && queue.hasPrevious());
        QTest::qWait(100);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(player.state().paused);
        player.setPaused(false);
        QTRY_VERIFY(player.state().active && player.state().seekable && !player.state().paused);
        player.seekAbsolute(3.8);
        QTRY_VERIFY(player.state().position > 3.0);
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active && player.state().position < 1.0, 3000);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(queue.setProperty("repeatMode", 0));
        QVERIFY(!queue.hasNext() && !queue.hasPrevious());
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.seekAbsolute(3.8);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
        QCOMPARE(loaded.count(), 1);
    }

    void stopAfterCurrentEofFinishesQueueAndDisarms() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        QSignalSpy armed(&queue, &PlaylistController::stopAfterCurrentChanged);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.addFiles({media, media});
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(player.state().active && player.state().seekable);
        queue.setStopAfterCurrent(true);
        QVERIFY(queue.stopAfterCurrent());
        QCOMPARE(armed.count(), 1);
        QCOMPARE(armed.at(0).at(0).toBool(), true);
        player.seekAbsolute(3.8);
        QTRY_VERIFY(player.state().position > 3.0);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
        QCOMPARE(queue.model()->currentIndex(), 0);
        QVERIFY(!queue.stopAfterCurrent());
        QCOMPARE(armed.count(), 2);
        QCOMPARE(armed.at(1).at(0).toBool(), false);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(!player.state().active);
    }

    void stopAfterCurrentOverridesRepeatOneLoop() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        queue.setRepeatMode(PlaylistController::RepeatMode::One);
        queue.addFiles({QString::fromUtf8(TEST_MEDIA_PATH)});
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(player.state().active && player.state().seekable);
        queue.setStopAfterCurrent(true);
        player.seekAbsolute(3.8);
        QTRY_VERIFY(player.state().position > 3.0);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
        QCOMPARE(loaded.count(), 1);
        QCOMPARE(queue.model()->currentIndex(), 0);
        QVERIFY(!queue.stopAfterCurrent());
        QVERIFY(!player.state().active);

        // Disarming restores the repeat-one native loop for the next open.
        queue.playAt(0);
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.seekAbsolute(3.8);
        QTRY_VERIFY(player.state().position > 3.0);
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active && player.state().position < 1.0, 3000);
        QCOMPARE(loaded.count(), 2);
        QCOMPARE(finished.count(), 1);
    }

    void stopAfterCurrentOverridesRepeatAllSingleItemLoop() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        queue.setRepeatMode(PlaylistController::RepeatMode::All);
        queue.addFiles({QString::fromUtf8(TEST_MEDIA_PATH)});
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(player.state().active && player.state().seekable);
        queue.setStopAfterCurrent(true);
        player.seekAbsolute(3.8);
        QTRY_VERIFY(player.state().position > 3.0);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
        QCOMPARE(loaded.count(), 1);
        QCOMPARE(queue.model()->currentIndex(), 0);
        QVERIFY(!queue.stopAfterCurrent());
        QVERIFY(!player.state().active);
    }

    void explicitStopAndClearDisarmStopAfterCurrent() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy armed(&queue, &PlaylistController::stopAfterCurrentChanged);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.addFiles({media, media});
        QTRY_COMPARE(loaded.count(), 1);
        queue.setStopAfterCurrent(true);
        QVERIFY(queue.stopAfterCurrent());
        player.stop();
        QTRY_VERIFY(!queue.stopAfterCurrent());
        QCOMPARE(armed.count(), 2);
        QCOMPARE(armed.at(0).at(0).toBool(), true);
        QCOMPARE(armed.at(1).at(0).toBool(), false);
        QTRY_VERIFY(!player.state().active);

        queue.setStopAfterCurrent(true);
        QVERIFY(queue.stopAfterCurrent());
        queue.clear();
        QTRY_VERIFY(!queue.stopAfterCurrent());
        QCOMPARE(armed.count(), 4);
        QCOMPARE(armed.at(2).at(0).toBool(), true);
        QCOMPARE(armed.at(3).at(0).toBool(), false);
        QCOMPARE(queue.model()->rowCount(), 0);
    }

    void failureExhaustionDisarmsStopAfterCurrent() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy errors(&queue, &PlaylistController::queueError);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        QSignalSpy armed(&queue, &PlaylistController::stopAfterCurrentChanged);
        queue.setStopAfterCurrent(true);
        QVERIFY(queue.stopAfterCurrent());
        queue.addFiles(
            {QStringLiteral("/missing/neko-one.mkv"), QStringLiteral("/missing/neko-two.mkv")});
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
        QCOMPARE(errors.count(), 2);
        QVERIFY(!queue.stopAfterCurrent());
        QCOMPARE(armed.count(), 2);
        QCOMPARE(armed.at(0).at(0).toBool(), true);
        QCOMPARE(armed.at(1).at(0).toBool(), false);
    }

    void removingCurrentRowDisarmsStopAfterCurrent() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.addFiles({media, media});
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(player.state().active);
        queue.setStopAfterCurrent(true);
        QVERIFY(queue.stopAfterCurrent());
        queue.removeAt(0);
        QVERIFY(!queue.stopAfterCurrent());
        QCOMPARE(queue.model()->rowCount(), 1);
        QTRY_VERIFY(!player.state().active);
    }

    void removingCurrentRowWhileIdleDisarmsStopAfterCurrent() {
        MpvController player(true, true);
        PlaylistController queue(player);
        queue.model()->addFiles(
            {QStringLiteral("/queue/one.mkv"), QStringLiteral("/queue/two.mkv")});
        queue.model()->setCurrentIndex(0);
        queue.setStopAfterCurrent(true);
        QVERIFY(queue.stopAfterCurrent());
        queue.removeAt(0);
        QVERIFY(!queue.stopAfterCurrent());
        QCOMPARE(queue.model()->rowCount(), 1);
        QCOMPARE(queue.model()->currentIndex(), 0);
    }

    void manualNextAdvancesWhileArmedThenEofStops() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.addFiles({media, media});
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(player.state().active && player.state().seekable);
        queue.setStopAfterCurrent(true);
        queue.playNext();
        QTRY_COMPARE(loaded.count(), 2);
        QCOMPARE(queue.model()->currentIndex(), 1);
        QVERIFY(queue.stopAfterCurrent());
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.seekAbsolute(3.8);
        QTRY_VERIFY(player.state().position > 3.0);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
        QCOMPARE(queue.model()->currentIndex(), 1);
        QVERIFY(!queue.stopAfterCurrent());
        QCOMPARE(loaded.count(), 2);
    }

    void brokenCycleFinishesAndNewItemsCanStart_data() {
        QTest::addColumn<int>("mode");
        QTest::newRow("single") << 1;
        QTest::newRow("all") << 2;
    }

    void brokenCycleFinishesAndNewItemsCanStart() {
        QFETCH(int, mode);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto broken = temporary.filePath(QStringLiteral("broken.mkv"));
        QFile file(broken);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("not a media file"), 16);
        file.close();
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy errors(&queue, &PlaylistController::queueError);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QVERIFY(queue.setProperty("repeatMode", mode));
        queue.addFiles({broken, QStringLiteral("/missing/neko.mkv"), broken});
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
        QTest::qWait(150);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(errors.count(), 3);
        QVERIFY(loaded.isEmpty());
        QVERIFY(!player.state().active);
        queue.addFiles({QString::fromUtf8(TEST_MEDIA_PATH)});
        QTRY_COMPARE(loaded.count(), 1);
        QCOMPARE(queue.model()->currentIndex(), 3);
    }

    void repeatAllCanWrapPastBrokenTailToPlayableHead() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto broken = temporary.filePath(QStringLiteral("broken.mkv"));
        QFile file(broken);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("invalid"), 7);
        file.close();
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        QVERIFY(queue.setProperty("repeatMode", 2));
        queue.addFiles({QString::fromUtf8(TEST_MEDIA_PATH), broken});
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.seekAbsolute(3.8);
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 2, 3000);
        QCOMPARE(queue.model()->currentIndex(), 0);
        QVERIFY(finished.isEmpty());
    }

    void emptyQueueNeverLoopsOrNavigates() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QVERIFY(queue.setProperty("repeatMode", 2));
        QVERIFY(!queue.hasNext() && !queue.hasPrevious());
        queue.playNext();
        queue.playPrevious();
        queue.playOrPause();
        QVERIFY(!player.state().active);
        queue.addFiles({QString::fromUtf8(TEST_MEDIA_PATH)});
        QTRY_VERIFY(player.state().active);
        queue.clear();
        QTRY_VERIFY(!player.state().active);
        QVERIFY(!queue.hasNext() && !queue.hasPrevious());
    }

    void appendingToSingleItemLoopEnablesAutomaticAdvance() {
        MpvController player(true, true);
        PlaylistController queue(player);
        queue.setRepeatMode(PlaylistController::RepeatMode::All);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.addFiles({media});
        QTRY_COMPARE(loaded.count(), 1);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        queue.addFiles({media});
        QVERIFY(player.state().paused);
        QCOMPARE(loaded.count(), 1);
        player.setPaused(false);
        QTRY_VERIFY(player.state().seekable && !player.state().paused);
        player.seekAbsolute(3.8);
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 2, 3000);
        QCOMPARE(queue.model()->currentIndex(), 1);
    }

    void nativeLoopPreservesTrackSelectionAndExternalSubtitles() {
        MpvController player(true, true);
        PlaylistController queue(player);
        queue.setRepeatMode(PlaylistController::RepeatMode::One);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        queue.addFiles({QString::fromUtf8(TEST_TRACK_MEDIA_PATH)});
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.selectAudioTrack(2);
        player.addSubtitle(QString::fromUtf8(TEST_EXTERNAL_SUBTITLE_PATH));
        const auto selected = [&player](MediaTrack::Type type, qint64 id) {
            for (const auto &track : player.tracks()) {
                if (track.type == type && track.id == id)
                    return track.selected;
            }
            return false;
        };
        QTRY_VERIFY(selected(MediaTrack::Type::Audio, 2));
        QTRY_VERIFY(selected(MediaTrack::Type::Subtitle, 2));
        QCOMPARE(player.tracks().size(), 4);
        player.setPaused(false);
        QTRY_VERIFY(!player.state().paused);
        player.seekAbsolute(3.8);
        QTRY_VERIFY(player.state().position > 3.0);
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active && player.state().position < 1.0, 3000);
        QCOMPARE(loaded.count(), 1);
        QCOMPARE(player.tracks().size(), 4);
        QVERIFY(selected(MediaTrack::Type::Audio, 2));
        QVERIFY(selected(MediaTrack::Type::Subtitle, 2));
    }

    void largeShuffledBatchRemovalKeepsNavigationUsable() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        const auto first = temporary.filePath(QStringLiteral("first.mkv"));
        const auto last = temporary.filePath(QStringLiteral("last.mkv"));
        QVERIFY(QFile::copy(media, first));
        QVERIFY(QFile::copy(media, last));

        QStringList paths;
        paths.fill(media, 5000);
        paths[0] = first;
        paths[4999] = last;

        MpvController player(true, true);
        PlaylistController queue(player);
        QAbstractItemModelTester tester(queue.model(),
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        queue.restoreFiles(paths, 2500);
        queue.setShuffleEnabled(true);
        queue.playAt(2500);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QTRY_COMPARE(loaded.count(), 1);

        QSignalSpy changed(&queue, &PlaylistController::currentChanged);
        QSignalSpy modelChanged(queue.model(), &PlaylistModel::currentIndexChanged);
        QSignalSpy removals(queue.model(), &QAbstractItemModel::rowsRemoved);

        QList<int> rows;
        for (auto row = 1000; row < 2000; ++row)
            rows.append(row);
        for (auto row = 3000; row < 4000; ++row)
            rows.append(row);
        queue.removeRowsAt(rows);

        QCOMPARE(queue.model()->rowCount(), 3000);
        QCOMPARE(queue.model()->currentIndex(), 1500);
        QCOMPARE(queue.model()->pathAt(0), first);
        QCOMPARE(queue.model()->pathAt(2999), last);
        // Two contiguous ranges, not one notification per removed row.
        QCOMPARE(removals.count(), 2);
        // One logical current change for the batch; the entry itself survives
        // and only its row number shifts.
        QCOMPARE(changed.count(), 1);
        QCOMPARE(modelChanged.count(), 1);

        // Pruning ran once for the batch, so both directions still reach survivors.
        QVERIFY(queue.hasNext());
        loaded.clear();
        queue.playNext();
        QTRY_COMPARE(loaded.count(), 1);
        const auto nextRow = queue.model()->currentIndex();
        QVERIFY(nextRow >= 0 && nextRow < 3000);
        QVERIFY(nextRow != 1500);
        QVERIFY(queue.hasPrevious());
        loaded.clear();
        queue.playPrevious();
        QTRY_COMPARE(loaded.count(), 1);
        QCOMPARE(queue.model()->currentIndex(), 1500);
    }
    void finishedHandlerCanNavigateWithoutDoubleAdvance() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        QStringList paths;
        for (const auto *name : {"first.mkv", "second.mkv", "third.mkv"}) {
            const auto path = temporary.filePath(QString::fromLatin1(name));
            QVERIFY(QFile::copy(media, path));
            paths.append(path);
        }
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        queue.restoreFiles(paths, 0);
        queue.playAt(0);
        QTRY_COMPARE(loaded.count(), 1);

        bool navigated = false;
        connect(&queue, &PlaylistController::mediaPlaybackFinished, &queue, [&] {
            if (!navigated) {
                navigated = true;
                queue.playNext();
            }
        });
        QVERIFY(QMetaObject::invokeMethod(&queue, "playFinished", Qt::DirectConnection));

        // The handler already advanced to row 1; playFinished must not advance
        // a second time from the new state.
        QTRY_COMPARE(loaded.count(), 2);
        QCOMPARE(queue.model()->currentIndex(), 1);
    }
};

QTEST_GUILESS_MAIN(PlaylistModesTest)
#include "playlist_modes_test.moc"
