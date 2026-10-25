#include "playback/mediatrack.h"
#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "session/sessionmanager.h"
#include "session/sessionstate.h"
#include "session/sessionstore.h"

#include <QDir>
#include <QFile>
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
