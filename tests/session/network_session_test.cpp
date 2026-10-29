#include "playback/mpv/mpvcontroller.h"
#include "playback/playlist/playlistcontroller.h"
#include "playback/playlist/playlistmodel.h"
#include "session/playbackhistorystore.h"
#include "session/sessionmanager.h"
#include "session/sessionstore.h"

#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

class NetworkSessionTest final : public QObject {
    Q_OBJECT

  private slots:
    void mixedSourcesRoundTripWithoutChangingUrlIdentity() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        const auto local = temporary.filePath(QStringLiteral("猫 sample.mkv"));
        const QStringList sources{
            local,
            QStringLiteral("http://example.com/video//stream.ts?token=a%2Fb&mode=live"),
            QStringLiteral("https://example.com/%E7%8C%AB%20sample.mkv#part"),
            QStringLiteral("rtsp://example.com:8554/live"),
            QStringLiteral("rtsps://example.com:322/live"),
            QStringLiteral("rtmp://example.com/live/channel"),
            QStringLiteral("rtmps://example.com:443/live/channel"),
            QStringLiteral("http://example.com/video//stream.ts?token=a%2Fb&mode=live")};
        SessionState saved;
        saved.paths = sources;
        saved.currentIndex = sources.size() - 1;
        saved.resumePosition = 12.5;

