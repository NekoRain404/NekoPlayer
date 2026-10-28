#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"
#include "session/sessionmanager.h"
#include "session/trackpreferencestore.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>

namespace {
QString backendText(MpvController &player, const char *property) {
    char *value = mpv_get_property_string(player.handle(), property);
    const auto text = QString::fromUtf8(value);
    mpv_free(value);
    return text;
}
} // namespace

class TrackPreferenceSessionTest final : public QObject {
    Q_OBJECT
  private slots:
    void identicalObservationsDoNotRewriteAndSaveFlushesChanges() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto settings = temporary.filePath(QStringLiteral("session.ini"));
        const auto tracks = settings + QStringLiteral(".tracks.ini");
        MpvController player(true, true);
        PlaylistController playlist(player);
        SessionManager session(player, playlist, settings);
        QVERIFY(session.restore().error.isEmpty());
        playlist.restoreFiles({QString::fromUtf8(TEST_TRACK_MEDIA_PATH)}, 0);
        playlist.playOrPause();
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        QTRY_VERIFY(QFileInfo::exists(tracks));

        QFile file(tracks);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto original = file.readAll();
        file.close();
        const auto frozen = QDateTime::currentDateTimeUtc().addSecs(-3600);
        QFile timestampFile(tracks);
        QVERIFY(timestampFile.open(QIODevice::ReadWrite));
        QVERIFY(timestampFile.setFileTime(frozen, QFileDevice::FileModificationTime));
        timestampFile.close();

