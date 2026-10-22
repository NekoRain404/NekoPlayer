#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "session/sessionmanager.h"
#include "session/trackpreferencestore.h"

#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace {
QString audioSelection(MpvController &player) {
    char *value = mpv_get_property_string(player.handle(), "aid");
    const auto text = QString::fromUtf8(value);
    mpv_free(value);
    return text;
}

QList<MediaTrack> externalAudioTracks(const MpvController &player) {
    QList<MediaTrack> result;
    for (const auto &track : player.tracks()) {
        if (track.type == MediaTrack::Type::Audio && track.external)
            result.append(track);
    }
    return result;
}
} // namespace

class ExternalAudioSessionTest final : public QObject {
    Q_OBJECT
  private slots:
    void externalAudioChoiceSurvivesControllerRestart_data() {
        QTest::addColumn<int>("choice");
        QTest::newRow("first-external-stream") << 0;
        QTest::newRow("second-external-stream") << 1;
        QTest::newRow("audio-disabled") << -1;
        QTest::newRow("internal-audio") << -2;
    }

    void externalAudioChoiceSurvivesControllerRestart() {
        QFETCH(int, choice);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto settings = temporary.filePath(QStringLiteral("session.ini"));
        const auto media = QString::fromUtf8(TEST_TRACK_MEDIA_PATH);
        const auto external = QString::fromUtf8(TEST_EXTERNAL_AUDIO_PATH);
        QVERIFY(QFileInfo::exists(external));
        const TrackPreferenceStore store(settings + QStringLiteral(".tracks.ini"));
        for (int round = 0; round < 2; ++round) {
            MpvController player(true, true);
            PlaylistController playlist(player);
            SessionManager session(player, playlist, settings);
            QVERIFY(session.restore().error.isEmpty());
            playlist.restoreFiles({media}, 0);
            QSignalSpy restored(&player, &MpvController::trackPreferenceRestoreFinished);
            playlist.playOrPause();
            QTRY_VERIFY(player.state().active && player.tracks().size() >= 3);
            player.setPaused(true);
            if (round == 0)
                player.addAudio(external);
            else
                QTRY_COMPARE(restored.count(), 1);
            QTRY_COMPARE(externalAudioTracks(player).size(), 2);
            const auto tracks = externalAudioTracks(player);
            const qint64 selected = choice >= 0 ? tracks.at(choice).id : choice == -1 ? 0 : 2;
            if (round == 0)
                player.selectAudioTrack(selected);
            QTRY_COMPARE(audioSelection(player),
                         selected == 0 ? QStringLiteral("no") : QString::number(selected));
            QVERIFY(session.save({}).isEmpty());
            const auto loaded = store.load();
            QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
            QCOMPARE(loaded.entries.size(), 1);
            const auto &selection = loaded.entries.front().selection;
            QCOMPARE(selection.externalAudioPaths, QStringList{external});
            if (choice >= 0) {
                QVERIFY(!selection.audioTrackId.has_value());
                QCOMPARE(selection.selectedExternalAudioPath, external);
                QCOMPARE(selection.selectedExternalAudioStream, choice);
            } else {
                QCOMPARE(selection.audioTrackId, std::optional<qint64>(selected));
                QVERIFY(selection.selectedExternalAudioPath.isEmpty());
                QCOMPARE(selection.selectedExternalAudioStream, -1);
            }
        }
    }

    void loadingExternalAudioClearsEarlierDisabledIntent() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto settings = temporary.filePath(QStringLiteral("session.ini"));
        const auto media = QString::fromUtf8(TEST_TRACK_MEDIA_PATH);
        const auto external = QString::fromUtf8(TEST_EXTERNAL_AUDIO_PATH);
        MpvController player(true, true);
        PlaylistController playlist(player);
        SessionManager session(player, playlist, settings);
        QVERIFY(session.restore().error.isEmpty());
        playlist.restoreFiles({media}, 0);
        playlist.playOrPause();
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        player.setPaused(true);
        // Queue both requests before the backend can confirm the disabled selection.
        player.selectAudioTrack(0);
        player.addAudio(external);
        QTRY_COMPARE(externalAudioTracks(player).size(), 2);
        QTRY_VERIFY(audioSelection(player) != QStringLiteral("no"));
        QVERIFY(session.save({}).isEmpty());
        const auto loaded = TrackPreferenceStore(settings + QStringLiteral(".tracks.ini")).load();
        QVERIFY(loaded.error.isEmpty());
        QCOMPARE(loaded.entries.size(), 1);
        QVERIFY(!loaded.entries.front().selection.audioTrackId.has_value());
        QCOMPARE(loaded.entries.front().selection.selectedExternalAudioPath, external);
        QSignalSpy restored(&player, &MpvController::trackPreferenceRestoreFinished);
        player.openFile(media);
        QTRY_COMPARE(restored.count(), 1);
        QTRY_COMPARE(externalAudioTracks(player).size(), 2);
        QTRY_VERIFY(audioSelection(player) != QStringLiteral("no"));
    }
};

QTEST_GUILESS_MAIN(ExternalAudioSessionTest)
#include "external_audio_session_test.moc"
