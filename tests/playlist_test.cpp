#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
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

    void displayNamesFollowPlaylistEdits() {
        PlaylistModel model;
        const auto local = QDir::current().absoluteFilePath(QStringLiteral("local.mkv"));
        const auto duplicate = QDir::current().absoluteFilePath(QStringLiteral("duplicate.mkv"));
        const auto network = QStringLiteral("https://example.test/media/network%20movie.mkv");
        QCOMPARE(model.addFiles({QStringLiteral("local.mkv"), network,
                                 QStringLiteral("duplicate.mkv"), QStringLiteral("duplicate.mkv")}),
                 4);
        QCOMPARE(model.pathAt(0), local);
        QCOMPARE(model.pathAt(1), network);
        QCOMPARE(model.pathAt(2), duplicate);
        QCOMPARE(model.pathAt(3), duplicate);
        QCOMPARE(model.data(model.index(0)).toString(), QStringLiteral("local.mkv"));
        QCOMPARE(model.data(model.index(1)).toString(), QStringLiteral("network movie.mkv"));
        QCOMPARE(model.data(model.index(2)).toString(), QStringLiteral("duplicate.mkv"));
        QCOMPARE(model.data(model.index(3)).toString(), QStringLiteral("duplicate.mkv"));
        QCOMPARE(model.data(model.index(1), PlaylistModel::PathRole).toString(), network);
        QCOMPARE(model.data(model.index(1), Qt::ToolTipRole).toString(), network);

        // Moving a row carries its cached display name with its path.
        QVERIFY(model.moveRow({}, 1, {}, 0));
        QCOMPARE(model.pathAt(0), network);
        QCOMPARE(model.data(model.index(0)).toString(), QStringLiteral("network movie.mkv"));
        QCOMPARE(model.data(model.index(1)).toString(), QStringLiteral("local.mkv"));
        QCOMPARE(model.data(model.index(2)).toString(), QStringLiteral("duplicate.mkv"));
        QCOMPARE(model.data(model.index(3)).toString(), QStringLiteral("duplicate.mkv"));

        // Removing a row shifts the remaining cached names with their paths.
        QVERIFY(model.removeAt(1));
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.pathAt(0), network);
        QCOMPARE(model.data(model.index(0)).toString(), QStringLiteral("network movie.mkv"));
        QCOMPARE(model.pathAt(1), duplicate);
        QCOMPARE(model.pathAt(2), duplicate);
        QCOMPARE(model.data(model.index(1)).toString(), QStringLiteral("duplicate.mkv"));
        QCOMPARE(model.data(model.index(2)).toString(), QStringLiteral("duplicate.mkv"));

        // Duplicate paths stay independent rows; moving one leaves the other in place.
        QVERIFY(model.setCurrentIndex(2));
        QVERIFY(model.moveRow({}, 2, {}, 0));
        QCOMPARE(model.currentIndex(), 0);
        QCOMPARE(model.pathAt(0), duplicate);
        QCOMPARE(model.pathAt(1), network);
        QCOMPARE(model.pathAt(2), duplicate);
        QCOMPARE(model.data(model.index(0)).toString(), QStringLiteral("duplicate.mkv"));
        QCOMPARE(model.data(model.index(1)).toString(), QStringLiteral("network movie.mkv"));
        QCOMPARE(model.data(model.index(2)).toString(), QStringLiteral("duplicate.mkv"));

        model.clear();
        QCOMPARE(model.rowCount(), 0);
        QVERIFY(model.paths().isEmpty());
    }

    void modelIgnoresEmptyPaths() {
        PlaylistModel model;
        QCOMPARE(model.addFiles({QString{}, QStringLiteral("a.mkv"), QString{}}), 1);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0)).toString(), QStringLiteral("a.mkv"));
    }

    void modelPreservesUrlEntriesForPlaylistRoundTrips() {
        PlaylistModel model;
        const auto url = QStringLiteral("https://example.test/media/movie.mkv");
        QCOMPARE(model.addFiles({url}), 1);
        QCOMPARE(model.pathAt(0), url);
    }

    void controllerDoesNotSkipUrlEntries() {
        MpvController player(true, true);
        PlaylistController queue(player);
        const auto url = QStringLiteral("http://127.0.0.1:1/unavailable.mkv");
        queue.addFiles({url, QString::fromUtf8(TEST_MEDIA_PATH)});
        QCOMPARE(queue.model()->currentIndex(), 0);
        QCOMPARE(queue.model()->currentPath(), url);
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

    void bulkRemovalKeepsSurvivingPlayback() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        QStringList paths;
        for (const auto *name : {"first.mkv", "second.mkv", "third.mkv", "fourth.mkv"}) {
            const auto path = temporary.filePath(QString::fromLatin1(name));
            QVERIFY(QFile::copy(media, path));
            paths.append(path);
        }
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy changed(&queue, &PlaylistController::currentChanged);
        queue.restoreFiles(paths, 1);
        queue.playAt(1);
        QTRY_COMPARE(loaded.count(), 1);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused && player.state().seekable);
        player.seekAbsolute(1.5);
        QTRY_VERIFY(qAbs(player.state().position - 1.5) < 0.08);
        changed.clear();
        queue.removeRowsAt({0, 3, 3, -1, 9});
        QCOMPARE(queue.model()->rowCount(), 2);
        QCOMPARE(queue.model()->currentIndex(), 0);
        QCOMPARE(queue.model()->currentPath(), paths.at(1));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(player.state().path, paths.at(1));
        QVERIFY(player.state().active && player.state().paused);
        QVERIFY(qAbs(player.state().position - 1.5) < 0.08);
        QCOMPARE(loaded.count(), 1);
    }

    void bulkRemovalOfCurrentStopsOnceAndSelectsSurvivor() {
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
        QSignalSpy stops(&player, &MpvController::stopRequested);
        QSignalSpy changed(&queue, &PlaylistController::currentChanged);
        queue.restoreFiles(paths, 1);
        queue.playAt(1);
        QTRY_COMPARE(loaded.count(), 1);
        changed.clear();
        queue.removeRowsAt({1});
        QCOMPARE(stops.count(), 1);
        QTRY_VERIFY(!player.state().active);
        QCOMPARE(queue.model()->rowCount(), 2);
        QCOMPARE(queue.model()->currentIndex(), 1);
        QCOMPARE(queue.model()->currentPath(), paths.at(2));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(loaded.count(), 1);
        queue.removeRowsAt({0, 1});
        QCOMPARE(stops.count(), 2);
        QCOMPARE(queue.model()->rowCount(), 0);
        QCOMPARE(queue.model()->currentIndex(), -1);
        QCOMPARE(changed.count(), 2);
        QCOMPARE(loaded.count(), 1);
    }

    void missingFileRowsDistinguishExistingMissingAndNetwork() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto existing = temporary.filePath(QStringLiteral("existing.mkv"));
        QFile file(existing);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        const auto missing = temporary.filePath(QStringLiteral("missing.mkv"));
        const auto network = QStringLiteral("https://example.test/movie.mkv");
        MpvController player(true, true);
        PlaylistController queue(player);
        queue.restoreFiles({existing, missing, network, QUrl::fromLocalFile(missing).toString(),
                            QUrl::fromLocalFile(existing).toString()},
                           0);
        const auto rows = queue.missingFileRows();
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows.at(0), 1);
        QCOMPARE(rows.at(1), 3);
        QVERIFY(!player.state().active);
    }

    void removeMissingFilesRemovesOnlyMissingRows() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        const auto second = temporary.filePath(QStringLiteral("second.mkv"));
        QVERIFY(QFile::copy(media, second));
        const auto missingFirst = temporary.filePath(QStringLiteral("missing-first.mkv"));
        const auto missingLast = temporary.filePath(QStringLiteral("missing-last.mkv"));
        const auto network = QStringLiteral("https://example.test/movie.mkv");
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        queue.restoreFiles({missingFirst, media, network, missingLast}, 1);
        queue.playAt(1);
        QTRY_COMPARE(loaded.count(), 1);
        QCOMPARE(queue.removeMissingFiles(), 2);
        QCOMPARE(queue.model()->paths(), QStringList({media, network}));
        QCOMPARE(queue.model()->currentIndex(), 0);
        QCOMPARE(player.state().path, media);
        QVERIFY(player.state().active);
        QCOMPARE(loaded.count(), 1);

        queue.restoreFiles({media, missingLast}, 1);
        QCOMPARE(queue.model()->currentIndex(), 1);
        QCOMPARE(queue.removeMissingFiles(), 1);
        QCOMPARE(queue.model()->rowCount(), 1);
        QCOMPARE(queue.model()->currentIndex(), 0);
        QCOMPARE(queue.model()->currentPath(), media);
        QTRY_VERIFY(!player.state().active);
        QCOMPARE(loaded.count(), 1);
    }

    void sortKeepsPlaybackAndArmedStopAfterCurrent() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        QStringList paths;
        for (const auto *name : {"episode3.mkv", "episode1.mkv", "episode2.mkv"}) {
            const auto path = temporary.filePath(QString::fromLatin1(name));
            QVERIFY(QFile::copy(media, path));
            paths.append(path);
        }
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy changed(&queue, &PlaylistController::currentChanged);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        queue.restoreFiles(paths, 0);
        queue.playAt(0);
        QTRY_COMPARE(loaded.count(), 1);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused && player.state().seekable);
        player.seekAbsolute(1.5);
        QTRY_VERIFY(qAbs(player.state().position - 1.5) < 0.08);
        const auto currentPath = player.state().path;
        queue.setStopAfterCurrent(true);
        changed.clear();
        queue.sortPlaylist(PlaylistModel::SortKey::Title, Qt::AscendingOrder);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(queue.model()->paths(), QStringList({paths.at(1), paths.at(2), paths.at(0)}));
        QCOMPARE(queue.model()->currentIndex(), 2);
        QCOMPARE(queue.model()->currentPath(), currentPath);
        QCOMPARE(player.state().path, currentPath);
        QVERIFY(player.state().active && player.state().paused);
        QVERIFY(qAbs(player.state().position - 1.5) < 0.08);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(queue.stopAfterCurrent());

        changed.clear();
        queue.sortPlaylist(PlaylistModel::SortKey::Title, Qt::AscendingOrder);
        QCOMPARE(changed.count(), 0);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(queue.stopAfterCurrent());

        player.setPaused(false);
        player.seekAbsolute(3.8);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
        QVERIFY(!queue.stopAfterCurrent());
        QCOMPARE(loaded.count(), 1);
    }

    void shuffleRemainsCoherentAfterBulkRemovalAndSort() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        QStringList paths;
        for (const auto *name : {"d.mkv", "b.mkv", "a.mkv", "c.mkv"}) {
            const auto path = temporary.filePath(QString::fromLatin1(name));
            QVERIFY(QFile::copy(media, path));
            paths.append(path);
        }
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        queue.restoreFiles(paths, 1);
        queue.setShuffleEnabled(true);
        queue.playAt(1);
        QTRY_COMPARE(loaded.count(), 1);
        queue.removeRowsAt({0, 3});
        QCOMPARE(queue.model()->rowCount(), 2);
        QCOMPARE(queue.model()->currentIndex(), 0);
        QCOMPARE(queue.model()->currentPath(), paths.at(1));
        QVERIFY(queue.hasNext());
        QVERIFY(!queue.hasPrevious());
        queue.playNext();
        QTRY_COMPARE(loaded.count(), 2);
        QCOMPARE(queue.model()->currentIndex(), 1);
        QCOMPARE(queue.model()->currentPath(), paths.at(2));

        queue.sortPlaylist(PlaylistModel::SortKey::Title, Qt::AscendingOrder);
        QCOMPARE(queue.model()->currentPath(), paths.at(2));
        QCOMPARE(player.state().path, paths.at(2));
        QVERIFY(player.state().active);
        QVERIFY(queue.hasNext());
        QVERIFY(!queue.hasPrevious());
        queue.playNext();
        QTRY_COMPARE(loaded.count(), 3);
        QCOMPARE(queue.model()->currentPath(), paths.at(1));
    }
};

QTEST_GUILESS_MAIN(PlaylistTest)
#include "playlist_test.moc"
