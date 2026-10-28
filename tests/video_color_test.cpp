#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "session/sessionmanager.h"
#include "session/sessionstore.h"

#include <QTemporaryDir>
#include <QtTest>
#include <cmath>
#include <limits>

namespace {
double readProperty(MpvController &player, const char *name) {
    double value = std::numeric_limits<double>::quiet_NaN();
    if (mpv_get_property(player.handle(), name, MPV_FORMAT_DOUBLE, &value) < 0)
        return std::numeric_limits<double>::quiet_NaN();
    return value;
}

QString readTextProperty(MpvController &player, const char *name) {
    if (!player.handle())
        return {};
    char *value = nullptr;
    if (mpv_get_property(player.handle(), name, MPV_FORMAT_STRING, &value) < 0 || !value)
        return {};
    const auto result = QString::fromUtf8(value);
    mpv_free(value);
    return result;
}
} // namespace

class VideoColorTest final : public QObject {
    Q_OBJECT
  private slots:
    void deinterlaceModeUsesNativeMpvProperty() {
        MpvController player(true, true);
        QCOMPARE(player.deinterlaceMode(), DeinterlaceMode::Auto);
        QTRY_COMPARE(readTextProperty(player, "deinterlace"), QStringLiteral("auto"));
        player.setDeinterlaceMode(DeinterlaceMode::On);
        QCOMPARE(player.deinterlaceMode(), DeinterlaceMode::On);
        QTRY_COMPARE(readTextProperty(player, "deinterlace"), QStringLiteral("yes"));
        player.setDeinterlaceMode(DeinterlaceMode::Off);
        QCOMPARE(player.deinterlaceMode(), DeinterlaceMode::Off);
        QTRY_COMPARE(readTextProperty(player, "deinterlace"), QStringLiteral("no"));
    }

    void toneMappingUsesGpuLibplaceboProperty() {
        MpvController player(true, true);
        QCOMPARE(player.hdrToneMapping(), HdrToneMapping::Auto);
        player.setHdrToneMapping(HdrToneMapping::Bt2390);
        QCOMPARE(player.hdrToneMapping(), HdrToneMapping::Bt2390);
        QTRY_COMPARE(readTextProperty(player, "tone-mapping"), QStringLiteral("bt.2390"));
        player.setHdrToneMapping(HdrToneMapping::Clip);
        QCOMPARE(player.hdrToneMapping(), HdrToneMapping::Clip);
        QTRY_COMPARE(readTextProperty(player, "tone-mapping"), QStringLiteral("clip"));
        player.setHdrToneMapping(HdrToneMapping::Auto);
        QCOMPARE(player.hdrToneMapping(), HdrToneMapping::Auto);
        QTRY_COMPARE(readTextProperty(player, "tone-mapping"), QStringLiteral("auto"));
    }

    void clampsColorAdjustmentsAndRejectsNonFiniteValues() {
        MpvController player(true, true);
        player.setVideoColorAdjustment({120, -120, 45, 20, -10});
        QCOMPARE(player.videoColorAdjustment(), (VideoColorAdjustment{100, -100, 45, 20, -10}));
        QTRY_COMPARE(readProperty(player, "brightness"), 100.0);
        QTRY_COMPARE(readProperty(player, "contrast"), -100.0);
        QTRY_COMPARE(readProperty(player, "saturation"), 45.0);
        QTRY_COMPARE(readProperty(player, "hue"), 20.0);
        QTRY_COMPARE(readProperty(player, "gamma"), -10.0);

        auto invalid = player.videoColorAdjustment();
        invalid.brightness = std::numeric_limits<double>::quiet_NaN();
        invalid.gamma = std::numeric_limits<double>::infinity();
        player.setVideoColorAdjustment(invalid);
        QCOMPARE(player.videoColorAdjustment().brightness, 0.0);
        QCOMPARE(player.videoColorAdjustment().gamma, 0.0);
        QTRY_COMPARE(readProperty(player, "brightness"), 0.0);
        QTRY_COMPARE(readProperty(player, "gamma"), 0.0);
    }

    void videoFiltersCombineAndPreserveDeinterlace() {
        MpvController player(true, true);
        QCOMPARE(player.sharpenLevel(), SharpenLevel::Off);
        QCOMPARE(player.denoiseLevel(), DenoiseLevel::Off);
        player.setDeinterlaceMode(DeinterlaceMode::On);
        QSignalSpy changed(&player, &MpvController::videoFiltersChanged);
        player.setVideoFilters(SharpenLevel::High, DenoiseLevel::Off);
        QCOMPARE(player.sharpenLevel(), SharpenLevel::High);
        QTRY_VERIFY(readTextProperty(player, "vf").contains(QStringLiteral("unsharp")));
        QVERIFY(readTextProperty(player, "vf").contains(QStringLiteral("luma_amount")));
        QVERIFY(readTextProperty(player, "vf").contains(QStringLiteral("1.5")));
        QCOMPARE(readTextProperty(player, "deinterlace"), QStringLiteral("yes"));
        player.setVideoFilters(SharpenLevel::Off, DenoiseLevel::Medium);
        QCOMPARE(player.sharpenLevel(), SharpenLevel::Off);
        QCOMPARE(player.denoiseLevel(), DenoiseLevel::Medium);
        QTRY_VERIFY(readTextProperty(player, "vf").contains(QStringLiteral("hqdn3d")));
        QVERIFY(!readTextProperty(player, "vf").contains(QStringLiteral("unsharp")));
        player.setVideoFilters(SharpenLevel::Low, DenoiseLevel::High);
        QTRY_VERIFY(readTextProperty(player, "vf").contains(QStringLiteral("unsharp")));
        QVERIFY(readTextProperty(player, "vf").contains(QStringLiteral("hqdn3d")));
        QCOMPARE(readTextProperty(player, "deinterlace"), QStringLiteral("yes"));
        player.setVideoFilters(SharpenLevel::Off, DenoiseLevel::Off);
        QTRY_VERIFY(readTextProperty(player, "vf").isEmpty() ||
                    readTextProperty(player, "vf") == QStringLiteral("no"));
        QCOMPARE(readTextProperty(player, "deinterlace"), QStringLiteral("yes"));
        QCOMPARE(changed.count(), 4);
    }

    void settingsPersistAndRestoreColorAdjustments() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        {
            MpvController player(true, true);
            PlaylistController playlist(player);
            SessionManager session(player, playlist, path);
            QVERIFY(session.restore().error.isEmpty());
            player.setVideoColorAdjustment({15, -20, 30, -40, 50});
            QVERIFY(session.save({}).isEmpty());
        }

        const auto saved = SessionStore(path).load();
        QVERIFY(saved.error.isEmpty());
        QCOMPARE(saved.state.videoColor, (VideoColorAdjustment{15, -20, 30, -40, 50}));

        MpvController restored(true, true);
        PlaylistController playlist(restored);
        SessionManager session(restored, playlist, path);
        QVERIFY(session.restore().error.isEmpty());
        QCOMPARE(restored.videoColorAdjustment(), saved.state.videoColor);
        QTRY_COMPARE(readProperty(restored, "brightness"), 15.0);
        QTRY_COMPARE(readProperty(restored, "gamma"), 50.0);
    }
};

QTEST_GUILESS_MAIN(VideoColorTest)
#include "video_color_test.moc"
