#include "playback/mpv/mpvcontroller.h"
#include "playback/playlist/playlistcontroller.h"
#include "session/sessionmanager.h"
#include "session/sessionstore.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class InteractionPreferencesTest final : public QObject {
    Q_OBJECT
  private slots:
    void interactionPreferencesRoundTrip() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        SessionState saved;
        saved.interaction = {.singleClickPlayPause = false,
                             .doubleClickFullscreen = false,
                             .middleClick = MiddleClickAction::Fullscreen,
                             .wheelAdjustsVolume = false,
                             .wheelVolumeStep = 12,
                             .wheelSeeksTimeline = false,
                             .playlistSingleClick = true,
                             .showRemainingTime = true};
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
        QVERIFY(loaded.state.interaction.showRemainingTime);
        QSettings persisted(path, QSettings::IniFormat);
        QCOMPARE(persisted.value(QStringLiteral("interaction/middleClick")).toString(),
                 QStringLiteral("fullscreen"));
        QCOMPARE(persisted.value(QStringLiteral("interaction/wheelVolumeStep")).toInt(), 12);
        QVERIFY(persisted.value(QStringLiteral("interaction/showRemainingTime")).toBool());
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
        QVERIFY(!loaded.state.interaction.showRemainingTime);
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
        settings.setValue(QStringLiteral("interaction/showRemainingTime"),
                          QStringLiteral("garbage"));
        settings.sync();
        const auto low = SessionStore(path).load();
        QVERIFY(low.error.isEmpty());
        QCOMPARE(low.state.interaction.wheelVolumeStep, 1);
        QCOMPARE(low.state.interaction.middleClick, MiddleClickAction::None);
        QVERIFY(low.state.interaction.singleClickPlayPause);
        QVERIFY(!low.state.interaction.doubleClickFullscreen);
        QVERIFY(!low.state.interaction.showRemainingTime);

        settings.setValue(QStringLiteral("interaction/wheelVolumeStep"), 99);
        settings.setValue(QStringLiteral("interaction/middleClick"), QStringLiteral("playPause"));
        settings.setValue(QStringLiteral("interaction/showRemainingTime"), QStringLiteral("1"));
        settings.sync();
        const auto high = SessionStore(path).load();
        QCOMPARE(high.state.interaction.wheelVolumeStep, 20);
        QCOMPARE(high.state.interaction.middleClick, MiddleClickAction::PlayPause);
        QVERIFY(high.state.interaction.showRemainingTime);

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
        saved.interaction.showRemainingTime = true;
        QVERIFY(SessionStore(path).save(saved).isEmpty());
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, path);
        QVERIFY(session.restore().error.isEmpty());
        QCOMPARE(session.interactionPreferences().middleClick, MiddleClickAction::Mute);
        QCOMPARE(session.interactionPreferences().wheelVolumeStep, 8);
        QVERIFY(session.interactionPreferences().showRemainingTime);

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
        QVERIFY(!loaded.state.interaction.showRemainingTime);
    }

    void interactionPreferencesNormalizeWheelStepAndDefaultEquality() {
        InteractionPreferences preferences;
        preferences.wheelVolumeStep = 0;
        QCOMPARE(preferences.normalized().wheelVolumeStep, 1);
        preferences.wheelVolumeStep = 99;
        QCOMPARE(preferences.normalized().wheelVolumeStep, 20);
        InteractionPreferences equal;
        QVERIFY(preferences != equal);
        equal.wheelVolumeStep = 20;
        QCOMPARE(preferences.normalized(), equal.normalized());
        equal.playlistSingleClick = true;
        QVERIFY(preferences != equal);
        equal.playlistSingleClick = false;
        QCOMPARE(preferences.normalized(), equal.normalized());
        equal.showRemainingTime = true;
        QVERIFY(preferences != equal);
    }
};

QTEST_GUILESS_MAIN(InteractionPreferencesTest)
#include "interaction_preferences_test.moc"
