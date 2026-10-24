#include "ui/thumbnailpreviewpolicy.h"

#include <QtTest>

class ThumbnailPreviewPolicyTest final : public QObject {
    Q_OBJECT

  private slots:
    void rejectsSeekableAudioWithoutMotionVideo() {
        PlaybackState state;
        state.active = true;
        state.seekable = true;
        state.duration = 120.0;
        state.path = QStringLiteral("/media/audio.flac");

        QVERIFY(!canRequestThumbnailPreview(state, false));
    }

    void acceptsSeekableLocalMotionVideo() {
        PlaybackState state;
        state.active = true;
        state.seekable = true;
        state.duration = 120.0;
        state.path = QStringLiteral("/media/movie.mkv");

        QVERIFY(canRequestThumbnailPreview(state, true));
    }

    void rejectsUnavailableOrNonlocalPreviewSources() {
        PlaybackState state;
        state.active = true;
        state.seekable = true;
        state.duration = 120.0;
        state.path = QStringLiteral("https://example.org/movie.mkv");
        QVERIFY(!canRequestThumbnailPreview(state, true));

        state.path = QStringLiteral("/media/movie.mkv");
        state.seekable = false;
        QVERIFY(!canRequestThumbnailPreview(state, true));
        state.seekable = true;
        state.duration = 0.0;
        QVERIFY(!canRequestThumbnailPreview(state, true));
        state.duration = 120.0;
        state.active = false;
        QVERIFY(!canRequestThumbnailPreview(state, true));
    }
};

QTEST_GUILESS_MAIN(ThumbnailPreviewPolicyTest)
#include "thumbnail_preview_policy_test.moc"