        // Repeated identical snapshots only refresh lastUsedUtcMs in memory.
        for (int round = 0; round < 5; ++round) {
            player.tracksChanged();
            QTest::qWait(10);
        }
        QCOMPARE(QFileInfo(tracks).lastModified().toUTC().toMSecsSinceEpoch(),
                 frozen.toMSecsSinceEpoch());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), original);
        file.close();

        // A real selection change must reach disk before save() returns.
        const auto alternate =
            std::find_if(player.tracks().cbegin(), player.tracks().cend(), [](const auto &track) {
                return track.type == MediaTrack::Type::Audio && !track.selected;
            });
        QVERIFY(alternate != player.tracks().cend());
        player.selectAudioTrack(alternate->id);
        QVERIFY2(session.save({}).isEmpty(), "close-path flush must not fail");
        const auto stored = TrackPreferenceStore(tracks).load();
        QVERIFY(stored.error.isEmpty());
        QCOMPARE(stored.entries.size(), 1);
        QCOMPARE(stored.entries.front().selection.audioTrackId,
                 std::optional<qint64>(alternate->id));
    }

    void staleHistoricalFingerprintDoesNotBlockCurrentMediaSave() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto settings = temporary.filePath(QStringLiteral("session.ini"));
        const auto media = QString::fromUtf8(TEST_TRACK_MEDIA_PATH);
        const auto stale = temporary.filePath(QStringLiteral("stale.mkv"));
        QVERIFY(QFile::copy(media, stale));
        const QFileInfo info(stale);
        TrackPreferenceEntry old;
        old.mediaPath = stale;
        old.fileSize = info.size();
        old.modifiedUtcMs = info.lastModified().toUTC().toMSecsSinceEpoch();
        old.lastUsedUtcMs = 1;
        const TrackPreferenceStore store(settings + QStringLiteral(".tracks.ini"));
        QVERIFY(store.save({old}).isEmpty());
        MpvController player(true, true);
        PlaylistController playlist(player);
        SessionManager session(player, playlist, settings);
        QVERIFY(session.restore().error.isEmpty());
        QFile changed(stale);
        QVERIFY(changed.open(QIODevice::Append));
        QCOMPARE(changed.write("changed"), qint64(7));
        changed.close();
        playlist.restoreFiles({media}, 0);
        playlist.playOrPause();
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        player.selectAudioTrack(2);
        QTRY_COMPARE(backendText(player, "aid"), QStringLiteral("2"));
        QVERIFY(session.save({}).isEmpty());
        const auto stored = store.load();
        QVERIFY(stored.error.isEmpty());
        QCOMPARE(stored.entries.size(), 1);
        QCOMPARE(stored.entries.front().mediaPath, media);
        QCOMPARE(stored.entries.front().selection.audioTrackId, std::optional<qint64>(2));
    }

    void unsupportedTrackConfigurationIsReportedWithoutBlockingPlayback() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto settings = temporary.filePath(QStringLiteral("session.ini"));
        const auto tracks = settings + QStringLiteral(".tracks.ini");
        {
            QSettings future(tracks, QSettings::IniFormat);
            future.setValue(QStringLiteral("schemaVersion"), 77);
            future.sync();
        }
        QFile original(tracks);
        QVERIFY(original.open(QIODevice::ReadOnly));
        const auto bytes = original.readAll();
        original.close();
        MpvController player(true, true);
        PlaylistController playlist(player);
        SessionManager session(player, playlist, settings);
        QSignalSpy errors(&session, &SessionManager::configurationErrorOccurred);
        QVERIFY(session.restore().error.isEmpty());
        QCOMPARE(errors.count(), 1);
        playlist.restoreFiles({QString::fromUtf8(TEST_TRACK_MEDIA_PATH)}, 0);
        playlist.playOrPause();
        QTRY_VERIFY(player.state().active);
        // The unsupported auxiliary store must not turn a successful session
        // save into "session not saved"; it stays visible through the signal.
        QVERIFY2(session.save({}).isEmpty(),
                 "track-preference failure must not fail the session save");
        QVERIFY(errors.count() >= 1);
        QVERIFY(SessionStore(settings).load().error.isEmpty());
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), bytes);
    }

    void externalChoiceSurvivesReopen_data() {
        QTest::addColumn<bool>("disableFirst");
        QTest::newRow("menu-selection") << false;
        QTest::newRow("load-after-disable") << true;
    }

    void externalChoiceSurvivesReopen() {
        QFETCH(bool, disableFirst);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto settings = temporary.filePath(QStringLiteral("session.ini"));
        const auto media = QString::fromUtf8(TEST_TRACK_MEDIA_PATH);
        const auto external = QString::fromUtf8(TEST_EXTERNAL_SUBTITLE_PATH);
        MpvController player(true, true);
        PlaylistController playlist(player);
        SessionManager session(player, playlist, settings);
        QVERIFY(session.restore().error.isEmpty());
        playlist.restoreFiles({media}, 0);
        playlist.playOrPause();
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        player.setPaused(true);
        if (disableFirst) {
            player.selectSubtitleTrack(0);
            QTRY_COMPARE(backendText(player, "sid"), QStringLiteral("no"));
        }
        player.addSubtitle(external);
        QTRY_COMPARE(player.tracks().size(), 4);
        QTRY_COMPARE(backendText(player, "sid"), QStringLiteral("2"));
        if (!disableFirst) {
            player.selectSubtitleTrack(1);
            QTRY_COMPARE(backendText(player, "sid"), QStringLiteral("1"));
            player.selectSubtitleTrack(2);
            QTRY_COMPARE(backendText(player, "sid"), QStringLiteral("2"));
        }
        QVERIFY(session.save({}).isEmpty());
        const auto stored = TrackPreferenceStore(settings + QStringLiteral(".tracks.ini")).load();
        QVERIFY(stored.error.isEmpty());
        QCOMPARE(stored.entries.size(), 1);
        QVERIFY(!stored.entries.front().selection.subtitleTrackId.has_value());
        QCOMPARE(stored.entries.front().selection.selectedExternalSubtitlePath, external);
        QSignalSpy finished(&player, &MpvController::trackPreferenceRestoreFinished);
        player.openFile(media);
        QTRY_VERIFY(!finished.isEmpty());
        QTRY_COMPARE(player.tracks().size(), 4);
        QTRY_COMPARE(backendText(player, "sid"), QStringLiteral("2"));
    }

    void rendererRecoveryDoesNotDuplicateExternalTracks() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        MpvController player(true, true);
        PlaylistController playlist(player);
        SessionManager session(player, playlist, temporary.filePath(QStringLiteral("session.ini")));
        QVERIFY(session.restore().error.isEmpty());
        playlist.restoreFiles({QString::fromUtf8(TEST_TRACK_MEDIA_PATH)}, 0);
        playlist.playOrPause();
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.addSubtitle(QString::fromUtf8(TEST_EXTERNAL_SUBTITLE_PATH));
        QTRY_COMPARE(player.tracks().size(), 4);
        QTRY_COMPARE(backendText(player, "sid"), QStringLiteral("2"));
        QVERIFY(session.save({}).isEmpty());
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy restored(&player, &MpvController::trackPreferenceRestoreFinished);
        player.prepareForRendererReset();
        player.resumeAfterRendererReset();
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_COMPARE(player.tracks().size(), 4);
        QTRY_COMPARE(backendText(player, "sid"), QStringLiteral("2"));
        QTest::qWait(100);
        QCOMPARE(player.tracks().size(), 4);
        QVERIFY(restored.isEmpty());
        QVERIFY(player.state().paused);
    }

    void disabledTracksSurviveRepeatedReopensAndImmediateCompletionSave() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto settings = temporary.filePath(QStringLiteral("session.ini"));
        const auto media = QString::fromUtf8(TEST_TRACK_MEDIA_PATH);
        MpvController player(true, true);
        PlaylistController playlist(player);
        SessionManager session(player, playlist, settings);
        QVERIFY(session.restore().error.isEmpty());
        playlist.restoreFiles({media}, 0);
        playlist.playOrPause();
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        player.selectAudioTrack(0);
        player.selectSubtitleTrack(0);
        QTRY_COMPARE(backendText(player, "aid"), QStringLiteral("no"));
        QTRY_COMPARE(backendText(player, "sid"), QStringLiteral("no"));
        QVERIFY(session.save({}).isEmpty());
        connect(&player, &MpvController::trackPreferenceRestoreFinished, &session,
                [&](bool canceled) {
                    if (!canceled)
                        QVERIFY(session.save({}).isEmpty());
                });
        QSignalSpy finished(&player, &MpvController::trackPreferenceRestoreFinished);
        for (int round = 0; round < 2; ++round) {
            player.openFile(media);
            QTRY_COMPARE(finished.count(), round + 1);
            const auto entries =
                TrackPreferenceStore(settings + QStringLiteral(".tracks.ini")).load().entries;
            QCOMPARE(entries.size(), 1);
            QCOMPARE(entries.front().selection.audioTrackId, std::optional<qint64>(0));
            QCOMPARE(entries.front().selection.subtitleTrackId, std::optional<qint64>(0));
            QTRY_COMPARE(backendText(player, "aid"), QStringLiteral("no"));
        }
    }

    void disabledTracksSurviveThreeControllerSessions() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto settings = temporary.filePath(QStringLiteral("session.ini"));
        const auto media = QString::fromUtf8(TEST_TRACK_MEDIA_PATH);
        for (int round = 0; round < 3; ++round) {
            MpvController player(true, true);
            PlaylistController playlist(player);
            SessionManager session(player, playlist, settings);
            QVERIFY(session.restore().error.isEmpty());
            playlist.restoreFiles({media}, 0);
            QSignalSpy finished(&player, &MpvController::trackPreferenceRestoreFinished);
            playlist.playOrPause();
            QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
            if (round == 0) {
                player.selectAudioTrack(0);
                player.selectSubtitleTrack(0);
            } else {
                QTRY_COMPARE(finished.count(), 1);
            }
            QTRY_COMPARE(backendText(player, "aid"), QStringLiteral("no"));
            QTRY_COMPARE(backendText(player, "sid"), QStringLiteral("no"));
            QVERIFY(session.save({}).isEmpty());
            const auto entries =
                TrackPreferenceStore(settings + QStringLiteral(".tracks.ini")).load().entries;
            QCOMPARE(entries.size(), 1);
            QCOMPARE(entries.front().selection.audioTrackId, std::optional<qint64>(0));
            QCOMPARE(entries.front().selection.subtitleTrackId, std::optional<qint64>(0));
        }
    }

    void selectedAudioTrackSurvivesControllerRestart() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto settings = temporary.filePath(QStringLiteral("session.ini"));
        const auto media = QString::fromUtf8(TEST_TRACK_MEDIA_PATH);
        QVERIFY(QFileInfo::exists(media));
        qint64 selectedId = 0;
        {
            MpvController player(true, true);
            PlaylistController playlist(player);
            SessionManager session(player, playlist, settings);
            QVERIFY(session.restore().error.isEmpty());
            playlist.restoreFiles({media}, 0);
            playlist.playOrPause();
            QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
            const auto alternate = std::find_if(
                player.tracks().cbegin(), player.tracks().cend(), [](const auto &track) {
                    return track.type == MediaTrack::Type::Audio && !track.selected;
                });
            QVERIFY(alternate != player.tracks().cend());
            selectedId = alternate->id;
            player.selectAudioTrack(selectedId);
            QTRY_COMPARE(backendText(player, "aid").toLongLong(), selectedId);
            QTRY_VERIFY([&] {
                const auto loaded =
                    TrackPreferenceStore(settings + QStringLiteral(".tracks.ini")).load();
                return !loaded.entries.isEmpty() &&
                       loaded.entries.front().selection.audioTrackId == selectedId;
            }());
            const auto entries =
                TrackPreferenceStore(settings + QStringLiteral(".tracks.ini")).load().entries;
            QCOMPARE(entries.size(), 1);
            QCOMPARE(entries.front().selection.audioTrackId, std::optional<qint64>(selectedId));
            QVERIFY(session.save({}).isEmpty());
        }

        MpvController player(true, true);
        PlaylistController playlist(player);
        SessionManager session(player, playlist, settings);
        QVERIFY(session.restore().error.isEmpty());
        playlist.restoreFiles({media}, 0);
        playlist.playOrPause();
        QTRY_VERIFY(player.state().active);
        QTRY_COMPARE(backendText(player, "aid").toLongLong(), selectedId);
    }

    void changedMediaFingerprintDoesNotRestoreOldSelection() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto settings = temporary.filePath(QStringLiteral("session.ini"));
        const auto media = temporary.filePath(QStringLiteral("multiple-tracks.mkv"));
        QVERIFY(QFile::copy(QString::fromUtf8(TEST_TRACK_MEDIA_PATH), media));
        const QFileInfo before(media);
        TrackPreferenceEntry entry;
        entry.mediaPath = before.canonicalFilePath();
        entry.fileSize = before.size();
        entry.modifiedUtcMs = before.lastModified().toUTC().toMSecsSinceEpoch();
        entry.lastUsedUtcMs = 1;
        entry.selection.audioTrackId = 2;
        QVERIFY(
            TrackPreferenceStore(settings + QStringLiteral(".tracks.ini")).save({entry}).isEmpty());
        QFile changed(media);
        QVERIFY(changed.open(QIODevice::Append));
        QVERIFY(changed.write("x") == 1);
        changed.close();

        MpvController player(true, true);
        PlaylistController playlist(player);
        SessionManager session(player, playlist, settings);
        QVERIFY(session.restore().error.isEmpty());
        playlist.restoreFiles({media}, 0);
        playlist.playOrPause();
        QTRY_VERIFY(player.state().active);
        QTRY_COMPARE(backendText(player, "aid").toLongLong(), qint64(1));
    }

    void mediaChangedDuringPlaybackDoesNotReuseOldSelection() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto settings = temporary.filePath(QStringLiteral("session.ini"));
        const auto media = temporary.filePath(QStringLiteral("multiple-tracks.mkv"));
        QVERIFY(QFile::copy(QString::fromUtf8(TEST_TRACK_MEDIA_PATH), media));
        MpvController player(true, true);
        PlaylistController playlist(player);
        SessionManager session(player, playlist, settings);
        QVERIFY(session.restore().error.isEmpty());
        playlist.restoreFiles({media}, 0);
        playlist.playOrPause();
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        QTRY_COMPARE(backendText(player, "aid"), QStringLiteral("1"));
        player.selectAudioTrack(2);
        QTRY_COMPARE(backendText(player, "aid"), QStringLiteral("2"));
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QFile changed(media);
        QVERIFY(changed.open(QIODevice::Append));
        QCOMPARE(changed.write("changed"), qint64(7));
        changed.close();
        QVERIFY(session.save({}).isEmpty());
        QVERIFY(TrackPreferenceStore(settings + QStringLiteral(".tracks.ini"))
                    .load()
                    .entries.isEmpty());
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        connect(&player, &MpvController::fileLoaded, &player,
                [&player] { player.setPaused(true); });
        player.openFile(media);
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_COMPARE(backendText(player, "aid"), QStringLiteral("1"));
    }

    void explicitlyDisabledSubtitleIsPersisted() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto settings = temporary.filePath(QStringLiteral("session.ini"));
        const auto media = QString::fromUtf8(TEST_TRACK_MEDIA_PATH);
        MpvController player(true, true);
        PlaylistController playlist(player);
        SessionManager session(player, playlist, settings);
        QVERIFY(session.restore().error.isEmpty());
        playlist.restoreFiles({media}, 0);
        playlist.playOrPause();
        QTRY_VERIFY(player.state().active);
        player.selectSubtitleTrack(0);
        QTRY_COMPARE(backendText(player, "sid"), QStringLiteral("no"));
        QTRY_VERIFY([&] {
            const auto loaded =
                TrackPreferenceStore(settings + QStringLiteral(".tracks.ini")).load();
            return !loaded.entries.isEmpty() &&
                   loaded.entries.front().selection.subtitleTrackId == std::optional<qint64>(0);
        }());
    }

    void mediaSwitchDoesNotInheritSelectionIntent() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto settings = temporary.filePath(QStringLiteral("session.ini"));
        const auto source = QString::fromUtf8(TEST_TRACK_MEDIA_PATH);
        const auto first = temporary.filePath(QStringLiteral("first.mkv"));
        const auto second = temporary.filePath(QStringLiteral("second.mkv"));
        QVERIFY(QFile::copy(source, first));
        QVERIFY(QFile::copy(source, second));

        MpvController player(true, true);
        PlaylistController playlist(player);
        SessionManager session(player, playlist, settings);
        QVERIFY(session.restore().error.isEmpty());
        playlist.restoreFiles({first, second}, 0);
        playlist.playOrPause();
        QTRY_VERIFY(player.state().active);
        QTRY_VERIFY(!player.tracks().isEmpty());
        const auto initialSubtitle = backendText(player, "sid");
        const auto initialSubtitleId =
            initialSubtitle == QStringLiteral("no") ? qint64(0) : initialSubtitle.toLongLong();
        player.selectSubtitleTrack(0);
        QTRY_COMPARE(backendText(player, "sid"), QStringLiteral("no"));

        playlist.playNext();
        QTRY_VERIFY(player.state().active && player.state().path == second);
        QTRY_VERIFY(!player.tracks().isEmpty());
        QTRY_COMPARE(backendText(player, "sid"), initialSubtitle);
        QVERIFY(session.save({}).isEmpty());

        const auto entries =
            TrackPreferenceStore(settings + QStringLiteral(".tracks.ini")).load().entries;
        const auto secondEntry =
            std::find_if(entries.cbegin(), entries.cend(), [&second](const auto &entry) {
                return entry.mediaPath == TrackPreferenceStore::normalizedPath(second);
            });
        QVERIFY(secondEntry != entries.cend());
        QCOMPARE(secondEntry->selection.subtitleTrackId, std::optional<qint64>(initialSubtitleId));
    }
};

QTEST_GUILESS_MAIN(TrackPreferenceSessionTest)
#include "track_preference_session_test.moc"
