#include "playback/mpvcontroller.h"
#include "ui/playbackoverlaytext.h"

#include <QtTest>
#include <limits>

class PlaybackOverlayTextTest final : public QObject {
    Q_OBJECT
  private slots:
    void seekUsesObservedPositionAndBoundsProgress() {
        PlaybackState state;
        state.active = state.seekable = true;
        state.position = 3661;
        state.duration = 14644;
        auto progress = formatSeekProgress(state);
        QVERIFY(progress.available);
        QCOMPARE(progress.value, 2500);
        QVERIFY(progress.text.contains(QStringLiteral("01:01:01 / 04:04:04")));
        QVERIFY(progress.text.contains(QStringLiteral("25.0%")));
        state.position = 200;
        state.duration = 100;
        progress = formatSeekProgress(state);
        QCOMPARE(progress.value, 10000);
        QVERIFY(progress.text.contains(QStringLiteral("00:03:20 / 00:01:40")));
        state.position = -1;
        QCOMPARE(formatSeekProgress(state).value, 0);
        state.position = std::numeric_limits<double>::max();
        state.duration = std::numeric_limits<double>::min();
        QCOMPARE(formatSeekProgress(state).value, 10000);
        state.position = 0;
        QCOMPARE(formatSeekProgress(state).value, 0);
    }

    void unavailableSeekInputs_data() {
        QTest::addColumn<bool>("active");
        QTest::addColumn<bool>("seekable");
        QTest::addColumn<double>("position");
        QTest::addColumn<double>("duration");
        const auto nan = std::numeric_limits<double>::quiet_NaN();
        const auto inf = std::numeric_limits<double>::infinity();
        QTest::newRow("idle") << false << true << 1.0 << 10.0;
        QTest::newRow("live") << true << false << 1.0 << 10.0;
        QTest::newRow("zero-duration") << true << true << 1.0 << 0.0;
        QTest::newRow("negative-duration") << true << true << 1.0 << -10.0;
        QTest::newRow("nan-duration") << true << true << 1.0 << nan;
        QTest::newRow("inf-duration") << true << true << 1.0 << inf;
        QTest::newRow("nan-position") << true << true << nan << 10.0;
        QTest::newRow("inf-position") << true << true << inf << 10.0;
        QTest::newRow("negative-inf-position") << true << true << -inf << 10.0;
    }

    void unavailableSeekInputs() {
        QFETCH(bool, active);
        QFETCH(bool, seekable);
        QFETCH(double, position);
        QFETCH(double, duration);
        PlaybackState state;
        state.active = active;
        state.seekable = seekable;
        state.position = position;
        state.duration = duration;
        const auto progress = formatSeekProgress(state);
        QVERIFY(!progress.available);
        QCOMPARE(progress.value, 0);
        QVERIFY(progress.text.isEmpty());
    }

    void informationShowsObservedTechnicalValuesCompactly() {
        PlaybackState state;
        state.active = true;
        state.path = QStringLiteral("/影片/movie.mkv");
        state.position = 65;
        state.duration = 120;
        state.playbackSpeed = 1.25;
        state.hardwareDecoder = QStringLiteral("vaapi");
        MediaInformation info;
        info.videoCodec = QStringLiteral("hevc");
        info.videoWidth = 1920;
        info.videoHeight = 1080;
        info.videoPixelFormat = QStringLiteral("yuv420p10le");
        info.containerFrameRate = 23.976;
        info.audioCodec = QStringLiteral("aac");
        info.audioSampleRate = 48000;
        info.audioChannels = QStringLiteral("stereo");
        const auto text = formatPlaybackOverlayInformation(info, state);
        for (const auto &part :
             {QStringLiteral("movie.mkv"), QStringLiteral("00:01:05"), QStringLiteral("00:02:00"),
              QStringLiteral("播放中"), QStringLiteral("1.25"), QStringLiteral("hevc"),
              QStringLiteral("1920 × 1080"), QStringLiteral("yuv420p10le"),
              QStringLiteral("23.976"), QStringLiteral("估计"), QStringLiteral("vaapi"),
              QStringLiteral("aac"), QStringLiteral("48000 Hz"), QStringLiteral("stereo")})
            QVERIFY2(text.contains(part), qPrintable(part));
        QVERIFY(text.count(QLatin1Char('\n')) >= 4);
        QVERIFY(text.count(QLatin1Char('\n')) >= 11);
        state.paused = true;
        state.hardwareDecoder = QStringLiteral("no");
        const auto paused = formatPlaybackOverlayInformation(info, state);
        QVERIFY(paused.contains(QStringLiteral("已暂停")));
        QVERIFY(paused.contains(QStringLiteral("软件解码")));
        QVERIFY(!paused.contains(QStringLiteral("vaapi")));
    }

