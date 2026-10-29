#include "playback/mpv/mpvcontroller.h"
#include "ui/dialogs/mediainformationtext.h"

#include <QtTest>

class MediaReportTest final : public QObject {
    Q_OBJECT
  private slots:
    void exposesActualValuesAndTreatsPathsAsPlainText() {
        PlaybackState state;
        state.active = true;
        state.path = QStringLiteral("/影片/<b>猫 & sample</b>.mkv");
        state.hardwareDecoder = QStringLiteral("vaapi");
        state.duration = 4;
        MediaInformation info;
        info.fileFormat = QStringLiteral("matroska,webm");
        info.fileSize = 123456;
        info.videoCodec = QStringLiteral("mpeg4");
        info.videoWidth = 320;
        info.videoHeight = 180;
        info.videoPixelFormat = QStringLiteral("yuv420p");
        info.videoPrimaries = QStringLiteral("bt.2020");
        info.videoTransfer = QStringLiteral("pq");
        info.containerFrameRate = 25;
        info.audioCodec = QStringLiteral("pcm_s16le");
        info.audioSampleRate = 48000;
        info.audioChannels = QStringLiteral("mono");
        info.audioSampleFormat = QStringLiteral("s16");
        const auto report = formatMediaInformation(info, state);
        for (const auto &text :
             {state.path, info.fileFormat, QStringLiteral("123456"), QStringLiteral("320 × 180"),
              QStringLiteral("25.000"), QStringLiteral("48000 Hz"), QStringLiteral("vaapi"),
              info.videoCodec, info.videoPixelFormat, info.videoPrimaries, QStringLiteral("HDR"),
              info.audioCodec, info.audioChannels, info.audioSampleFormat})
            QVERIFY2(report.contains(text), qPrintable(text));
        state.hardwareDecoder = QStringLiteral("no");
        QVERIFY(formatMediaInformation(info, state).contains(QStringLiteral("软件解码")));
    }

    void unavailableAndInactiveMediaDoNotInventDecoderOrZeroValues() {
        PlaybackState state;
        state.active = true;
        MediaInformation info;
        const auto report = formatMediaInformation(info, state);
        QVERIFY(report.contains(QStringLiteral("未知")));
        QVERIFY(!report.contains(QStringLiteral("0 × 0")));
        QVERIFY(!report.contains(QStringLiteral("0 Hz")));
        QVERIFY(!report.contains(QStringLiteral("软件解码")));
        state.active = false;
        state.path = QStringLiteral("/old.mkv");
        info.videoCodec = QStringLiteral("stale-codec");
        const auto idle = formatMediaInformation(info, state);
        QVERIFY(idle.contains(QStringLiteral("未播放媒体")));
        QVERIFY(!idle.contains(QStringLiteral("stale-codec")));
        QVERIFY(!idle.contains(state.path));
    }

    void redactsNetworkCredentialsAndQueryFromReport() {
        PlaybackState state;
        state.active = true;
        state.path = QStringLiteral("https://user:token@example.test/video.mkv?sig=secret");
        const auto report = formatMediaInformation(MediaInformation{}, state);
        QVERIFY(report.contains(QStringLiteral("example.test")));
        QVERIFY(report.contains(QStringLiteral("video.mkv")));
        QVERIFY(report.contains(QStringLiteral("已隐藏凭据与查询参数")));
        QVERIFY(!report.contains(QStringLiteral("user")));
        QVERIFY(!report.contains(QStringLiteral("token")));
        QVERIFY(!report.contains(QStringLiteral("sig=secret")));
        QVERIFY(!report.contains(QStringLiteral("?")));
    }
};

QTEST_GUILESS_MAIN(MediaReportTest)
#include "media_report_test.moc"
