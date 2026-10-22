#include "playback/mpvcontroller.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace {
QString mediaPath;
QString externalPath;
QString silentMediaPath;

QList<MediaTrack> externalAudio(const MpvController &player) {
    QList<MediaTrack> result;
    for (const auto &track : player.tracks()) {
        if (track.type == MediaTrack::Type::Audio && track.external)
            result.append(track);
    }
    return result;
}

QList<MediaTrack> externalSubtitles(const MpvController &player) {
    QList<MediaTrack> result;
    for (const auto &track : player.tracks()) {
        if (track.type == MediaTrack::Type::Subtitle && track.external)
            result.append(track);
    }
    return result;
}

int audioCount(const MpvController &player) {
    int count = 0;
    for (const auto &track : player.tracks()) {
        if (track.type == MediaTrack::Type::Audio)
            ++count;
    }
    return count;
}

int selectedExternalOrdinal(const MpvController &player) {
    const auto tracks = externalAudio(player);
    for (qsizetype ordinal = 0; ordinal < tracks.size(); ++ordinal) {
        if (tracks.at(ordinal).selected)
            return static_cast<int>(ordinal);
    }
    return -1;
}

QString backendAudio(MpvController &player) {
    char *value = mpv_get_property_string(player.handle(), "aid");
    const auto text = QString::fromUtf8(value);
    mpv_free(value);
    return text;
}
} // namespace

class ExternalAudioTest final : public QObject {
    Q_OBJECT
  private slots:
    void initTestCase() {
        QVERIFY(QFileInfo::exists(mediaPath));
        QVERIFY(QFileInfo::exists(externalPath));
        QVERIFY(QFileInfo::exists(silentMediaPath));
        mediaPath = QFileInfo(mediaPath).canonicalFilePath();
        externalPath = QFileInfo(externalPath).canonicalFilePath();
    }

    void addsAndSelectsExternalAudioWithoutReloading() {
        MpvController player(true, true);
        QVERIFY2(player.isReady(), qPrintable(player.errorString()));
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy requested(&player, &MpvController::audioLoadRequested);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(mediaPath);
        QTRY_VERIFY(player.state().active && audioCount(player) == 2);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);

