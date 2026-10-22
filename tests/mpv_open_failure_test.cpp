#include "playback/mpvcontroller_p.h"

#include <QSignalSpy>
#include <QtTest>

class MpvControllerTestAccess final {
  public:
    static void primeExistingPlayback(MpvController &player) {
        auto &impl = *player.impl_;
        impl.state.active = true;
        impl.state.path = QStringLiteral("/old/media.mkv");
        impl.playingPath = impl.state.path;
        impl.state.position = 12.5;
        impl.state.paused = true;
        impl.mediaReady = true;
    }

    static uint64_t stageOpen(MpvController &player, QString path, uint64_t command) {
        auto &impl = *player.impl_;
        const auto generation = ++impl.openGeneration;
        impl.pendingOpens.append({command, generation, std::move(path)});
        impl.state.path = impl.pendingOpens.back().path;
        impl.openingPosition = 3.0;
        impl.openingCommand = command;
        return generation;
    }

    static uint64_t beginSynchronousOpen(MpvController &player) {
        return ++player.impl_->openGeneration;
    }

    static void startFile(MpvController &player, int64_t entry) {
        player.impl_->commitPendingOpen(entry);
    }

    static void commandReply(MpvController &player, uint64_t command, int status,
                             int64_t entry = -1) {
        char key[] = "playlist_entry_id";
        char *keys[] = {key};
        mpv_node value{};
        value.format = MPV_FORMAT_INT64;
        value.u.int64 = entry;
        mpv_node values[] = {value};
        mpv_node_list list{};
        list.num = 1;
        list.keys = keys;
        list.values = values;
        mpv_event_command result{};
        result.result.format = MPV_FORMAT_NODE_MAP;
        result.result.u.list = &list;
        mpv_event event{};
        event.reply_userdata = command;
        event.error = status;
        event.data = &result;
        player.impl_->handleOpeningCommandReply(event);
    }

    static void synchronousFailure(MpvController &player, uint64_t generation, int status) {
        player.impl_->notifySynchronousOpenFailure(generation, status);
    }

    static qsizetype pendingOpenCount(const MpvController &player) {
        return player.impl_->pendingOpens.size();
    }

    static QString playingPath(const MpvController &player) { return player.impl_->playingPath; }
};

class MpvOpenFailureTest final : public QObject {
    Q_OBJECT
  private slots:
    void rejectedOpenNotifiesQueueAndKeepsExistingPlayback() {
        MpvController player(true, true);
        QSignalSpy failed(&player, &MpvController::mediaLoadFailed);
        MpvControllerTestAccess::primeExistingPlayback(player);
        MpvControllerTestAccess::stageOpen(player, QStringLiteral("/new/media.mkv"), 41);
        MpvControllerTestAccess::commandReply(player, 41, MPV_ERROR_INVALID_PARAMETER);

        QCOMPARE(failed.count(), 1);
        QCOMPARE(player.state().path, QStringLiteral("/old/media.mkv"));
        QCOMPARE(player.state().position, 12.5);
        QVERIFY(player.state().active);
        QVERIFY(player.state().paused);
    }

    void rejectedCommandReplyNotifiesQueueAndKeepsExistingPlayback() {
        MpvController player(true, true);
        QSignalSpy failed(&player, &MpvController::mediaLoadFailed);
        MpvControllerTestAccess::primeExistingPlayback(player);
        MpvControllerTestAccess::stageOpen(player, QStringLiteral("/new/media.mkv"), 42);

        MpvControllerTestAccess::commandReply(player, 42, MPV_ERROR_INVALID_PARAMETER);

        QCOMPARE(failed.count(), 1);
        QCOMPARE(player.state().path, QStringLiteral("/old/media.mkv"));
        QCOMPARE(player.state().position, 12.5);
        QVERIFY(player.state().active);
        QVERIFY(player.state().paused);
    }

    void overlappingOpensMatchStartEventsByPlaylistEntry() {
        MpvController player(true, true);
        MpvControllerTestAccess::primeExistingPlayback(player);
        MpvControllerTestAccess::stageOpen(player, QStringLiteral("/first.mkv"), 41);
        MpvControllerTestAccess::stageOpen(player, QStringLiteral("/second.mkv"), 42);

        MpvControllerTestAccess::startFile(player, 101);
        MpvControllerTestAccess::commandReply(player, 42, 0, 202);
        MpvControllerTestAccess::commandReply(player, 41, 0, 101);
        QCOMPARE(MpvControllerTestAccess::playingPath(player), QStringLiteral("/first.mkv"));
        QCOMPARE(player.state().path, QStringLiteral("/second.mkv"));
        MpvControllerTestAccess::startFile(player, 202);
        QCOMPARE(MpvControllerTestAccess::playingPath(player), QStringLiteral("/second.mkv"));
    }

    void synchronousFailureDoesNotRejectReentrantNewerOpen() {
        MpvController player(true, true);
        QSignalSpy failed(&player, &MpvController::mediaLoadFailed);
        const auto generation = MpvControllerTestAccess::beginSynchronousOpen(player);
        connect(
            &player, &MpvController::fileOpenRequested, &player,
            [&] { MpvControllerTestAccess::stageOpen(player, QStringLiteral("/second.mkv"), 42); },
            Qt::DirectConnection);
        emit player.fileOpenRequested();

        MpvControllerTestAccess::synchronousFailure(player, generation,
                                                    MPV_ERROR_INVALID_PARAMETER);

        QCOMPARE(failed.count(), 0);
        QCOMPARE(MpvControllerTestAccess::pendingOpenCount(player), 1);
    }
};

QTEST_GUILESS_MAIN(MpvOpenFailureTest)
#include "mpv_open_failure_test.moc"