    void informationIncludesContainerPlaybackAudioAndSelectedTrackDetails() {
        PlaybackState state;
        state.active = true;
        state.path = QStringLiteral("/影片/movie.mkv");
        state.position = 65;
        state.duration = 120;
        state.volume = 42.5;
        state.muted = true;
        state.audioDelay = 0.125;
        state.subtitleDelay = -0.75;
        MediaInformation info;
        info.fileFormat = QStringLiteral("matroska,webm");
        info.fileSize = 123456;
        info.audioSampleFormat = QStringLiteral("fltp");
        QList<MediaTrack> tracks{{1,
                                  MediaTrack::Type::Audio,
                                  QStringLiteral("ja"),
                                  QStringLiteral("Director's commentary"),
                                  QStringLiteral("aac"),
                                  false,
                                  true,
                                  {}},
                                 {2,
                                  MediaTrack::Type::Audio,
                                  QStringLiteral("en"),
                                  QStringLiteral("Alternate"),
                                  QStringLiteral("aac"),
                                  false,
                                  false,
                                  {}},
                                 {3, MediaTrack::Type::Subtitle, QStringLiteral("en"),
                                  QStringLiteral("English captions"), QStringLiteral("ass"), true,
                                  true, QStringLiteral("/private/subtitle.srt")}};

        const auto text = formatPlaybackOverlayInformation(info, state, tracks);
        for (const auto &part :
             {QStringLiteral("matroska,webm"), QStringLiteral("123456"), QStringLiteral("fltp"),
              QStringLiteral("42.5"), QStringLiteral("Director's commentary"),
              QStringLiteral("English captions")})
            QVERIFY2(text.contains(part), qPrintable(part));
        QVERIFY(text.contains(QStringLiteral("125 ms")));
        QVERIFY(text.contains(QStringLiteral("-750 ms")));
        QVERIFY(text.contains(QStringLiteral("静音")) || text.contains(QStringLiteral("已静音")));
        QVERIFY(!text.contains(QStringLiteral("Alternate")));
        QVERIFY(!text.contains(QStringLiteral("/private/subtitle.srt")));
    }

    void informationLabelsHdrTransferAndPrimaries() {
        MediaInformation info;
        info.videoCodec = QStringLiteral("hevc");
        info.videoPrimaries = QStringLiteral("bt.2020");
        info.videoTransfer = QStringLiteral("pq");
        PlaybackState state;
        state.active = true;
        state.path = QStringLiteral("hdr.mkv");

        const auto text = formatPlaybackOverlayInformation(info, state);
        QVERIFY(text.contains(QStringLiteral("HDR · PQ")));
        QVERIFY(text.contains(QStringLiteral("bt.2020")));
    }

    void unavailableLiveDetailsAreUnknownAndTrackTextIsSanitized() {
        PlaybackState state;
        state.active = true;
        state.path = QStringLiteral("https://user:secret@example.org/live?token=private");
        state.seekable = false;
        state.position = std::numeric_limits<double>::quiet_NaN();
        state.duration = 0;
        state.volume = std::numeric_limits<double>::infinity();
        state.audioDelay = std::numeric_limits<double>::quiet_NaN();
        state.subtitleDelay = std::numeric_limits<double>::infinity();
        MediaInformation info;
        QList<MediaTrack> tracks{{1,
                                  MediaTrack::Type::Audio,
                                  QStringLiteral("en"),
                                  QStringLiteral("Live\ncommentary") + QChar(0x1b),
                                  QStringLiteral("aac"),
                                  false,
                                  true,
                                  {}}};

        const auto text = formatPlaybackOverlayInformation(info, state, tracks);
        QVERIFY(text.contains(QStringLiteral("live")));
        for (const auto &secret : {"user", "secret", "token", "private"})
            QVERIFY(!text.contains(QLatin1String(secret)));
        QVERIFY(text.contains(QStringLiteral("未知")));
        QVERIFY(!text.contains(QStringLiteral("nan"), Qt::CaseInsensitive));
        QVERIFY(!text.contains(QStringLiteral("inf"), Qt::CaseInsensitive));
        QVERIFY(!text.contains(QChar(0x1b)));
        QVERIFY(!text.contains(QStringLiteral("Live\ncommentary")));
        QVERIFY(text.contains(QStringLiteral("Live commentary")));
    }

    void informationHidesNetworkSecretsAndBoundsHostileNames() {
        PlaybackState state;
        state.active = true;
        state.path =
            QStringLiteral("https://user:password@example.org/video.mkv?token=secret#private");
        auto text = formatPlaybackOverlayInformation({}, state);
        QVERIFY(text.contains(QStringLiteral("video.mkv")));
        for (const auto &secret : {"user", "password", "token", "secret", "private"})
            QVERIFY(!text.contains(QLatin1String(secret)));
        state.path = QStringLiteral("/tmp/<b>unsafe</b>\n\r\t") + QChar(0x1b) +
                     QString(500, QLatin1Char('x'));
        text = formatPlaybackOverlayInformation({}, state);
        const auto nameLine = text.section(QLatin1Char('\n'), 0, 0);
        QVERIFY(nameLine.size() <= 170);
        QVERIFY(!nameLine.contains(QLatin1Char('\r')));
        QVERIFY(!nameLine.contains(QLatin1Char('\t')));
        QVERIFY(!nameLine.contains(QChar(0x1b)));
        QVERIFY(text.count(QLatin1Char('\n')) >= 6);
    }

    void audioOnlyAndIdleDoNotRetainVideoOrDecoder() {
        PlaybackState state;
        state.active = true;
        state.path = QStringLiteral("/music.flac");
        state.hardwareDecoder = QStringLiteral("stale-vaapi");
        MediaInformation info;
        info.audioCodec = QStringLiteral("flac");
        info.audioSampleRate = 96000;
        info.audioChannels = QStringLiteral("stereo");
        const auto text = formatPlaybackOverlayInformation(info, state);
        QVERIFY(text.contains(QStringLiteral("flac")));
        QVERIFY(text.contains(QStringLiteral("96000 Hz")));
        QVERIFY(!text.contains(QStringLiteral("stale-vaapi")));
        QVERIFY(!text.contains(QStringLiteral("0 × 0")));
        QVERIFY(text.contains(QStringLiteral("无视频")));
        const auto unknown = formatPlaybackOverlayInformation({}, state);
        QVERIFY(unknown.contains(QStringLiteral("未知")));
        QVERIFY(!unknown.contains(QStringLiteral("0 Hz")));
        state.active = false;
        QCOMPARE(formatPlaybackOverlayInformation(info, state), QStringLiteral("未播放媒体"));
    }
};

QTEST_GUILESS_MAIN(PlaybackOverlayTextTest)
#include "playback_overlay_text_test.moc"
