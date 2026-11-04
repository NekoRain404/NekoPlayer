#include "playback/mpv/mpvcontroller.h"

#include <QSignalSpy>
#include <QtTest>

namespace {
QString option(MpvController &player, const char *name) {
    char *value = mpv_get_property_string(player.handle(), name);
    const auto text = QString::fromUtf8(value);
    mpv_free(value);
    return text;
}
} // namespace

class AudioEffectsTest final : public QObject {
    Q_OBJECT
  private slots:
    void replayGainAndExclusiveReachMpv() {
        MpvController player(true, true);
        QVERIFY(player.isReady());
        QSignalSpy replay(&player, &MpvController::replayGainModeChanged);
        QSignalSpy exclusive(&player, &MpvController::audioExclusiveChanged);
        QCOMPARE(option(player, "replaygain"), QStringLiteral("no"));
        player.setReplayGainMode(ReplayGainMode::Track);
        QCOMPARE(player.replayGainMode(), ReplayGainMode::Track);
        QCOMPARE(option(player, "replaygain"), QStringLiteral("track"));
        QCOMPARE(replay.count(), 1);
        player.setReplayGainMode(ReplayGainMode::Album);
        QCOMPARE(option(player, "replaygain"), QStringLiteral("album"));
        player.setAudioExclusive(true);
        QVERIFY(player.audioExclusive());
        QCOMPARE(option(player, "audio-exclusive"), QStringLiteral("yes"));
        QCOMPARE(exclusive.count(), 1);
        player.setAudioExclusive(false);
        QCOMPARE(option(player, "audio-exclusive"), QStringLiteral("no"));
    }

    void customEqualizerBandsReachTheAudioChain() {
        MpvController player(true, true);
        QVERIFY(player.isReady());
        const QList<double> gains{1.0, 2.0, -3.0, 0.5, 0.0, -0.5, 3.0, -2.0, 1.5, 0.0};
        player.setEqualizerGains(gains);
        QVERIFY(player.equalizerCustom());
        QCOMPARE(player.equalizerGains(), gains);
        QTRY_VERIFY(option(player, "af").contains(QStringLiteral("equalizer=f=31")));
        QVERIFY(option(player, "af").contains(QStringLiteral("g=-3")));
        player.setEqualizerPreset(EqualizerPreset::Off);
        QVERIFY(!player.equalizerCustom());
        QTRY_VERIFY(option(player, "af").isEmpty() || option(player, "af") == QStringLiteral("no"));
    }
};

QTEST_GUILESS_MAIN(AudioEffectsTest)
#include "audio_effects_test.moc"
