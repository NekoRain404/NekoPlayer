#include "desktop/mprisplayer.h"
#include "playback/mpv/mpvcontroller.h"
#include "playback/playlist/playlistcontroller.h"

#include <QFileInfo>
#include <QtTest>

class MprisTest final : public QObject {
    Q_OBJECT
  private slots:
    void propertiesFollowPlaybackAndQueueIntent() {
        MpvController player(true, true);
        PlaylistController playlist(player);
        MprisPlayer mpris(player, playlist);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        QTRY_COMPARE(mpris.playbackStatus(), QStringLiteral("Playing"));

        const auto metadata = mpris.metadata();
        QVERIFY(metadata.value(QStringLiteral("xesam:title"))
                    .toString()
                    .contains(QStringLiteral("sample")));
        QVERIFY(metadata.value(QStringLiteral("mpris:length")).toLongLong() > 0);
        QVERIFY(mpris.canSeek());
        QVERIFY(mpris.canGoNext() == playlist.hasNext());

        mpris.pause();
        QTRY_VERIFY(player.state().paused);
        QCOMPARE(mpris.playbackStatus(), QStringLiteral("Paused"));
        mpris.play();
        QTRY_VERIFY(!player.state().paused);

        mpris.setVolume(0.5);
        QTRY_VERIFY(std::abs(player.state().volume - 50.0) < 0.01);
        mpris.setRate(1.5);
        QTRY_VERIFY(std::abs(player.state().playbackSpeed - 1.5) < 0.01);
        mpris.setShuffle(true);
        QVERIFY(playlist.shuffleEnabled());
        mpris.setLoopStatus(QStringLiteral("Track"));
        QCOMPARE(playlist.repeatMode(), PlaylistController::RepeatMode::One);
        QCOMPARE(mpris.loopStatus(), QStringLiteral("Track"));

        const auto before = mpris.position();
        mpris.seekRelative(500000);
        QVERIFY(mpris.position() >= before);
        mpris.stop();
        QTRY_COMPARE(mpris.playbackStatus(), QStringLiteral("Stopped"));
    }
};

QTEST_GUILESS_MAIN(MprisTest)
#include "mpris_test.moc"
