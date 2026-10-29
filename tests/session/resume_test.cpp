#include "playback/mpv/mpvcontroller.h"
#include "playback/playlist/playlistcontroller.h"
#include "playback/playlist/playlistmodel.h"
#include "session/sessionmanager.h"
#include "session/sessionstore.h"

#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>
#include <limits>

class ResumeTest final : public QObject {
    Q_OBJECT
    static SessionState bookmark() {
        SessionState saved;
        saved.paths = {QString::fromUtf8(TEST_MEDIA_PATH), QString::fromUtf8(TEST_MEDIA_PATH)};
        saved.currentIndex = 1;
        saved.resumePosition = 1.24;
        return saved;
    }

  private slots:
    void resumePreferenceCanStartRestoredQueueAtBeginning() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        auto saved = bookmark();
        saved.resumePlaybackPosition = false;
        QVERIFY(SessionStore(file).save(saved).isEmpty());

        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QVERIFY(session.restore().error.isEmpty());
        QCOMPARE(queue.pendingResumePosition(), 0.0);
        QVERIFY(!SessionStore(file).load().state.resumePlaybackPosition);
    }

    void rendererReloadBeforeFirstPositionKeepsResumeOwnership() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        bool recovering = false;
        connect(&player, &MpvController::fileLoaded, &player, [&] {
            player.setPaused(true);
            if (!recovering) {
                recovering = true;
                player.prepareForRendererReset();
                player.resumeAfterRendererReset();
            }
        });
        player.openFileAtPosition(QString::fromUtf8(TEST_MEDIA_PATH), 1.24);
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_VERIFY(player.state().paused && player.state().seekable);
        QTRY_VERIFY(std::abs(player.state().position - 1.24) < 0.08);
        player.seekAbsolute(2.0);
        QTRY_VERIFY(std::abs(player.state().position - 2.0) < 0.08);
        QVERIFY(std::abs(player.resumePosition() - 2.0) < 0.08);
    }

    void replacingLoadingFileRetainsNewPosition() {
        MpvController player(true, true);
        const auto path = QString::fromUtf8(TEST_MEDIA_PATH);
        player.openFile(path);
        QTRY_VERIFY(player.state().active && player.state().seekable);
        int phase = 0;
        connect(&player, &MpvController::stateChanged, &player, [&] {
            if (player.state().active)
                return;
            if (phase == 0) {
                phase = 1;
                player.openFileAtPosition(path, 1.24);
            } else if (phase == 1) {
                QCOMPARE(player.resumePosition(), 1.24);
            }
        });
        connect(&player, &MpvController::fileLoaded, &player, [&] { player.setPaused(true); });
        player.openFile(path);
        QTRY_COMPARE(phase, 1);
        QTRY_VERIFY(player.state().paused);
        QTRY_VERIFY(std::abs(player.state().position - 1.24) < 0.08);
        phase = 2;
    }

    void exhaustedMissingSelectionDiscardsBookmark() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        auto saved = bookmark();
        saved.paths = {temporary.filePath(QStringLiteral("missing.mkv"))};
        saved.currentIndex = 0;
        QVERIFY(SessionStore(file).save(saved).isEmpty());
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QVERIFY(session.restore().error.isEmpty());
        connect(&queue, &PlaylistController::queueError, &queue,
                [&] { QCOMPARE(queue.pendingResumePosition(), 0.0); });
        queue.playOrPause();
        QVERIFY(session.save({}).isEmpty());
        QCOMPARE(SessionStore(file).load().state.resumePosition, 0.0);
    }

    void nearEndAndStopDoNotLeaveResumePosition() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QVERIFY(session.restore().error.isEmpty());
        queue.addFiles({QString::fromUtf8(TEST_MEDIA_PATH)});
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.seekAbsolute(3.4);
        QTRY_VERIFY(std::abs(player.state().position - 3.4) < 0.08);
        QVERIFY(session.save({}).isEmpty());
        QCOMPARE(SessionStore(file).load().state.resumePosition, 0.0);
        player.seekAbsolute(1.24);
        QTRY_VERIFY(std::abs(player.state().position - 1.24) < 0.08);
        player.stop();
        QVERIFY(session.save({}).isEmpty());
        QCOMPARE(SessionStore(file).load().state.resumePosition, 0.0);
    }

    void failedSelectionDoesNotApplyPositionToNextFile_data() {
        QTest::addColumn<bool>("missing");
        QTest::newRow("missing") << true;
        QTest::newRow("undecodable") << false;
    }

    void failedSelectionDoesNotApplyPositionToNextFile() {
        QFETCH(bool, missing);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        auto saved = bookmark();
        const auto invalid = temporary.filePath(QStringLiteral("invalid.mkv"));
        if (!missing) {
            QFile bad(invalid);
            QVERIFY(bad.open(QIODevice::WriteOnly));
            QCOMPARE(bad.write("invalid video"), 13);
        }
        saved.paths.prepend(invalid);
        saved.currentIndex = 0;
        QVERIFY(SessionStore(file).save(saved).isEmpty());
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QVERIFY(session.restore().error.isEmpty());
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&queue, &PlaylistController::queueError);
        connect(&player, &MpvController::fileLoaded, &player, [&] { player.setPaused(true); });
        queue.playOrPause();
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(player.state().paused);
        QCOMPARE(errors.count(), 1);
        QVERIFY(player.state().position < 0.08);
        QCOMPARE(queue.model()->currentIndex(), 1);
    }

    void nativeRepeatAfterResumeReturnsToBeginning() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        QVERIFY(SessionStore(file).save(bookmark()).isEmpty());
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QVERIFY(session.restore().error.isEmpty());
        queue.setRepeatMode(PlaylistController::RepeatMode::One);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        connect(&player, &MpvController::fileLoaded, &player, [&] { player.setPaused(true); });
        queue.playOrPause();
        QTRY_VERIFY(player.state().active && player.state().paused && player.state().seekable);
        QTRY_VERIFY(std::abs(player.state().position - 1.24) < 0.08);
        player.seekAbsolute(3.8);
        QTRY_VERIFY(player.state().position > 3.7);
        player.setPaused(false);
        QTRY_VERIFY_WITH_TIMEOUT(player.state().position < 0.4, 2000);
        QCOMPARE(loaded.count(), 1);
        player.stop();
        QTRY_VERIFY(!player.state().active);
        QVERIFY(session.save({}).isEmpty());
        QCOMPARE(SessionStore(file).load().state.resumePosition, 0.0);
    }

    void immediateSaveRetainsIdleAndLoadingBookmark() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        QVERIFY(SessionStore(file).save(bookmark()).isEmpty());
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QVERIFY(session.restore().error.isEmpty());
        QVERIFY(session.save({}).isEmpty());
        QCOMPARE(SessionStore(file).load().state.resumePosition, 1.24);
        queue.playOrPause();
        // Close before any asynchronous playback/property notification arrives.
        QVERIFY(session.save({}).isEmpty());
        QCOMPARE(SessionStore(file).load().state.resumePosition, 1.24);
        player.stop();
        QVERIFY(session.save({}).isEmpty());
        QCOMPARE(SessionStore(file).load().state.resumePosition, 0.0);
    }

    void bookmarkFollowsEntryAcrossEdits() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        QVERIFY(SessionStore(file).save(bookmark()).isEmpty());
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QVERIFY(session.restore().error.isEmpty());
        queue.moveItem(1, 0);
        QCOMPARE(queue.model()->currentIndex(), 0);
        queue.removeAt(1);
        QVERIFY(session.save({}).isEmpty());
        QCOMPARE(SessionStore(file).load().state.resumePosition, 1.24);
        connect(&player, &MpvController::fileLoaded, &player, [&] { player.setPaused(true); });
        queue.playOrPause();
        QTRY_VERIFY(player.state().active && player.state().paused);
        QTRY_VERIFY(std::abs(player.state().position - 1.24) < 0.08);
    }

    void anotherDuplicateStartsFromBeginning() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        QVERIFY(SessionStore(file).save(bookmark()).isEmpty());
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QVERIFY(session.restore().error.isEmpty());
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        connect(&player, &MpvController::fileLoaded, &player, [&] { player.setPaused(true); });
        queue.playAt(0);
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(player.state().paused);
        QVERIFY(player.state().position < 0.08);
        queue.playAt(1);
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_VERIFY(player.state().paused);
        QVERIFY(player.state().position < 0.08);
    }

    void explicitStopAndClearDiscardIdleBookmark_data() {
        QTest::addColumn<bool>("clear");
        QTest::newRow("stop") << false;
        QTest::newRow("clear") << true;
    }

    void explicitStopAndClearDiscardIdleBookmark() {
        QFETCH(bool, clear);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        QVERIFY(SessionStore(file).save(bookmark()).isEmpty());
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QVERIFY(session.restore().error.isEmpty());
        if (clear)
            queue.clear();
        else
            player.stop();
        QVERIFY(session.save({}).isEmpty());
        QCOMPARE(SessionStore(file).load().state.resumePosition, 0.0);
    }

    void invalidAndUnownedBookmarksAreDiscarded_data() {
        QTest::addColumn<double>("position");
        QTest::addColumn<QStringList>("paths");
        QTest::addColumn<int>("current");
        const QStringList paths{QString::fromUtf8(TEST_MEDIA_PATH)};
        QTest::newRow("negative") << -1.0 << paths << 0;
        QTest::newRow("nan") << std::numeric_limits<double>::quiet_NaN() << paths << 0;
        QTest::newRow("infinity") << std::numeric_limits<double>::infinity() << paths << 0;
        QTest::newRow("empty") << 1.24 << QStringList{} << -1;
        QTest::newRow("invalid-row") << 1.24 << paths << 3;
        QTest::newRow("filtered-row")
            << 1.24 << QStringList{QStringLiteral("relative.mkv"), paths.front()} << 0;
    }

    void invalidAndUnownedBookmarksAreDiscarded() {
        QFETCH(double, position);
        QFETCH(QStringList, paths);
        QFETCH(int, current);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        SessionStore store(temporary.filePath(QStringLiteral("settings.ini")));
        auto saved = bookmark();
        saved.paths = paths;
        saved.currentIndex = current;
        saved.resumePosition = position;
        QVERIFY(store.save(saved).isEmpty());
        const auto loaded = store.load();
        QVERIFY(loaded.error.isEmpty());
        QCOMPARE(loaded.state.resumePosition, 0.0);
    }

    void closingPausedPlaybackSavesPosition() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QVERIFY(session.restore().error.isEmpty());
        queue.addFiles({QString::fromUtf8(TEST_MEDIA_PATH)});
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.seekAbsolute(1.24);
        QTRY_VERIFY(std::abs(player.state().position - 1.24) < 0.08);
        QVERIFY(session.save({}).isEmpty());
        QSettings settings(file, QSettings::IniFormat);
        QVERIFY(std::abs(settings.value(QStringLiteral("playlist/resumePosition"), 0).toDouble() -
                         1.24) < 0.08);
        const auto history = PlaybackHistoryStore(file + QStringLiteral(".history.ini")).load();
        QCOMPARE(history.entries.size(), 1);
        QVERIFY(std::abs(history.entries.front().position - 1.24) < 0.08);
    }

    void restoredSelectionLoadsAtSavedPositionOnce() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        SessionState saved;
        saved.paths = {QString::fromUtf8(TEST_MEDIA_PATH)};
        saved.currentIndex = 0;
        QVERIFY(SessionStore(file).save(saved).isEmpty());
        {
            QSettings settings(file, QSettings::IniFormat);
            settings.setValue(QStringLiteral("playlist/resumePosition"), 1.24);
            settings.sync();
        }
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        connect(&player, &MpvController::fileLoaded, &player, [&] { player.setPaused(true); });
        QVERIFY(session.restore().error.isEmpty());
        QVERIFY(!player.state().active && loaded.isEmpty());
        queue.playOrPause();
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(player.state().paused);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(player.state().position - 1.24) < 0.08, 1500);
        player.stop();
        QTRY_VERIFY(!player.state().active);
        queue.playOrPause();
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_VERIFY(player.state().paused);
        QTRY_VERIFY(player.state().position < 0.08);
    }

    void switchingBetweenFilesRestoresEachFilesLastPosition() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        const auto firstMedia = QString::fromUtf8(TEST_MEDIA_PATH);
        const auto secondMedia = temporary.filePath(QStringLiteral("second.mkv"));
        QVERIFY(QFile::copy(firstMedia, secondMedia));
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QVERIFY(session.restore().error.isEmpty());
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        connect(&player, &MpvController::fileLoaded, &player, [&] { player.setPaused(true); });

        queue.addFiles({firstMedia, secondMedia});
        QTRY_VERIFY(player.state().active && player.state().paused && player.state().seekable);
        player.seekAbsolute(1.24);
        QTRY_VERIFY(std::abs(player.state().position - 1.24) < 0.08);
        queue.playAt(1);
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_VERIFY(player.state().active && player.state().paused);
        QVERIFY(player.state().position < 0.08);

        queue.playAt(0);
        QTRY_COMPARE(loaded.count(), 3);
        QTRY_VERIFY(player.state().active && player.state().paused);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(player.state().position - 1.24) < 0.08, 1500);
    }

    void perFileHistoryRestoresInNewSessionWithoutSessionBookmark() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        {
            MpvController player(true, true);
            PlaylistController queue(player);
            SessionManager session(player, queue, file);
            QVERIFY(session.restore().error.isEmpty());
            QSignalSpy snapshots(&queue, &PlaylistController::playbackPositionSnapshot);
            queue.addFiles({media});
            QTRY_VERIFY(player.state().active && player.state().seekable);
            player.setPaused(true);
            QTRY_VERIFY(player.state().paused);
            player.seekAbsolute(1.24);
            QTRY_VERIFY(std::abs(player.state().position - 1.24) < 0.08);
            player.stop();
            QCOMPARE(snapshots.count(), 1);
            QVERIFY(snapshots.front().at(1).toDouble() > 1.0);
            QVERIFY(snapshots.front().at(2).toDouble() > 1.0);
            QVERIFY(session.save({}).isEmpty());
            QCOMPARE(SessionStore(file).load().state.resumePosition, 0.0);
            QCOMPARE(
                PlaybackHistoryStore(file + QStringLiteral(".history.ini")).load().entries.size(),
                1);
        }

        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        connect(&player, &MpvController::fileLoaded, &player, [&] { player.setPaused(true); });
        QVERIFY(session.restore().error.isEmpty());
        queue.playOrPause();
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(player.state().paused);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(player.state().position - 1.24) < 0.08, 1500);
    }

    void closingAtBeginningClearsPerFileResumePosition() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        SessionState saved;
        saved.paths = {media};
        saved.currentIndex = 0;
        QVERIFY(SessionStore(file).save(saved).isEmpty());
        PlaybackHistoryStore history(file + QStringLiteral(".history.ini"));
        QVERIFY(history.save({{media, 1.24, 4.0, 100}}).isEmpty());

        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QVERIFY(session.restore().error.isEmpty());
        connect(&player, &MpvController::fileLoaded, &player, [&] { player.setPaused(true); });
        queue.playOrPause();
        QTRY_VERIFY(player.state().active && player.state().paused);
        QTRY_VERIFY(std::abs(player.state().position - 1.24) < 0.08);
        player.seekAbsolute(0.0);
        QTRY_VERIFY(player.state().position < 0.08);

        QVERIFY(session.save({}).isEmpty());
        QVERIFY(history.load().entries.isEmpty());
    }

    void eofRemovesHistoryForEndedMediaBeforeAdvancing() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        const auto firstMedia = QString::fromUtf8(TEST_MEDIA_PATH);
        const auto secondMedia = temporary.filePath(QStringLiteral("second.mkv"));
        QVERIFY(QFile::copy(firstMedia, secondMedia));
        PlaybackHistoryStore history(file + QStringLiteral(".history.ini"));
        QVERIFY(
            history.save({{firstMedia, 1.0, 4.0, 100}, {secondMedia, 2.24, 4.0, 200}}).isEmpty());
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        connect(&player, &MpvController::fileLoaded, &player, [&] { player.setPaused(true); });
        QVERIFY(session.restore().error.isEmpty());
        queue.addFiles({firstMedia, secondMedia});
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(player.state().paused);
        player.seekAbsolute(3.5);
        QTRY_VERIFY_WITH_TIMEOUT(player.state().position > 3.4, 1500);
        player.setPaused(false);
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(player.state().position - 2.24) < 0.08, 1500);
        const auto entries = history.load().entries;
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.front().path, PlaybackHistoryStore::normalizedPath(secondMedia));
        QVERIFY(std::abs(entries.front().position - 2.24) < 0.08);
    }
};

QTEST_GUILESS_MAIN(ResumeTest)
#include "resume_test.moc"
