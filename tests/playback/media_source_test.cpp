#include "playback/media/mediasource.h"
#include "playback/playlist/playlistmodel.h"

#include <QtTest>

class MediaSourceTest final : public QObject {
    Q_OBJECT
  private slots:
    void networkAddresses_data() {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");
        QTest::newRow("signed") << "https://example.org/a%2Fb.mkv?sig=a%2Bb%3D&x=1+2"
                                << "https://example.org/a%2Fb.mkv?sig=a%2Bb%3D&x=1+2";
        QTest::newRow("signed-escapes") << "https://example.org/%7e.mkv?sig=a%2fb%41"
                                        << "https://example.org/%7e.mkv?sig=a%2fb%41";
        QTest::newRow("trim") << "  https://example.org/video.mkv  "
                              << "https://example.org/video.mkv";
        QTest::newRow("unicode") << "https://example.org/猫.mkv"
                                 << "https://example.org/%E7%8C%AB.mkv";
        QTest::newRow("rtsp") << "rtsp://127.0.0.1:8554/live" << "rtsp://127.0.0.1:8554/live";
        QTest::newRow("rtmp") << "rtmp://example.org/live/key" << "rtmp://example.org/live/key";
        QTest::newRow("ipv6") << "http://[::1]:8080/video" << "http://[::1]:8080/video";
        QTest::newRow("empty") << "" << "";
        QTest::newRow("hostless") << "https:///movie.mkv" << "";
        QTest::newRow("missing-scheme") << "example.org/video" << "";
        QTest::newRow("bad-escape") << "https://example.org/%xx" << "";
        QTest::newRow("bad-port") << "http://example.org:99999/movie" << "";
        QTest::newRow("unsupported") << "ftp://example.org/video" << "";
        QTest::newRow("local") << "/tmp/video.mkv" << "";
        QTest::newRow("newline") << "https://example.org/a\nb" << "";
        QTest::newRow("space") << "https://example.org/a b" << "";
    }

    void networkAddresses() {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(MediaSource::networkUrl(input), expected);
        QCOMPARE(MediaSource::normalize(expected), expected);
    }

    void localAndNetworkNormalization() {
        QCOMPARE(MediaSource::normalize(QStringLiteral("/tmp/a/../猫.mkv")),
                 QStringLiteral("/tmp/猫.mkv"));
        QCOMPARE(MediaSource::normalize(QStringLiteral("file:///tmp/%E7%8C%AB.mkv")),
                 QStringLiteral("/tmp/猫.mkv"));
        QCOMPARE(MediaSource::normalize(QStringLiteral("https://example.org/a/../b?x=/../")),
                 QStringLiteral("https://example.org/a/../b?x=/../"));
        QVERIFY(MediaSource::normalize(QStringLiteral("http:missing-host")).isEmpty());
        QVERIFY(MediaSource::normalize(QStringLiteral("file:///tmp/video?x=1")).isEmpty());
        QVERIFY(MediaSource::normalize(QStringLiteral("file://localhost/tmp/video.mkv")).isEmpty());
        QVERIFY(MediaSource::normalize(QStringLiteral("file://server/share/video.mkv")).isEmpty());
        QVERIFY(MediaSource::normalize(QStringLiteral("file:///tmp/%00movie.mkv")).isEmpty());
        QVERIFY(MediaSource::normalize(QStringLiteral("a") + QChar::Null).isEmpty());
    }

    void playlistShowsMediaNameWithoutUrlQuery() {
        PlaylistModel model;
        model.addFiles({QStringLiteral("https://example.org/video/movie.mkv?token=abc/secret")});
        QCOMPARE(model.data(model.index(0), Qt::DisplayRole).toString(),
                 QStringLiteral("movie.mkv"));
        QCOMPARE(model.pathAt(0),
                 QStringLiteral("https://example.org/video/movie.mkv?token=abc/secret"));
    }

    void playlistUsesHostForRootUrl() {
        PlaylistModel model;
        model.addFiles({QStringLiteral("https://example.org/")});
        QCOMPARE(model.data(model.index(0), Qt::DisplayRole).toString(),
                 QStringLiteral("example.org"));
    }
};

QTEST_GUILESS_MAIN(MediaSourceTest)
#include "media_source_test.moc"
