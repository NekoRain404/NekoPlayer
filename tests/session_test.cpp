#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"
#include "session/sessionmanager.h"
#include "session/sessionstore.h"

#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class SessionTest final : public QObject {
    Q_OBJECT
  private slots:
    void restoredSessionRemainsIdleAndHonorsLaterChanges() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        SessionState saved;
        saved.paths = {temporary.filePath(QStringLiteral("missing.mkv")), media, media};
        saved.currentIndex = 1;
        saved.audio = {23.0, true};
        QVERIFY(SessionStore(file).save(saved).isEmpty());
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QVERIFY(session.restore().error.isEmpty());
        QCOMPARE(queue.model()->rowCount(), 3);
        QCOMPARE(queue.model()->currentIndex(), 1);
        // Saving before the first mpv observation must preserve loaded preferences.
        QVERIFY(session.save({}).isEmpty());
        QCOMPARE(SessionStore(file).load().state.audio.volume, 23.0);
        QVERIFY(SessionStore(file).load().state.audio.muted);
        QTRY_COMPARE(player.state().volume, 23.0);
        QTRY_VERIFY(player.state().muted);
        QVERIFY(!player.state().active);
        QVERIFY(loaded.isEmpty());
        queue.playOrPause();
        QTRY_COMPARE(loaded.count(), 1);
        QCOMPARE(player.state().path, media);
        player.setVolume(61.0);
        player.setMuted(false);
        session.rememberMedia(media);
        QCOMPARE(session.mediaDirectory(), QFileInfo(media).absolutePath());
        QVERIFY(session.save({}).isEmpty());
        const auto changed = SessionStore(file).load();
        QCOMPARE(changed.state.audio.volume, 61.0);
        QVERIFY(!changed.state.audio.muted);
        QCOMPARE(changed.state.window.mediaDirectory, QFileInfo(media).absolutePath());
        queue.clear();
        QVERIFY(session.save({}).isEmpty());
        MpvController restarted(true, true);
        PlaylistController newQueue(restarted);
        SessionManager newSession(restarted, newQueue, file);
        QVERIFY(newSession.restore().error.isEmpty());
        QCOMPARE(newQueue.model()->rowCount(), 0);
        QTRY_COMPARE(restarted.state().volume, 61.0);
        QVERIFY(!restarted.state().muted);
    }

    void restartPreservesQueueAndPreferences() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("nested/settings.ini"));
        const auto media = temporary.filePath(QStringLiteral("猫 sample [1].mkv"));
        const auto missing = temporary.filePath(QStringLiteral("missing.mkv"));
        SessionState saved;
        saved.paths = {media, missing, media};
        saved.currentIndex = 2;
        saved.audio = {37.5, true};
        saved.mediaLibrary.scanningEnabled = false;
        saved.mediaLibrary.recursiveByDefault = true;
        saved.resumePlaybackPosition = false;
        saved.seekStepSeconds = 30;
        saved.window = {QByteArray::fromHex("00ff0012"), true, temporary.path()};
        QVERIFY(SessionStore(file).save(saved).isEmpty());
        const auto loaded = SessionStore(file).load();
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.state.paths, QStringList({media, missing, media}));
        QCOMPARE(loaded.state.currentIndex, 2);
        QCOMPARE(loaded.state.audio.volume, 37.5);
        QVERIFY(loaded.state.audio.muted);
        QCOMPARE(loaded.state.window.geometry, QByteArray::fromHex("00ff0012"));
        QVERIFY(loaded.state.window.playlistVisible);
        QCOMPARE(loaded.state.window.mediaDirectory, temporary.path());
        QVERIFY(!loaded.state.mediaLibrary.scanningEnabled);
        QVERIFY(loaded.state.mediaLibrary.recursiveByDefault);
        QVERIFY(!loaded.state.resumePlaybackPosition);
        QSettings persisted(file, QSettings::IniFormat);
        QCOMPARE(persisted.value(QStringLiteral("playback/seekStepSeconds")).toInt(), 30);
        QCOMPARE(loaded.state.seekStepSeconds, 30);
        QCOMPARE(loaded.state.tuning.playbackSpeed, 1.0);
        QCOMPARE(loaded.state.tuning.audioDelay, 0.0);
        QCOMPARE(loaded.state.tuning.subtitleDelay, 0.0);
    }

    void interactionPreferencesRoundTrip() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        SessionState saved;
        saved.interaction = {false, false, MiddleClickAction::Fullscreen, false, 12, false, true};
        QVERIFY(SessionStore(path).save(saved).isEmpty());
        const auto loaded = SessionStore(path).load();
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QVERIFY(!loaded.state.interaction.singleClickPlayPause);
        QVERIFY(!loaded.state.interaction.doubleClickFullscreen);
        QCOMPARE(loaded.state.interaction.middleClick, MiddleClickAction::Fullscreen);
        QVERIFY(!loaded.state.interaction.wheelAdjustsVolume);
        QCOMPARE(loaded.state.interaction.wheelVolumeStep, 12);
        QVERIFY(!loaded.state.interaction.wheelSeeksTimeline);
        QVERIFY(loaded.state.interaction.playlistSingleClick);
        QSettings persisted(path, QSettings::IniFormat);
        QCOMPARE(persisted.value(QStringLiteral("interaction/middleClick")).toString(),
                 QStringLiteral("fullscreen"));
        QCOMPARE(persisted.value(QStringLiteral("interaction/wheelVolumeStep")).toInt(), 12);
    }

    void schemaOneConfigurationWithoutInteractionValuesUsesDefaults() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.setValue(QStringLiteral("schemaVersion"), 1);
            settings.setValue(QStringLiteral("audio/volume"), 42.0);
            settings.sync();
        }
        const auto loaded = SessionStore(path).load();
        QVERIFY(loaded.error.isEmpty());
        QVERIFY(loaded.state.interaction.singleClickPlayPause);
        QVERIFY(loaded.state.interaction.doubleClickFullscreen);
        QCOMPARE(loaded.state.interaction.middleClick, MiddleClickAction::None);
        QVERIFY(loaded.state.interaction.wheelAdjustsVolume);
        QCOMPARE(loaded.state.interaction.wheelVolumeStep, 5);
        QVERIFY(loaded.state.interaction.wheelSeeksTimeline);
        QVERIFY(!loaded.state.interaction.playlistSingleClick);
    }

    void interactionWheelStepClampsAndMalformedValuesFallBackIndependently() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        QSettings settings(path, QSettings::IniFormat);
        settings.setValue(QStringLiteral("schemaVersion"), 1);
        settings.setValue(QStringLiteral("interaction/wheelVolumeStep"), 0);
        settings.setValue(QStringLiteral("interaction/middleClick"), QStringLiteral("garbage"));
        settings.setValue(QStringLiteral("interaction/singleClickPlayPause"),
                          QStringLiteral("garbage"));
        settings.setValue(QStringLiteral("interaction/doubleClickFullscreen"), false);
        settings.sync();
        const auto low = SessionStore(path).load();
        QVERIFY(low.error.isEmpty());
        QCOMPARE(low.state.interaction.wheelVolumeStep, 1);
        QCOMPARE(low.state.interaction.middleClick, MiddleClickAction::None);
        QVERIFY(low.state.interaction.singleClickPlayPause);
        QVERIFY(!low.state.interaction.doubleClickFullscreen);

        settings.setValue(QStringLiteral("interaction/wheelVolumeStep"), 99);
        settings.setValue(QStringLiteral("interaction/middleClick"), QStringLiteral("playPause"));
        settings.sync();
        const auto high = SessionStore(path).load();
        QCOMPARE(high.state.interaction.wheelVolumeStep, 20);
        QCOMPARE(high.state.interaction.middleClick, MiddleClickAction::PlayPause);

        settings.setValue(QStringLiteral("interaction/wheelVolumeStep"), QStringLiteral("garbage"));
        settings.sync();
        QCOMPARE(SessionStore(path).load().state.interaction.wheelVolumeStep, 5);
    }

    void sessionManagerRestoresAndSavesInteractionPreferences() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        SessionState saved;
        saved.interaction.middleClick = MiddleClickAction::Mute;
        saved.interaction.wheelVolumeStep = 8;
        QVERIFY(SessionStore(path).save(saved).isEmpty());
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, path);
        QVERIFY(session.restore().error.isEmpty());
        QCOMPARE(session.interactionPreferences().middleClick, MiddleClickAction::Mute);
        QCOMPARE(session.interactionPreferences().wheelVolumeStep, 8);

        InteractionPreferences changed;
        changed.singleClickPlayPause = false;
        changed.middleClick = MiddleClickAction::PlayPause;
        changed.wheelVolumeStep = 30;
        session.setInteractionPreferences(changed);
        QCOMPARE(session.interactionPreferences().wheelVolumeStep, 20);
        QVERIFY(session.save({}).isEmpty());
        const auto loaded = SessionStore(path).load();
        QVERIFY(!loaded.state.interaction.singleClickPlayPause);
        QCOMPARE(loaded.state.interaction.middleClick, MiddleClickAction::PlayPause);
        QCOMPARE(loaded.state.interaction.wheelVolumeStep, 20);
    }

    void legacyConfigurationDefaultsToEnabledOnDemandScanning() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("legacy.ini"));
        QSettings settings(path, QSettings::IniFormat);
        settings.setValue(QStringLiteral("schemaVersion"), 1);
        settings.sync();
        const auto loaded = SessionStore(path).load();
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QVERIFY(loaded.state.mediaLibrary.scanningEnabled);
        QVERIFY(!loaded.state.mediaLibrary.recursiveByDefault);
    }

    void schemaOneConfigurationWithoutTuningValuesUsesDefaults() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.setValue(QStringLiteral("schemaVersion"), 1);
            settings.setValue(QStringLiteral("audio/volume"), 42.0);
            settings.sync();
        }

        const auto loaded = SessionStore(path).load();
        QVERIFY(loaded.error.isEmpty());
        QCOMPARE(loaded.state.tuning.playbackSpeed, 1.0);
        QCOMPARE(loaded.state.tuning.audioDelay, 0.0);
        QCOMPARE(loaded.state.tuning.subtitleDelay, 0.0);
    }

    void audioOutputDeviceIsPersisted() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        SessionStore store(path);

        SessionState saved;
        saved.audio.outputDevice = QStringLiteral("pipewire/primary");
        QVERIFY(store.save(saved).isEmpty());
        QCOMPARE(store.load().state.audio.outputDevice, QStringLiteral("pipewire/primary"));

        saved.audio.outputDevice = QStringLiteral("auto");
        QVERIFY(store.save(saved).isEmpty());
        QCOMPARE(store.load().state.audio.outputDevice, QStringLiteral("auto"));
    }

    void schemaOneAudioOutputDeviceDefaultsForMissingOrMalformedValues() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        QSettings settings(path, QSettings::IniFormat);
        settings.setValue(QStringLiteral("schemaVersion"), 1);
        settings.sync();

        QCOMPARE(SessionStore(path).load().state.audio.outputDevice, QStringLiteral("auto"));

        settings.setValue(QStringLiteral("audio/outputDevice"), QString{});
        settings.sync();
        QCOMPARE(SessionStore(path).load().state.audio.outputDevice, QStringLiteral("auto"));

        settings.setValue(QStringLiteral("audio/outputDevice"),
                          QStringLiteral("pipewire") + QChar::Null + QStringLiteral("/primary"));
        settings.sync();
        QCOMPARE(SessionStore(path).load().state.audio.outputDevice, QStringLiteral("auto"));
    }

    void unavailableOutputDeviceFallsBackWithoutReplacingPreference() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        SessionState saved;
        saved.audio.outputDevice = QStringLiteral("pipewire/primary");
        QVERIFY(SessionStore(path).save(saved).isEmpty());

        MpvController player(true, true);
        PlaylistController playlist(player);
        SessionManager manager(player, playlist, path);
        QVERIFY(manager.restore().error.isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(!player.audioDevices().isEmpty(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(player.selectedAudioDevice(), QStringLiteral("auto"), 5000);

        QVERIFY(manager.save({}).isEmpty());
        QCOMPARE(SessionStore(path).load().state.audio.outputDevice,
                 QStringLiteral("pipewire/primary"));
    }

    void emptyQueueReplacesPreviousSession() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        SessionStore store(temporary.filePath(QStringLiteral("settings.ini")));
        SessionState saved;
        saved.paths = {temporary.filePath(QStringLiteral("old.mkv"))};
        saved.currentIndex = 0;
        QVERIFY(store.save(saved).isEmpty());
        QCOMPARE(store.load().state.paths.size(), 1);
        QVERIFY(store.save({}).isEmpty());
        const auto loaded = store.load();
        QVERIFY(loaded.error.isEmpty());
        QVERIFY(loaded.state.paths.isEmpty());
        QCOMPARE(loaded.state.currentIndex, -1);
    }

    void unknownSchemaIsNotOverwritten() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        {
            QSettings settings(file, QSettings::IniFormat);
            settings.setValue(QStringLiteral("schemaVersion"), 99);
            settings.setValue(QStringLiteral("future/value"), QStringLiteral("keep me"));
            settings.sync();
        }
        QFile before(file);
        QVERIFY(before.open(QIODevice::ReadOnly));
        const auto original = before.readAll();
        before.close();
        SessionStore store(file);
        QVERIFY(!store.load().error.isEmpty());
        QVERIFY(!store.save({}).isEmpty());
        QVERIFY(!SessionStore(file).save({}).isEmpty());
        QVERIFY(before.open(QIODevice::ReadOnly));
        QCOMPARE(before.readAll(), original);
    }

    void malformedConfigurationIsPreserved() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        const QByteArray original("[General]\nschemaVersion=1\n[broken group\nvalue=keep\n");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(original), original.size());
        file.close();
        SessionStore store(path);
        QVERIFY(!store.load().error.isEmpty());
        QVERIFY(!store.save({}).isEmpty());
        QVERIFY(!SessionStore(path).save({}).isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), original);
    }

    void pendingFilesSurviveClosingBeforeRendererReady() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, path);
        QVERIFY(session.restore().error.isEmpty());
        QVERIFY(session.save({}, {media, media}).isEmpty());
        const auto loaded = SessionStore(path).load();
        QCOMPARE(loaded.state.paths, QStringList({media, media}));
        QCOMPARE(loaded.state.currentIndex, 0);
        QVERIFY(!player.state().active);
    }

    void storedTuningPreferencesRestoreToPlayer() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.setValue(QStringLiteral("schemaVersion"), 1);
            settings.setValue(QStringLiteral("playback/speed"), 1.5);
            settings.setValue(QStringLiteral("playback/audioDelay"), -0.25);
            settings.setValue(QStringLiteral("playback/subtitleDelay"), 2.75);
            settings.sync();
        }
        const auto stored = SessionStore(path).load();
        QVERIFY(stored.error.isEmpty());
        QCOMPARE(stored.state.tuning.playbackSpeed, 1.5);
        QCOMPARE(stored.state.tuning.audioDelay, -0.25);
        QCOMPARE(stored.state.tuning.subtitleDelay, 2.75);
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, path);
        QSignalSpy errors(&player, &MpvController::errorOccurred);

        QVERIFY(session.restore().error.isEmpty());
        QVERIFY(session.save({}).isEmpty());
        QSettings restored(path, QSettings::IniFormat);
        QCOMPARE(restored.value(QStringLiteral("playback/speed")).toDouble(), 1.5);
        QCOMPARE(restored.value(QStringLiteral("playback/audioDelay")).toDouble(), -0.25);
        QCOMPARE(restored.value(QStringLiteral("playback/subtitleDelay")).toDouble(), 2.75);
        QTRY_VERIFY(qAbs(player.state().playbackSpeed - 1.5) < 0.000001);
        QTRY_VERIFY(qAbs(player.state().audioDelay - (-0.25)) < 0.000001);
        QTRY_VERIFY(qAbs(player.state().subtitleDelay - 2.75) < 0.000001);
        QVERIFY2(errors.isEmpty(),
                 qPrintable(errors.isEmpty() ? QString{} : errors.first().first().toString()));
    }

    void tuningRequestsArePersistedBeforeMpvObservation() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, path);
        QVERIFY(session.restore().error.isEmpty());

        player.setPlaybackSpeed(1.75);
        player.setAudioDelay(-0.4);
        player.setSubtitleDelay(3.25);
        QVERIFY(session.save({}).isEmpty());

        QSettings settings(path, QSettings::IniFormat);
        QVERIFY(qAbs(settings.value(QStringLiteral("playback/speed")).toDouble() - 1.75) <
                0.000001);
        QVERIFY(qAbs(settings.value(QStringLiteral("playback/audioDelay")).toDouble() - (-0.4)) <
                0.000001);
        QVERIFY(qAbs(settings.value(QStringLiteral("playback/subtitleDelay")).toDouble() - 3.25) <
                0.000001);
    }

    void invalidValuesCannotEscapeDomainLimits() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        const auto media = temporary.filePath(QStringLiteral("missing.mkv"));
        {
            QSettings settings(file, QSettings::IniFormat);
            settings.setValue(QStringLiteral("schemaVersion"), 1);
            settings.setValue(QStringLiteral("audio/volume"), QStringLiteral("nan"));
            settings.setValue(QStringLiteral("audio/muted"), QStringLiteral("garbage"));
            settings.setValue(QStringLiteral("playlist/paths"),
                              QStringList{QString{}, QStringLiteral("relative.mkv"), media});
            settings.setValue(QStringLiteral("playlist/currentIndex"), 2);
            settings.setValue(QStringLiteral("window/mediaDirectory"), media);
            settings.setValue(QStringLiteral("playback/speed"), 99.0);
            settings.setValue(QStringLiteral("playback/audioDelay"), -99.0);
            settings.setValue(QStringLiteral("playback/subtitleDelay"), QStringLiteral("nan"));
            settings.setValue(QStringLiteral("playback/seekStepSeconds"), 15);
            settings.sync();
        }
        const auto loaded = SessionStore(file).load();
        QVERIFY(loaded.error.isEmpty());
        QCOMPARE(loaded.state.audio.volume, 100.0);
        QVERIFY(!loaded.state.audio.muted);
        QCOMPARE(loaded.state.paths, QStringList{media});
        QCOMPARE(loaded.state.currentIndex, 0);
        QVERIFY(loaded.state.window.mediaDirectory.isEmpty());
        QCOMPARE(loaded.state.tuning.playbackSpeed, 4.0);
        QCOMPARE(loaded.state.tuning.audioDelay, -10.0);
        QCOMPARE(loaded.state.tuning.subtitleDelay, 0.0);
        QCOMPARE(loaded.state.seekStepSeconds, 5);
    }

    void inaccessibleDestinationReportsFailure() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto blocker = temporary.filePath(QStringLiteral("regular-file"));
        QFile file(blocker);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        SessionStore store(blocker + QStringLiteral("/settings.ini"));
        QVERIFY(!store.save({}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(SessionTest)
#include "session_test.moc"
