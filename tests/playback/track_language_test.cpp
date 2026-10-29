#include "playback/media/mediatrack.h"
#include "playback/mpv/mpvcontroller.h"
#include "playback/playlist/playlistcontroller.h"
#include "session/sessionmanager.h"
#include "session/sessionstate.h"
#include "session/sessionstore.h"
#include "session/trackpreferencestore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>

namespace {
qint64 selectedTrack(const MpvController &player, MediaTrack::Type type) {
    for (const auto &track : player.tracks()) {
        if (track.type == type && track.selected)
            return track.id;
    }
    return 0;
}

QString selectedLanguage(const MpvController &player, MediaTrack::Type type) {
    for (const auto &track : player.tracks()) {
        if (track.type == type && track.selected)
            return track.language;
    }
    return {};
}

int externalSubtitleCount(const MpvController &player) {
    return static_cast<int>(
        std::count_if(player.tracks().cbegin(), player.tracks().cend(), [](const auto &track) {
            return track.type == MediaTrack::Type::Subtitle && track.external;
        }));
}

qint64 externalSubtitleId(const MpvController &player) {
    for (const auto &track : player.tracks()) {
        if (track.type == MediaTrack::Type::Subtitle && track.external)
            return track.id;
    }
    return 0;
}

QString externalSubtitlePath(const MpvController &player) {
    for (const auto &track : player.tracks()) {
        if (track.type == MediaTrack::Type::Subtitle && track.external)
            return track.externalPath;
    }
    return {};
}

bool writeSiblingSubtitle(const QString &directory) {
    QFile subtitle(QDir(directory).filePath(QStringLiteral("movie.srt")));
    return subtitle.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           subtitle.write("1\n00:00:00,000 --> 00:00:04,000\nExternal\n") > 0;
}

bool prepareSiblingMedia(const QTemporaryDir &temporary) {
    return temporary.isValid() &&
           QFile::copy(QString::fromUtf8(TEST_LANGUAGE_MEDIA_PATH),
                       temporary.filePath(QStringLiteral("movie.mkv"))) &&
           writeSiblingSubtitle(temporary.path());
}

void waitForTracks(MpvController &player, int count) {
    QTRY_COMPARE_WITH_TIMEOUT(player.tracks().size(), count, 10000);
    player.setPaused(true);
    QTRY_VERIFY_WITH_TIMEOUT(player.state().paused, 5000);
}
} // namespace

class TrackLanguageTest final : public QObject {
    Q_OBJECT
  private slots:
    void preferencesSelectEnglishAudioAndSubtitle() {
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.setLanguagePreferences({QStringLiteral("eng")}, {QStringLiteral("eng")});
        player.openFile(QString::fromUtf8(TEST_LANGUAGE_MEDIA_PATH));
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 10000);
        waitForTracks(player, 4);

