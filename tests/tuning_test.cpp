#include "playback/mpvcontroller.h"
#include <QSignalSpy>
#include <QtTest>
#include <limits>

namespace {
double backendValue(const MpvController &player, const char *name) {
    double result = std::numeric_limits<double>::quiet_NaN();
    mpv_get_property(player.handle(), name, MPV_FORMAT_DOUBLE, &result);
    return result;
}
bool set(MpvController &player, const char *method, double value) {
    return QMetaObject::invokeMethod(&player, method, Qt::DirectConnection, Q_ARG(double, value));
}
} // namespace

class TuningTest final : public QObject {
    Q_OBJECT
  private slots:
    void adjustsPausedMediaWithoutReloading() {
        MpvController player(true, true);
        QVERIFY2(player.isReady(), qPrintable(player.errorString()));
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        QSignalSpy tuning(&player, &MpvController::tuningChanged);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_COMPARE(loaded.count(), 1);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.seekAbsolute(1.0);
        QTRY_VERIFY(qAbs(player.state().position - 1.0) < 0.2);
        QVERIFY(set(player, "setPlaybackSpeed", 1.5));
        QVERIFY(set(player, "setAudioDelay", 0.2));
        QVERIFY(set(player, "setSubtitleDelay", -0.3));
        QTRY_VERIFY(qAbs(backendValue(player, "speed") - (1.5)) < 0.000001);
        QTRY_VERIFY(qAbs(backendValue(player, "audio-delay") - (0.2)) < 0.000001);
        QTRY_VERIFY(qAbs(backendValue(player, "sub-delay") - (-0.3)) < 0.000001);
        QTRY_VERIFY(qAbs(player.state().playbackSpeed - (1.5)) < 0.000001);
        QTRY_VERIFY(qAbs(player.state().audioDelay - (0.2)) < 0.000001);
        QTRY_VERIFY(qAbs(player.state().subtitleDelay - (-0.3)) < 0.000001);
        QVERIFY(!tuning.isEmpty());
        QVERIFY(player.state().paused);
        QVERIFY(qAbs(player.state().position - 1.0) < 0.2);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(errors.isEmpty());
    }

    void rapidStepsAccumulateBeforePropertyReplies() {
        MpvController player(true, true);
        QVERIFY(set(player, "setPlaybackSpeed", 1.0));
        QVERIFY(set(player, "setAudioDelay", 0.0));
        QVERIFY(set(player, "setSubtitleDelay", 0.0));
        for (int i = 0; i < 3; ++i) {
            QVERIFY(set(player, "adjustPlaybackSpeed", 0.1));
            QVERIFY(set(player, "adjustAudioDelay", 0.05));
            QVERIFY(set(player, "adjustSubtitleDelay", -0.1));
        }
        QTRY_VERIFY(qAbs(backendValue(player, "speed") - 1.3) < 0.0001);
        QTRY_VERIFY(qAbs(backendValue(player, "audio-delay") - 0.15) < 0.0001);
        QTRY_VERIFY(qAbs(backendValue(player, "sub-delay") + 0.3) < 0.0001);
        QVERIFY(set(player, "setPlaybackSpeed", 1.0));
        QVERIFY(set(player, "setAudioDelay", 0.0));
        QVERIFY(set(player, "setSubtitleDelay", 0.0));
        QTRY_VERIFY(qAbs(backendValue(player, "speed") - (1.0)) < 0.000001);
        QTRY_VERIFY(qAbs(backendValue(player, "audio-delay") - (0.0)) < 0.000001);
        QTRY_VERIFY(qAbs(backendValue(player, "sub-delay") - (0.0)) < 0.000001);
    }

    void clampsAndRejectsInvalidNumbers_data() {
        QTest::addColumn<QByteArray>("method");
        QTest::addColumn<QByteArray>("name");
        QTest::addColumn<double>("lower");
        QTest::addColumn<double>("upper");
        QTest::newRow("speed") << QByteArray("setPlaybackSpeed") << QByteArray("speed") << 0.25
                               << 4.0;
        QTest::newRow("audio") << QByteArray("setAudioDelay") << QByteArray("audio-delay") << -10.0
                               << 10.0;
        QTest::newRow("subtitle") << QByteArray("setSubtitleDelay") << QByteArray("sub-delay")
                                  << -600.0 << 600.0;
    }

