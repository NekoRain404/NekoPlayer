#include "playback/mpv/mpvcontroller.h"
#include "playback/playlist/playlistcontroller.h"
#include "session/playbackhistorystore.h"
#include "session/sessionmanager.h"

#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>

class PlaybackHistorySessionTest final : public QObject {
    Q_OBJECT

  private slots:
    void evictedHistoryDoesNotResumeInCurrentSession() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QVERIFY(session.restore().error.isEmpty());
        queue.playbackPositionSnapshot(media, 1.24, 4.0);
        QTest::qWait(2);
        for (int index = 1; index < 101; ++index) {
            const auto path = temporary.filePath(QStringLiteral("item-%1.mkv").arg(index));
            queue.playbackPositionSnapshot(path, 2.0, 4.0);
        }

        const auto entries =
            PlaybackHistoryStore(file + QStringLiteral(".history.ini")).load().entries;
        QCOMPARE(entries.size(), 100);
        QVERIFY(std::none_of(entries.cbegin(), entries.cend(), [&](const auto &entry) {
            return entry.path == PlaybackHistoryStore::normalizedPath(media);
        }));

        queue.restoreFiles({media}, 0);
        connect(&player, &MpvController::fileLoaded, &player, [&] { player.setPaused(true); });
        queue.playOrPause();
        QTRY_VERIFY(player.state().active && player.state().paused);
        QVERIFY(player.state().position < 0.08);
    }
};

QTEST_GUILESS_MAIN(PlaybackHistorySessionTest)
#include "playback_history_session_test.moc"
