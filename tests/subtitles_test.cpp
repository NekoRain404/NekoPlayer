#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/trackselectionpreference.h"
#include "session/sessionmanager.h"
#include "session/sessionstore.h"

#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
#include <limits>

namespace {
QString backendText(MpvController &player, const char *property) {
    char *value = mpv_get_property_string(player.handle(), property);
    const auto text = QString::fromUtf8(value);
    mpv_free(value);
    return text;
}
} // namespace

class SubtitlesTest final : public QObject {
    Q_OBJECT
  private slots:
    void hidingSubtitlesPreservesSelectionAndPause() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.selectSubtitleTrack(1);
        QTRY_COMPARE(backendText(player, "sid"), QStringLiteral("1"));
        QTRY_VERIFY(std::any_of(
            player.tracks().cbegin(), player.tracks().cend(), [](const MediaTrack &track) {
                return track.type == MediaTrack::Type::Subtitle && track.id == 1 && track.selected;
            }));
        const auto selected = player.tracks();
        player.setSubtitleVisible(false);
        QTRY_COMPARE(backendText(player, "sub-visibility"), QStringLiteral("no"));
        QCOMPARE(player.tracks(), selected);
        QVERIFY(player.state().active && player.state().paused);
        player.setSubtitleVisible(true);
        QTRY_COMPARE(backendText(player, "sub-visibility"), QStringLiteral("yes"));
        QCOMPARE(backendText(player, "sid"), QStringLiteral("1"));
        QCOMPARE(loaded.count(), 1);
    }

    void rapidAdjustmentsUseLatestIntent() {
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.setSubtitleScale(1.5);
        for (int step = 0; step < 5; ++step)
            player.adjustSubtitleScale(0.1);
        player.setSubtitlePosition(80);
        for (int step = 0; step < 3; ++step)
            player.adjustSubtitlePosition(-2);
        player.setSubtitleFont(QStringLiteral("Noto Sans CJK SC"));
        QCOMPARE(player.subtitlePreferences().scale, 2.0);
        QCOMPARE(player.subtitlePreferences().position, 74.0);
        QTRY_COMPARE(backendText(player, "sub-scale").toDouble(), 2.0);
        QTRY_COMPARE(backendText(player, "sub-pos").toDouble(), 74.0);
        QTRY_COMPARE(backendText(player, "sub-font"), QStringLiteral("Noto Sans CJK SC"));
        QCOMPARE(backendText(player, "sub-ass-override"), QStringLiteral("scale"));
        QVERIFY(errors.isEmpty());
    }

    void invalidRequestsStayWithinDisplayLimits() {
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.setSubtitleScale(200);
        player.setSubtitlePosition(-200);
        QTRY_COMPARE(backendText(player, "sub-scale").toDouble(), 4.0);
        QTRY_COMPARE(backendText(player, "sub-pos").toDouble(), 0.0);
        player.setSubtitleScale(std::numeric_limits<double>::quiet_NaN());
        player.setSubtitlePosition(std::numeric_limits<double>::infinity());
        QCOMPARE(player.subtitlePreferences().scale, 4.0);
        QCOMPARE(player.subtitlePreferences().position, 0.0);
        player.setSubtitleFont(QStringLiteral("Noto Sans"));
        QTRY_COMPARE(backendText(player, "sub-font"), QStringLiteral("Noto Sans"));
        player.setSubtitleFont(QStringLiteral("invalid\nfont"));
        QTRY_COMPARE(backendText(player, "sub-font"), QStringLiteral("sans-serif"));
        player.setSubtitleFont(QStringLiteral("nul") + QChar::Null + QStringLiteral("font"));
        QCOMPARE(player.subtitlePreferences().font, QStringLiteral("sans-serif"));
        player.setSubtitleScale(-200);
        player.setSubtitlePosition(200);
        QTRY_COMPARE(backendText(player, "sub-scale").toDouble(), 0.25);
        QTRY_COMPARE(backendText(player, "sub-pos").toDouble(), 100.0);
        QVERIFY(errors.isEmpty());
    }

    void immediateSaveAndIdleRestoreKeepLatestPreferences() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        {
            MpvController player(true, true);
            PlaylistController playlist(player);
            SessionManager session(player, playlist, path);
            QVERIFY(session.restore().error.isEmpty());
            player.setSubtitleVisible(false);
            player.setSubtitleScale(1.8);
            player.adjustSubtitleScale(0.2);
            player.setSubtitlePosition(86);
            player.setSubtitleFont(QStringLiteral("Noto Sans CJK SC"));
            // Save before processing asynchronous mpv events.
            QVERIFY(session.save({}).isEmpty());
            const auto saved = SessionStore(path).load().state.subtitles;
            QVERIFY(!saved.visible);
            QCOMPARE(saved.scale, 2.0);
            QCOMPARE(saved.position, 86.0);
            QCOMPARE(saved.font, QStringLiteral("Noto Sans CJK SC"));
        }
        MpvController player(true, true);
        PlaylistController playlist(player);
        SessionManager session(player, playlist, path);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QVERIFY(session.restore().error.isEmpty());
        QVERIFY(session.save({}).isEmpty());
        QTRY_COMPARE(backendText(player, "sub-visibility"), QStringLiteral("no"));
        QTRY_COMPARE(backendText(player, "sub-scale").toDouble(), 2.0);
        QTRY_COMPARE(backendText(player, "sub-pos").toDouble(), 86.0);
        QTRY_COMPARE(backendText(player, "sub-font"), QStringLiteral("Noto Sans CJK SC"));
        QVERIFY(!player.state().active && loaded.isEmpty());
    }

    void preferencesSurviveStopAndRendererRecovery() {
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.setSubtitlePreferences({false, 1.5, 90, QStringLiteral("Noto Sans")});
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        player.prepareForRendererReset();
        player.resumeAfterRendererReset();
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(player.state().active && player.state().paused);
        QCOMPARE(backendText(player, "sub-visibility"), QStringLiteral("no"));
        QCOMPARE(backendText(player, "sub-scale").toDouble(), 1.5);
        QCOMPARE(backendText(player, "sub-pos").toDouble(), 90.0);
        QCOMPARE(backendText(player, "sub-font"), QStringLiteral("Noto Sans"));
        player.stop();
        QTRY_VERIFY(!player.state().active);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active);
        QCOMPARE(backendText(player, "sub-visibility"), QStringLiteral("no"));
        QCOMPARE(backendText(player, "sub-scale").toDouble(), 1.5);
        QCOMPARE(backendText(player, "sub-pos").toDouble(), 90.0);
        QCOMPARE(backendText(player, "sub-font"), QStringLiteral("Noto Sans"));
        QVERIFY(errors.isEmpty());
    }

    void restoresAudioAndExternalSubtitleSelectionAsynchronously() {
        MpvController player(true, true);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        const auto audio =
            std::find_if(player.tracks().cbegin(), player.tracks().cend(), [](const auto &track) {
                return track.type == MediaTrack::Type::Audio && !track.selected;
            });
        QVERIFY(audio != player.tracks().cend());
        const auto audioId = audio->id;

        const auto subtitle = QString::fromUtf8(TEST_EXTERNAL_SUBTITLE_PATH);
        TrackSelectionPreference preference;
        preference.audioTrackId = audioId;
        preference.externalSubtitlePaths = {subtitle};
        preference.selectedExternalSubtitlePath = subtitle;
        preference.selectedExternalSubtitleStream = 0;
        bool confirmedAtCompletion = false;
        connect(&player, &MpvController::trackPreferenceRestoreFinished, &player,
                [&](bool canceled) {
                    confirmedAtCompletion =
                        !canceled &&
                        std::any_of(player.tracks().cbegin(), player.tracks().cend(),
                                    [audioId](const auto &track) {
                                        return track.type == MediaTrack::Type::Audio &&
                                               track.id == audioId && track.selected;
                                    }) &&
                        std::any_of(player.tracks().cbegin(), player.tracks().cend(),
                                    [&](const auto &track) {
                                        return track.externalPath == subtitle && track.selected;
                                    });
                });
        player.restoreTrackPreference(preference);

        QTRY_COMPARE(backendText(player, "aid").toLongLong(), audioId);
        QTRY_VERIFY(
            std::any_of(player.tracks().cbegin(), player.tracks().cend(), [&](const auto &track) {
                return track.type == MediaTrack::Type::Subtitle && track.external &&
                       track.externalPath == subtitle && track.selected;
            }));
        QTRY_VERIFY(confirmedAtCompletion);
    }

    void brokenExternalSubtitleDoesNotBlockOtherRestoration() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto brokenPath = temporary.filePath(QStringLiteral("broken.srt"));
        QFile broken(brokenPath);
        QVERIFY(broken.open(QIODevice::WriteOnly));
        QVERIFY(broken.write("not a subtitle") > 0);
        broken.close();
        MpvController player(true, true);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        player.setPaused(true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        QSignalSpy finished(&player, &MpvController::trackPreferenceRestoreFinished);
        TrackSelectionPreference preference;
        preference.audioTrackId = 2;
        preference.externalSubtitlePaths = {brokenPath,
                                            QString::fromUtf8(TEST_EXTERNAL_SUBTITLE_PATH)};
        preference.selectedExternalSubtitlePath = preference.externalSubtitlePaths.back();
        preference.selectedExternalSubtitleStream = 0;
        player.restoreTrackPreference(preference);
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(!finished.front().front().toBool());
        QVERIFY(!errors.isEmpty());
        QCOMPARE(backendText(player, "aid"), QStringLiteral("2"));
        QCOMPARE(backendText(player, "sid"), QStringLiteral("2"));
        QCOMPARE(player.tracks().size(), 4);
    }

    void restoresExplicitlyDisabledSubtitleSelection() {
        MpvController player(true, true);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active);
        TrackSelectionPreference preference;
        preference.subtitleTrackId = 0;
        player.restoreTrackPreference(preference);
        QTRY_COMPARE(backendText(player, "sid"), QStringLiteral("no"));
    }

    void restoresSecondStreamFromOneExternalFile() {
        MpvController player(true, true);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        player.setPaused(true);
        QSignalSpy finished(&player, &MpvController::trackPreferenceRestoreFinished);
        TrackSelectionPreference preference;
        preference.selectedExternalSubtitlePath = QString::fromUtf8(TEST_MULTI_SUBTITLE_PATH);
        preference.selectedExternalSubtitleStream = 1;
        player.restoreTrackPreference(preference);
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(!finished.front().front().toBool());
        QCOMPARE(player.tracks().size(), 5);
        QCOMPARE(backendText(player, "sid"), QStringLiteral("3"));
        QVERIFY(player.tracks().back().external && player.tracks().back().selected);
    }

    void explicitUserChoiceCancelsPendingRestore() {
        MpvController player(true, true);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        QSignalSpy finished(&player, &MpvController::trackPreferenceRestoreFinished);
        TrackSelectionPreference preference;
        preference.audioTrackId = 2;
        preference.subtitleTrackId = 1;
        player.restoreTrackPreference(preference);
        player.selectSubtitleTrack(0);
        QTRY_COMPARE(backendText(player, "sid"), QStringLiteral("no"));
        QTRY_COMPARE(backendText(player, "aid"), QStringLiteral("1"));
        QCOMPARE(finished.count(), 1);
        QVERIFY(finished.front().front().toBool());
    }

    void pendingOpenRejectsPreferenceRestore() {
        MpvController player(true, true);
        const auto media = QString::fromUtf8(TEST_TRACK_MEDIA_PATH);
        player.openFile(media);
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        player.openFile(media);
        QVERIFY(!player.state().active);
        QSignalSpy finished(&player, &MpvController::trackPreferenceRestoreFinished);
        TrackSelectionPreference preference;
        preference.audioTrackId = 2;
        player.restoreTrackPreference(preference);
        QCOMPARE(finished.count(), 1);
        QVERIFY(finished.front().front().toBool());
    }

    void reentrantOpenCancelsRestoreBeforeCompletion() {
        MpvController player(true, true);
        const auto media = QString::fromUtf8(TEST_TRACK_MEDIA_PATH);
        player.openFile(media);
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        QSignalSpy finished(&player, &MpvController::trackPreferenceRestoreFinished);
        bool replaced = false;
        connect(&player, &MpvController::tracksChanged, &player, [&] {
            if (!replaced && std::any_of(player.tracks().cbegin(), player.tracks().cend(),
                                         [](const auto &track) {
                                             return track.type == MediaTrack::Type::Audio &&
                                                    track.id == 2 && track.selected;
                                         })) {
                replaced = true;
                player.openFile(media);
            }
        });
        TrackSelectionPreference preference;
        preference.audioTrackId = 2;
        player.restoreTrackPreference(preference);
        QTRY_VERIFY(replaced);
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(finished.front().front().toBool());
        QTRY_VERIFY(player.state().active);
        QTRY_COMPARE(backendText(player, "aid"), QStringLiteral("1"));
    }

    void canceledRestoreCallbackCanOpenAnotherMediaDuringStop() {
        MpvController player(true, true);
        const auto media = QString::fromUtf8(TEST_TRACK_MEDIA_PATH);
        player.openFile(media);
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        bool reopened = false;
        connect(&player, &MpvController::trackPreferenceRestoreFinished, &player,
                [&](bool canceled) {
                    if (canceled && !reopened) {
                        reopened = true;
                        player.openFile(media);
                    }
                });
        TrackSelectionPreference preference;
        preference.audioTrackId = 2;
        player.restoreTrackPreference(preference);
        player.stop();
        QVERIFY(reopened);
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(player.state().active);
    }

    void missingExternalSubtitleDoesNotBlockRestore() {
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active);
        TrackSelectionPreference preference;
        preference.externalSubtitlePaths = {QStringLiteral("/tmp/neko-missing-subtitle.srt")};
        preference.selectedExternalSubtitlePath = preference.externalSubtitlePaths.front();
        preference.selectedExternalSubtitleStream = 0;
        player.restoreTrackPreference(preference);
        QTRY_VERIFY(!errors.isEmpty());
        QVERIFY(player.state().active);
        QVERIFY(std::none_of(player.tracks().cbegin(), player.tracks().cend(),
                             [](const auto &track) { return track.external; }));
    }

    void oldAndMalformedConfigurationUsesIndependentDefaults() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        SessionStore store(path);
        QVERIFY(store.save({}).isEmpty());
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.remove(QStringLiteral("subtitles"));
            settings.sync();
        }
        const auto old = store.load().state.subtitles;
        QVERIFY(old.visible);
        QCOMPARE(old.scale, 1.0);
        QCOMPARE(old.position, 100.0);
        QCOMPARE(old.font, QStringLiteral("sans-serif"));
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.setValue(QStringLiteral("subtitles/visible"), QStringLiteral("invalid"));
            settings.setValue(QStringLiteral("subtitles/scale"), QStringLiteral("nan"));
            settings.setValue(QStringLiteral("subtitles/position"), QStringLiteral("invalid"));
            settings.setValue(QStringLiteral("subtitles/font"), QString(257, QLatin1Char('a')));
            settings.sync();
        }
        const auto malformed = store.load().state.subtitles;
        QCOMPARE(malformed, old);
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.setValue(QStringLiteral("subtitles/visible"), false);
            settings.setValue(QStringLiteral("subtitles/scale"), 1.75);
            settings.setValue(QStringLiteral("subtitles/position"), -50);
            settings.setValue(QStringLiteral("subtitles/font"), QStringLiteral("  Noto Sans  "));
            settings.sync();
        }
        const auto mixed = store.load().state.subtitles;
        QVERIFY(!mixed.visible);
        QCOMPARE(mixed.scale, 1.75);
        QCOMPARE(mixed.position, 0.0);
        QCOMPARE(mixed.font, QStringLiteral("Noto Sans"));
    }
};

QTEST_GUILESS_MAIN(SubtitlesTest)
#include "subtitles_test.moc"