    void clampsAndRejectsInvalidNumbers() {
        QFETCH(QByteArray, method);
        QFETCH(QByteArray, name);
        QFETCH(double, lower);
        QFETCH(double, upper);
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        QVERIFY(set(player, method.constData(), -1000.0));
        QTRY_COMPARE(backendValue(player, name.constData()), lower);
        QVERIFY(set(player, method.constData(), 1000.0));
        QTRY_COMPARE(backendValue(player, name.constData()), upper);
        QVERIFY(set(player, method.constData(), std::numeric_limits<double>::infinity()));
        QVERIFY(set(player, method.constData(), std::numeric_limits<double>::quiet_NaN()));
        QTest::qWait(50);
        QCOMPARE(backendValue(player, name.constData()), upper);
        QVERIFY(errors.isEmpty());
    }

    void retainsSettingsAcrossReplacementAndRendererRecovery() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QVERIFY(set(player, "setPlaybackSpeed", 1.25));
        QVERIFY(set(player, "setAudioDelay", 0.1));
        QVERIFY(set(player, "setSubtitleDelay", -0.2));
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_COMPARE(loaded.count(), 1);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.prepareForRendererReset();
        player.resumeAfterRendererReset();
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_VERIFY(player.state().paused);
        QTRY_VERIFY(qAbs(backendValue(player, "speed") - (1.25)) < 0.000001);
        QTRY_VERIFY(qAbs(backendValue(player, "audio-delay") - (0.1)) < 0.000001);
        QTRY_VERIFY(qAbs(backendValue(player, "sub-delay") - (-0.2)) < 0.000001);
        player.stop();
        QTRY_VERIFY(!player.state().active);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_COMPARE(loaded.count(), 3);
        QTRY_VERIFY(qAbs(backendValue(player, "speed") - (1.25)) < 0.000001);
        QTRY_VERIFY(qAbs(backendValue(player, "audio-delay") - (0.1)) < 0.000001);
        QTRY_VERIFY(qAbs(backendValue(player, "sub-delay") - (-0.2)) < 0.000001);
        const MpvController nextLaunch(true, true);
        QCOMPARE(backendValue(nextLaunch, "speed"), 1.0);
        QCOMPARE(backendValue(nextLaunch, "audio-delay"), 0.0);
        QCOMPARE(backendValue(nextLaunch, "sub-delay"), 0.0);
    }

    void observesBackendChangesBeforeTheNextStep() {
        MpvController player(true, true);
        player.setAudioDelay(0.2);
        QTRY_VERIFY(qAbs(player.state().audioDelay - 0.2) < 0.000001);
        player.setAudioDelay(0.2); // No property-change event for an unchanged value.
        QTest::qWait(50);
        double speed = 1.8;
        double audio = -0.3;
        double subtitles = 0.7;
        QCOMPARE(mpv_set_property(player.handle(), "speed", MPV_FORMAT_DOUBLE, &speed), 0);
        QCOMPARE(mpv_set_property(player.handle(), "audio-delay", MPV_FORMAT_DOUBLE, &audio), 0);
        QCOMPARE(mpv_set_property(player.handle(), "sub-delay", MPV_FORMAT_DOUBLE, &subtitles), 0);
        QTRY_VERIFY(qAbs(player.state().playbackSpeed - (1.8)) < 0.000001);
        QTRY_VERIFY(qAbs(player.state().audioDelay + 0.3) < 0.000001);
        QTRY_VERIFY(qAbs(player.state().subtitleDelay - (0.7)) < 0.000001);
        player.adjustPlaybackSpeed(0.1);
        player.adjustAudioDelay(0.05);
        player.adjustSubtitleDelay(-0.1);
        QTRY_VERIFY(qAbs(player.state().playbackSpeed - 1.9) < 0.0001);
        QTRY_VERIFY(qAbs(player.state().audioDelay - (-0.25)) < 0.000001);
        QTRY_VERIFY(qAbs(player.state().subtitleDelay - 0.6) < 0.0001);
    }
};

QTEST_GUILESS_MAIN(TuningTest)
#include "tuning_test.moc"