        player.addAudio(externalPath);
        QTRY_COMPARE(externalAudio(player).size(), 2);
        QTRY_VERIFY(selectedExternalOrdinal(player) >= 0);
        QCOMPARE(audioCount(player), 4);
        QCOMPARE(requested.count(), 1);
        QCOMPARE(loaded.count(), 1);
        for (const auto &track : externalAudio(player)) {
            QCOMPARE(track.externalPath, externalPath);
            QVERIFY(!track.codec.isEmpty());
        }
        const auto second = externalAudio(player).at(1).id;
        player.selectAudioTrack(second);
        QTRY_COMPARE(selectedExternalOrdinal(player), 1);
        QTRY_COMPARE(backendAudio(player).toLongLong(), second);
        QVERIFY(player.state().paused);
        QVERIFY(errors.isEmpty());
    }

    void rendererResetRestoresExternalAudio_data() {
        QTest::addColumn<bool>("disabled");
        QTest::newRow("second-stream") << false;
        QTest::newRow("disabled") << true;
    }

    void rendererResetRestoresExternalAudio() {
        QFETCH(bool, disabled);
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(mediaPath);
        QTRY_VERIFY(player.state().active && audioCount(player) == 2);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.addAudio(externalPath);
        QTRY_COMPARE(externalAudio(player).size(), 2);
        player.selectAudioTrack(externalAudio(player).at(1).id);
        QTRY_COMPARE(selectedExternalOrdinal(player), 1);
        if (disabled) {
            player.selectAudioTrack(0);
            QTRY_COMPARE(backendAudio(player), QStringLiteral("no"));
            // Recovery snapshots observed tracks; a direct backend read does not
            // drain the queued property event that updates that snapshot.
            QTRY_COMPARE(selectedExternalOrdinal(player), -1);
        }

        for (int round = 0; round < 2; ++round) {
            player.prepareForRendererReset();
            player.resumeAfterRendererReset();
            QTRY_COMPARE(loaded.count(), round + 2);
            QTRY_VERIFY(!player.trackRestorationPending());
            QTRY_COMPARE(externalAudio(player).size(), 2);
            QTRY_COMPARE(audioCount(player), 4);
            QTRY_COMPARE(selectedExternalOrdinal(player), disabled ? -1 : 1);
            if (disabled)
                QTRY_COMPARE(backendAudio(player), QStringLiteral("no"));
            for (const auto &track : externalAudio(player))
                QCOMPARE(track.externalPath, externalPath);
            QVERIFY(player.state().paused);
            QVERIFY(errors.isEmpty());
        }
    }

    void missingAudioDoesNotPreventValidAddition() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(mediaPath);
        QTRY_VERIFY(player.state().active && audioCount(player) == 2);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.addAudio(temporary.filePath(QStringLiteral("不存在 missing.mka")));
        QTRY_COMPARE(errors.count(), 1);
        QVERIFY(player.state().active);
        QCOMPARE(audioCount(player), 2);
        QVERIFY(externalAudio(player).isEmpty());

        player.addAudio(externalPath);
        QTRY_COMPARE(externalAudio(player).size(), 2);
        QTRY_VERIFY(selectedExternalOrdinal(player) >= 0);
        QCOMPARE(audioCount(player), 4);
        QCOMPARE(errors.count(), 1);
    }

    void preferenceRestoresExternalAudioWithoutDuplicates_data() {
        QTest::addColumn<bool>("disabled");
        QTest::newRow("second-stream") << false;
        QTest::newRow("disabled") << true;
    }

    void preferenceRestoresExternalAudioWithoutDuplicates() {
        QFETCH(bool, disabled);
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        QSignalSpy finished(&player, &MpvController::trackPreferenceRestoreFinished);
        player.openFile(mediaPath);
        QTRY_VERIFY(player.state().active && audioCount(player) == 2);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        TrackSelectionPreference preference;
        preference.externalAudioPaths = {externalPath, externalPath};
        preference.selectedExternalAudioPath = externalPath;
        preference.selectedExternalAudioStream = 1;
        if (disabled)
            preference.audioTrackId = 0;

        bool stateConfirmedAtCompletion = false;
        connect(&player, &MpvController::trackPreferenceRestoreFinished, &player,
                [&](bool canceled) {
                    stateConfirmedAtCompletion =
                        !canceled && audioCount(player) == 4 && externalAudio(player).size() == 2 &&
                        selectedExternalOrdinal(player) == (disabled ? -1 : 1) &&
                        (!disabled || backendAudio(player) == QStringLiteral("no"));
                });
        for (int round = 0; round < 2; ++round) {
            stateConfirmedAtCompletion = false;
            player.restoreTrackPreference(preference);
            QTRY_COMPARE(finished.count(), round + 1);
            QVERIFY(!finished.back().front().toBool());
            QVERIFY(stateConfirmedAtCompletion);
            QVERIFY(!player.trackRestorationPending());
            QCOMPARE(externalAudio(player).size(), 2);
            for (const auto &track : externalAudio(player))
                QCOMPARE(track.externalPath, externalPath);
            QVERIFY(errors.isEmpty());
        }
    }

    void selectedPreferencePathAloneLoadsSecondStream() {
        MpvController player(true, true);
        QSignalSpy finished(&player, &MpvController::trackPreferenceRestoreFinished);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(mediaPath);
        QTRY_VERIFY(player.state().active && audioCount(player) == 2);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        TrackSelectionPreference preference;
        preference.selectedExternalAudioPath = externalPath;
        preference.selectedExternalAudioStream = 1;
        player.restoreTrackPreference(preference);
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(!finished.front().front().toBool());
        QCOMPARE(externalAudio(player).size(), 2);
        QCOMPARE(selectedExternalOrdinal(player), 1);
        QVERIFY(errors.isEmpty());
    }

    void mixedContainerRecoveryLoadsEachPathOnce() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(silentMediaPath);
        QTRY_VERIFY(player.state().active && loaded.count() == 1);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.addAudio(mediaPath);
        QTRY_COMPARE(externalAudio(player).size(), 2);
        QTRY_COMPARE(externalSubtitles(player).size(), 1);
        player.selectAudioTrack(externalAudio(player).at(1).id);
        player.selectSubtitleTrack(externalSubtitles(player).front().id);
        QTRY_COMPARE(selectedExternalOrdinal(player), 1);
        QTRY_VERIFY(externalSubtitles(player).front().selected);

        for (int round = 0; round < 2; ++round) {
            player.prepareForRendererReset();
            player.resumeAfterRendererReset();
            QTRY_COMPARE(loaded.count(), round + 2);
            QTRY_VERIFY(!player.trackRestorationPending());
            QTRY_COMPARE(externalAudio(player).size(), 2);
            QTRY_COMPARE(externalSubtitles(player).size(), 1);
            QTRY_COMPARE(selectedExternalOrdinal(player), 1);
            QTRY_VERIFY(externalSubtitles(player).front().selected);
            QCOMPARE(player.tracks().size(), 3);
            for (const auto &track : player.tracks()) {
                QVERIFY(track.external);
                QCOMPARE(track.externalPath, mediaPath);
            }
            QVERIFY(player.state().paused);
            QVERIFY(errors.isEmpty());
        }
    }

    void mixedContainerPreferenceLoadsEachPathOnceAfterReopen() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy finished(&player, &MpvController::trackPreferenceRestoreFinished);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(silentMediaPath);
        QTRY_VERIFY(player.state().active && loaded.count() == 1);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.addAudio(mediaPath);
        QTRY_COMPARE(externalAudio(player).size(), 2);
        QTRY_COMPARE(externalSubtitles(player).size(), 1);

        player.openFile(silentMediaPath);
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_VERIFY(player.state().active && player.tracks().isEmpty());
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        TrackSelectionPreference preference;
        preference.externalAudioPaths = {mediaPath};
        preference.externalSubtitlePaths = {mediaPath};
        preference.selectedExternalAudioPath = mediaPath;
        preference.selectedExternalAudioStream = 1;
        preference.selectedExternalSubtitlePath = mediaPath;
        preference.selectedExternalSubtitleStream = 0;
        for (int round = 0; round < 2; ++round) {
            player.restoreTrackPreference(preference);
            QTRY_COMPARE(finished.count(), round + 1);
            QVERIFY(!finished.back().front().toBool());
            QCOMPARE(externalAudio(player).size(), 2);
            QCOMPARE(externalSubtitles(player).size(), 1);
            QCOMPARE(selectedExternalOrdinal(player), 1);
            QVERIFY(externalSubtitles(player).front().selected);
            QCOMPARE(player.tracks().size(), 3);
            for (const auto &track : player.tracks())
                QCOMPARE(track.externalPath, mediaPath);
            QVERIFY(errors.isEmpty());
        }
    }

    void addsAudioToVideoWithoutInternalAudio() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(silentMediaPath);
        QTRY_VERIFY(player.state().active);
        QTRY_COMPARE(loaded.count(), 1);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QCOMPARE(audioCount(player), 0);
        player.addAudio(externalPath);
        QTRY_COMPARE(externalAudio(player).size(), 2);
        QTRY_VERIFY(selectedExternalOrdinal(player) >= 0);
        QCOMPARE(audioCount(player), 2);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(player.state().active);
        QVERIFY(errors.isEmpty());
    }
};

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    auto arguments = application.arguments();
    if (arguments.size() < 4) {
        qCritical("Usage: external_audio_test <media> <two-stream-audio> <silent-video> [QtTest "
                  "options]");
        return 1;
    }
    mediaPath = arguments.takeAt(1);
    externalPath = arguments.takeAt(1);
    silentMediaPath = arguments.takeAt(1);
    ExternalAudioTest test;
    return QTest::qExec(&test, arguments);
}

#include "external_audio_test.moc"