        QVERIFY(SessionStore(file).save(saved).isEmpty());
        const auto loaded = SessionStore(file).load();
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.state.paths, sources);
        QCOMPARE(loaded.state.currentIndex, 7);
        QCOMPARE(loaded.state.resumePosition, 0.0);
    }

    void fileUrlsBecomeLocalPaths() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        const auto local = temporary.filePath(QStringLiteral("猫 sample [1].mkv"));
        SessionState saved;
        saved.paths = {QUrl::fromLocalFile(local).toString(QUrl::FullyEncoded)};
        saved.currentIndex = 0;

        QVERIFY(SessionStore(file).save(saved).isEmpty());
        const auto loaded = SessionStore(file).load();
        QVERIFY(loaded.error.isEmpty());
        QCOMPARE(loaded.state.paths, QStringList{local});
        QCOMPARE(loaded.state.currentIndex, 0);
    }

    void malformedStoredSourcesAreRemovedWithoutMovingBookmark_data() {
        QTest::addColumn<QString>("invalid");
        QTest::newRow("relative") << QStringLiteral("relative/video.mkv");
        QTest::newRow("missing-host") << QStringLiteral("https:///video.mkv");
        QTest::newRow("broken-ipv6") << QStringLiteral("rtsp://[broken/live");
        QTest::newRow("bad-port") << QStringLiteral("http://example.com:wrong/live");
        QTest::newRow("bad-percent") << QStringLiteral("https://example.com/%zz");
        QTest::newRow("unsupported-scheme") << QStringLiteral("ftp://example.com/video.mkv");
        QTest::newRow("relative-file-url") << QStringLiteral("file:relative/video.mkv");
        QTest::newRow("encoded-nul-file-url") << QStringLiteral("file:///tmp/%00movie.mkv");
        QTest::newRow("nul") << QStringLiteral("http://example.com/") + QChar::Null;
    }

    void malformedStoredSourcesAreRemovedWithoutMovingBookmark() {
        QFETCH(QString, invalid);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        const auto local = temporary.filePath(QStringLiteral("video.mkv"));
        const auto network = QStringLiteral("https://example.com/live?token=a%2Fb");
        {
            QSettings settings(file, QSettings::IniFormat);
            settings.setValue(QStringLiteral("schemaVersion"), 1);
            settings.setValue(QStringLiteral("playlist/paths"),
                              QStringList{invalid, network, local, local});
            settings.setValue(QStringLiteral("playlist/currentIndex"), 3);
            settings.setValue(QStringLiteral("playlist/resumePosition"), 14.0);
            settings.sync();
            QCOMPARE(settings.status(), QSettings::NoError);
        }

        const auto loaded = SessionStore(file).load();
        QVERIFY(loaded.error.isEmpty());
        QCOMPARE(loaded.state.paths, QStringList({network, local, local}));
        QCOMPARE(loaded.state.currentIndex, 2);
        QCOMPARE(loaded.state.resumePosition, 14.0);
    }

    void selectedNetworkSourceDiscardsStoredResumePosition_data() {
        QTest::addColumn<QString>("source");
        QTest::newRow("http") << QStringLiteral("http://example.com/video.mkv");
        QTest::newRow("https") << QStringLiteral("https://example.com/video.mkv");
        QTest::newRow("rtsp") << QStringLiteral("rtsp://example.com/live");
        QTest::newRow("rtmp") << QStringLiteral("rtmp://example.com/live/channel");
    }

    void selectedNetworkSourceDiscardsStoredResumePosition() {
        QFETCH(QString, source);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        {
            QSettings settings(file, QSettings::IniFormat);
            settings.setValue(QStringLiteral("schemaVersion"), 1);
            settings.setValue(QStringLiteral("playlist/paths"), QStringList{source});
            settings.setValue(QStringLiteral("playlist/currentIndex"), 0);
            settings.setValue(QStringLiteral("playlist/resumePosition"), 45.0);
            settings.sync();
            QCOMPARE(settings.status(), QSettings::NoError);
        }

        const auto loaded = SessionStore(file).load();
        QVERIFY(loaded.error.isEmpty());
        QCOMPARE(loaded.state.paths, QStringList{source});
        QCOMPARE(loaded.state.currentIndex, 0);
        QCOMPARE(loaded.state.resumePosition, 0.0);
    }

    void removingInvalidSelectedSourceDiscardsItsBookmark() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        const auto network = QStringLiteral("rtsp://example.com/live");
        SessionState saved;
        saved.paths = {QStringLiteral("https:///invalid"), network};
        saved.currentIndex = 0;
        saved.resumePosition = 45.0;

        QVERIFY(SessionStore(file).save(saved).isEmpty());
        const auto loaded = SessionStore(file).load();
        QVERIFY(loaded.error.isEmpty());
        QCOMPARE(loaded.state.paths, QStringList{network});
        QCOMPARE(loaded.state.currentIndex, 0);
        QCOMPARE(loaded.state.resumePosition, 0.0);
    }

    void rememberingNetworkSourcesPreservesLocalDirectory() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QVERIFY(session.restore().error.isEmpty());
        session.rememberMedia(temporary.filePath(QStringLiteral("local.mkv")));

        session.rememberMedia(QStringLiteral("https://example.com/live/video.mkv"));
        QCOMPARE(session.mediaDirectory(), temporary.path());
        session.rememberMedia(QStringLiteral("rtsp://example.com:8554/live"));
        QCOMPARE(session.mediaDirectory(), temporary.path());
        QVERIFY(session.save({}).isEmpty());
        QCOMPARE(SessionStore(file).load().state.window.mediaDirectory, temporary.path());
    }

    void rememberingFileUrlUsesItsLocalDirectory() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QVERIFY(session.restore().error.isEmpty());

        const auto local = temporary.filePath(QStringLiteral("猫 sample.mkv"));
        session.rememberMedia(QUrl::fromLocalFile(local).toString(QUrl::FullyEncoded));
        QCOMPARE(session.mediaDirectory(), temporary.path());
    }

    void pendingNetworkSourcesSurviveSaveAndRestoreWithoutPlayback() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        const QStringList sources{QStringLiteral("https://example.com/live.m3u8"),
                                  QStringLiteral("rtmp://example.com/live/channel")};
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QSignalSpy requested(&player, &MpvController::fileOpenRequested);
        QVERIFY(session.restore().error.isEmpty());
        QVERIFY(session.save({}, sources).isEmpty());

        const auto restored = session.restore();
        QVERIFY(restored.error.isEmpty());
        QCOMPARE(queue.model()->paths(), sources);
        QCOMPARE(queue.model()->currentIndex(), 0);
        QVERIFY(requested.isEmpty());
        QVERIFY(!player.state().active);
    }

    void networkSnapshotsAreExcludedFromLocalPlaybackHistory() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        const auto local = temporary.filePath(QStringLiteral("local.mkv"));
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QVERIFY(session.restore().error.isEmpty());
        queue.playbackPositionSnapshot(local, 12.0, 120.0);
        queue.playbackPositionSnapshot(QStringLiteral("https://example.com/video.mkv"), 15.0,
                                       120.0);
        queue.playbackPositionSnapshot(QStringLiteral("rtsp://example.com/live"), 20.0, 120.0);
        QVERIFY(session.save({}).isEmpty());

        const auto history = PlaybackHistoryStore(file + QStringLiteral(".history.ini")).load();
        QVERIFY(history.error.isEmpty());
        QCOMPARE(history.entries.size(), 1);
        QCOMPARE(history.entries.first().path, local);
        QCOMPARE(history.entries.first().position, 12.0);
    }
};

QTEST_GUILESS_MAIN(NetworkSessionTest)
#include "network_session_test.moc"