        QTRY_COMPARE(selectedLanguage(player, MediaTrack::Type::Audio), QStringLiteral("eng"));
        QTRY_COMPARE(selectedLanguage(player, MediaTrack::Type::Subtitle), QStringLiteral("eng"));
        QCOMPARE(selectedTrack(player, MediaTrack::Type::Audio), qint64(2));
        QCOMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), qint64(2));
        QVERIFY(errors.isEmpty());
    }

    void explicitSelectionWinsOverLanguagePreferences() {
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.setLanguagePreferences({QStringLiteral("eng")}, {QStringLiteral("eng")});
        player.openFile(QString::fromUtf8(TEST_LANGUAGE_MEDIA_PATH));
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 10000);
        waitForTracks(player, 4);
        QTRY_COMPARE(selectedLanguage(player, MediaTrack::Type::Audio), QStringLiteral("eng"));
        QTRY_COMPARE(selectedLanguage(player, MediaTrack::Type::Subtitle), QStringLiteral("eng"));

        player.selectAudioTrack(1);
        player.selectSubtitleTrack(1);
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Audio), qint64(1));
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), qint64(1));
        QCOMPARE(selectedLanguage(player, MediaTrack::Type::Audio), QStringLiteral("jpn"));
        QCOMPARE(selectedLanguage(player, MediaTrack::Type::Subtitle), QStringLiteral("zho"));
        QVERIFY(errors.isEmpty());
    }

    void emptyPreferencesKeepMpvDefaultSelection() {
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.setLanguagePreferences({}, {});
        player.openFile(QString::fromUtf8(TEST_LANGUAGE_MEDIA_PATH));
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 10000);
        waitForTracks(player, 4);

        // The container marks the Chinese subtitle default; the first audio track wins.
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Audio), qint64(1));
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), qint64(1));
        QCOMPARE(selectedLanguage(player, MediaTrack::Type::Audio), QStringLiteral("jpn"));
        QCOMPARE(selectedLanguage(player, MediaTrack::Type::Subtitle), QStringLiteral("zho"));
        QVERIFY(errors.isEmpty());
    }

    void autoloadExternalSubtitlesFollowsIntent_data() {
        QTest::addColumn<bool>("enabled");
        QTest::newRow("enabled") << true;
        QTest::newRow("disabled") << false;
    }

    void autoloadExternalSubtitlesFollowsIntent() {
        QFETCH(bool, enabled);
        QTemporaryDir temporary;
        QVERIFY(prepareSiblingMedia(temporary));

        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.setAutoLoadExternalSubtitles(enabled);
        player.openFile(temporary.filePath(QStringLiteral("movie.mkv")));
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 10000);
        waitForTracks(player, enabled ? 5 : 4);
        if (enabled) {
            QTRY_COMPARE(externalSubtitleCount(player), 1);
            QVERIFY(externalSubtitleId(player) > 0);
        } else {
            QCOMPARE(externalSubtitleCount(player), 0);
        }
        QVERIFY(errors.isEmpty());
    }

    void autoLoadedSiblingIsExplicitOnlyAfterSelection() {
        QTemporaryDir temporary;
        QVERIFY(prepareSiblingMedia(temporary));
        const auto media = temporary.filePath(QStringLiteral("movie.mkv"));
        const auto sibling = temporary.filePath(QStringLiteral("movie.srt"));

        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.setAutoLoadExternalSubtitles(true);
        player.openFile(media);
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 10000);
        waitForTracks(player, 5);
        QTRY_COMPARE(externalSubtitleCount(player), 1);

        const auto id = externalSubtitleId(player);
        QVERIFY(id > 0);
        const auto path = externalSubtitlePath(player);
        QVERIFY(!path.isEmpty());
        QVERIFY(!player.isExplicitExternalPath(sibling));
        QVERIFY(!player.isExplicitExternalPath(path));

        player.selectSubtitleTrack(id);
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), id);
        QVERIFY(player.isExplicitExternalPath(sibling));
        QVERIFY(player.isExplicitExternalPath(path));
        QVERIFY(errors.isEmpty());
    }

    void defaultDoesNotAutoloadSiblingSubtitles() {
        QTemporaryDir temporary;
        QVERIFY(prepareSiblingMedia(temporary));

        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(temporary.filePath(QStringLiteral("movie.mkv")));
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 10000);
        waitForTracks(player, 4);
        QCOMPARE(externalSubtitleCount(player), 0);
        QVERIFY(errors.isEmpty());
    }

    void dirtyLanguageTokensSelectEnglish() {
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        // Leading/trailing whitespace and empty entries must be normalized away.
        player.setLanguagePreferences({QStringLiteral(" eng "), QString(), QStringLiteral("jpn")},
                                      {QStringLiteral(" eng "), QString(), QStringLiteral("zho")});
        player.openFile(QString::fromUtf8(TEST_LANGUAGE_MEDIA_PATH));
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 10000);
        waitForTracks(player, 4);
        QTRY_COMPARE(selectedLanguage(player, MediaTrack::Type::Audio), QStringLiteral("eng"));
        QTRY_COMPARE(selectedLanguage(player, MediaTrack::Type::Subtitle), QStringLiteral("eng"));
        QCOMPARE(selectedTrack(player, MediaTrack::Type::Audio), qint64(2));
        QCOMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), qint64(2));
        QVERIFY(errors.isEmpty());
    }

    void languageListCapKeepsMpvDefaultAudio() {
        QStringList audio;
        for (int index = 0; index < 16; ++index)
            audio.append(QStringLiteral("junk%1").arg(index));
        audio.append(QStringLiteral("eng"));

        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.setLanguagePreferences(audio, {});
        player.openFile(QString::fromUtf8(TEST_LANGUAGE_MEDIA_PATH));
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 10000);
        waitForTracks(player, 4);

        // eng is the 17th token and exceeds the cap, so mpv keeps its default.
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Audio), qint64(1));
        QCOMPARE(selectedLanguage(player, MediaTrack::Type::Audio), QStringLiteral("jpn"));
        QVERIFY(errors.isEmpty());
    }

    void emptyPreferencesResetSelectionOnReopen() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        const auto path = QString::fromUtf8(TEST_LANGUAGE_MEDIA_PATH);
        player.setLanguagePreferences({QStringLiteral("eng")}, {QStringLiteral("eng")});
        player.openFile(path);
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 10000);
        waitForTracks(player, 4);
        QTRY_COMPARE(selectedLanguage(player, MediaTrack::Type::Audio), QStringLiteral("eng"));
        QTRY_COMPARE(selectedLanguage(player, MediaTrack::Type::Subtitle), QStringLiteral("eng"));

        player.setLanguagePreferences({}, {});
        player.openFile(path);
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 2, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 10000);
        waitForTracks(player, 4);
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Audio), qint64(1));
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), qint64(1));
        QCOMPARE(selectedLanguage(player, MediaTrack::Type::Audio), QStringLiteral("jpn"));
        QCOMPARE(selectedLanguage(player, MediaTrack::Type::Subtitle), QStringLiteral("zho"));
        QVERIFY(errors.isEmpty());
    }

    void sessionRestoreAppliesLanguagesBeforeMediaLoad() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto sessionPath = temporary.filePath(QStringLiteral("settings.ini"));
        SessionState saved;
        saved.paths = {QString::fromUtf8(TEST_LANGUAGE_MEDIA_PATH)};
        saved.currentIndex = 0;
        saved.languages.audioLanguages = {QStringLiteral("eng")};
        saved.languages.subtitleLanguages = {QStringLiteral("eng")};
        saved.languages.autoLoadExternalSubtitles = false;
        QVERIFY(SessionStore(sessionPath).save(saved).isEmpty());

        MpvController player(true, true);
        PlaylistController playlist(player);
        SessionManager session(player, playlist, sessionPath);
        QVERIFY(session.restore().error.isEmpty());
        QCOMPARE(session.languagePreferences().audioLanguages, saved.languages.audioLanguages);
        QCOMPARE(session.languagePreferences().subtitleLanguages,
                 saved.languages.subtitleLanguages);

        playlist.playAt(0);
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 10000);
        waitForTracks(player, 4);
        QTRY_COMPARE(selectedLanguage(player, MediaTrack::Type::Audio), QStringLiteral("eng"));
        QTRY_COMPARE(selectedLanguage(player, MediaTrack::Type::Subtitle), QStringLiteral("eng"));
    }

    void sessionSetterAppliesPreferencesImmediately() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto sessionPath = temporary.filePath(QStringLiteral("settings.ini"));
        SessionState saved;
        saved.paths = {QString::fromUtf8(TEST_LANGUAGE_MEDIA_PATH)};
        saved.currentIndex = 0;
        QVERIFY(SessionStore(sessionPath).save(saved).isEmpty());

        MpvController player(true, true);
        PlaylistController playlist(player);
        SessionManager session(player, playlist, sessionPath);
        QVERIFY(session.restore().error.isEmpty());

        LanguagePreferences preferences;
        preferences.audioLanguages = {QStringLiteral("eng")};
        preferences.subtitleLanguages = {QStringLiteral("eng")};
        session.setLanguagePreferences(preferences);
        QCOMPARE(session.languagePreferences().audioLanguages, preferences.audioLanguages);
        QCOMPARE(session.languagePreferences().subtitleLanguages, preferences.subtitleLanguages);

        playlist.playAt(0);
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 10000);
        waitForTracks(player, 4);
        QTRY_COMPARE(selectedLanguage(player, MediaTrack::Type::Audio), QStringLiteral("eng"));
        QTRY_COMPARE(selectedLanguage(player, MediaTrack::Type::Subtitle), QStringLiteral("eng"));
    }

    void autoLoadedSiblingIsNotPersistedAsExplicitMemory() {
        QTemporaryDir temporary;
        QVERIFY(prepareSiblingMedia(temporary));
        const auto sessionPath = temporary.filePath(QStringLiteral("settings.ini"));
        const auto media = temporary.filePath(QStringLiteral("movie.mkv"));
        const auto sibling = temporary.filePath(QStringLiteral("movie.srt"));
        {
            MpvController player(true, true);
            PlaylistController playlist(player);
            SessionManager session(player, playlist, sessionPath);
            QVERIFY(session.restore().error.isEmpty());
            LanguagePreferences preferences;
            preferences.autoLoadExternalSubtitles = true;
            session.setLanguagePreferences(preferences);
            playlist.restoreFiles({media}, 0);
            playlist.playOrPause();
            QTRY_VERIFY_WITH_TIMEOUT(player.state().active && player.tracks().size() == 5, 10000);
            player.setPaused(true);
            QTRY_COMPARE_WITH_TIMEOUT(externalSubtitleCount(player), 1, 5000);
            QVERIFY(!player.isExplicitExternalPath(sibling));
            QVERIFY(session.save({}).isEmpty());
        }

        const TrackPreferenceStore store(sessionPath + QStringLiteral(".tracks.ini"));
        const auto saved = store.load();
        QVERIFY2(saved.error.isEmpty(), qPrintable(saved.error));
        QCOMPARE(saved.entries.size(), 1);
        QVERIFY(saved.entries.front().selection.externalSubtitlePaths.isEmpty());
        QVERIFY(saved.entries.front().selection.selectedExternalSubtitlePath.isEmpty());

        // A later session with auto-load off must not resurrect the discovered sibling.
        MpvController player(true, true);
        PlaylistController playlist(player);
        SessionManager session(player, playlist, sessionPath);
        QVERIFY(session.restore().error.isEmpty());
        session.setLanguagePreferences({});
        QVERIFY(!session.languagePreferences().autoLoadExternalSubtitles);
        QSignalSpy restored(&player, &MpvController::trackPreferenceRestoreFinished);
        playlist.playAt(0);
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(restored.count(), 1, 10000);
        waitForTracks(player, 4);
        QCOMPARE(externalSubtitleCount(player), 0);
    }

    void restoringSavedPathAlreadyAutoLoadedKeepsOriginWithoutDuplicate() {
        QTemporaryDir temporary;
        QVERIFY(prepareSiblingMedia(temporary));
        const auto media = temporary.filePath(QStringLiteral("movie.mkv"));
        const auto sibling = temporary.filePath(QStringLiteral("movie.srt"));
        const auto canonicalSibling = QFileInfo(sibling).canonicalFilePath();
        QVERIFY(!canonicalSibling.isEmpty());

        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.setAutoLoadExternalSubtitles(true);
        player.openFile(media);
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 10000);
        waitForTracks(player, 5);
        QTRY_COMPARE(externalSubtitleCount(player), 1);
        const auto externalId = externalSubtitleId(player);
        QVERIFY(externalId > 0);

        TrackSelectionPreference preference;
        preference.externalSubtitlePaths = {canonicalSibling};
        preference.selectedExternalSubtitlePath = canonicalSibling;
        preference.selectedExternalSubtitleStream = 0;
        QSignalSpy restored(&player, &MpvController::trackPreferenceRestoreFinished);
        player.restoreTrackPreference(preference);
        QTRY_COMPARE_WITH_TIMEOUT(restored.count(), 1, 10000);

        // The saved path was already auto-loaded: keep its explicit origin and
        // dedupe the restore instead of adding a second external stream.
        QVERIFY(player.isExplicitExternalPath(sibling));
        QVERIFY(player.isExplicitExternalPath(canonicalSibling));
        QCOMPARE(externalSubtitleCount(player), 1);
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), externalId);
        QVERIFY(errors.isEmpty());
    }

    void restoredAutoLoadedSiblingKeepsExplicitOrigin() {
        QTemporaryDir temporary;
        QVERIFY(prepareSiblingMedia(temporary));
        const auto sessionPath = temporary.filePath(QStringLiteral("settings.ini"));
        const auto media = temporary.filePath(QStringLiteral("movie.mkv"));
        const auto sibling = temporary.filePath(QStringLiteral("movie.srt"));
        const auto canonicalSibling = QFileInfo(sibling).canonicalFilePath();
        QVERIFY(!canonicalSibling.isEmpty());

        {
            MpvController player(true, true);
            PlaylistController playlist(player);
            SessionManager session(player, playlist, sessionPath);
            QVERIFY(session.restore().error.isEmpty());
            LanguagePreferences preferences;
            preferences.autoLoadExternalSubtitles = true;
            session.setLanguagePreferences(preferences);
            playlist.restoreFiles({media}, 0);
            playlist.playOrPause();
            QTRY_VERIFY_WITH_TIMEOUT(player.state().active && player.tracks().size() == 5, 10000);
            player.setPaused(true);
            QTRY_COMPARE_WITH_TIMEOUT(externalSubtitleCount(player), 1, 5000);
            const auto id = externalSubtitleId(player);
            QVERIFY(id > 0);
            player.selectSubtitleTrack(id);
            QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), id);
            QVERIFY(session.save({}).isEmpty());
        }

        {
            const auto saved =
                TrackPreferenceStore(sessionPath + QStringLiteral(".tracks.ini")).load();
            QVERIFY2(saved.error.isEmpty(), qPrintable(saved.error));
            QCOMPARE(saved.entries.size(), 1);
            QCOMPARE(saved.entries.front().selection.externalSubtitlePaths,
                     QStringList{canonicalSibling});
            QCOMPARE(saved.entries.front().selection.selectedExternalSubtitlePath,
                     canonicalSibling);
        }

        {
            MpvController player(true, true);
            PlaylistController playlist(player);
            SessionManager session(player, playlist, sessionPath);
            QVERIFY(session.restore().error.isEmpty());
            QVERIFY(session.languagePreferences().autoLoadExternalSubtitles);
            QSignalSpy restored(&player, &MpvController::trackPreferenceRestoreFinished);
            playlist.playAt(0);
            QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 10000);
            player.setPaused(true);
            QTRY_VERIFY_WITH_TIMEOUT(player.state().paused, 5000);
            QTRY_COMPARE_WITH_TIMEOUT(restored.count(), 1, 10000);
            QTRY_COMPARE_WITH_TIMEOUT(externalSubtitleCount(player), 1, 10000);
            QVERIFY(player.isExplicitExternalPath(sibling));
            QVERIFY(player.isExplicitExternalPath(canonicalSibling));
            QVERIFY(session.save({}).isEmpty());
        }

        {
            const auto saved =
                TrackPreferenceStore(sessionPath + QStringLiteral(".tracks.ini")).load();
            QVERIFY2(saved.error.isEmpty(), qPrintable(saved.error));
            QCOMPARE(saved.entries.size(), 1);
            QCOMPARE(saved.entries.front().selection.externalSubtitlePaths,
                     QStringList{canonicalSibling});
            QCOMPARE(saved.entries.front().selection.selectedExternalSubtitlePath,
                     canonicalSibling);
        }
    }

    void recoveryKeepsExplicitExternalSubtitleWithoutDuplicates() {
        QTemporaryDir temporary;
        QVERIFY(prepareSiblingMedia(temporary));

        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.setAutoLoadExternalSubtitles(true);
        player.openFile(temporary.filePath(QStringLiteral("movie.mkv")));
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 10000);
        waitForTracks(player, 5);
        QTRY_COMPARE(externalSubtitleCount(player), 1);
        const auto externalId = externalSubtitleId(player);
        QVERIFY(externalId > 0);
        player.selectSubtitleTrack(externalId);
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), externalId);

        player.prepareForRendererReset();
        player.resumeAfterRendererReset();
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 2, 10000);
        waitForTracks(player, 5);
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), externalId);
        QTRY_COMPARE(externalSubtitleCount(player), 1);
        QVERIFY(errors.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TrackLanguageTest)
#include "track_language_test.moc"
