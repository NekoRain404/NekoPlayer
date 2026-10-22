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

    static void primeRecovery(MpvController &player, const QString &path, bool seekable) {
        primeExistingPlayback(player);
        player.impl_->state.path = path;
        player.impl_->playingPath = path;
        player.impl_->state.seekable = seekable;
    }

    static uint64_t beginSynchronousOpen(MpvController &player) {
        return ++player.impl_->openGeneration;
    }

    static void startFile(MpvController &player, int64_t entry) {
        player.impl_->playingEntry = entry;
        player.impl_->commitPendingOpen(entry);
    }

    static void fileLoaded(MpvController &player) { player.impl_->handleFileLoaded(); }

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

    static void endFile(MpvController &player, const mpv_event_end_file &event) {
        player.impl_->handleEndFile(event);
    }

    static void stageStaleEntry(MpvController &player, int64_t entry, uint64_t generation) {
        player.impl_->openGeneration = generation + 1;
        player.impl_->openGenerationsByEntry.insert(entry, generation);
    }

    static qsizetype pendingOpenCount(const MpvController &player) {
        return player.impl_->pendingOpens.size();
    }

    static QString playingPath(const MpvController &player) { return player.impl_->playingPath; }
};

class MpvOpenFailureTest final : public QObject {
    Q_OBJECT
  private slots:
    void rendererRecoveryRespectsStreamSeekability_data() {
        QTest::addColumn<QString>("path");
        QTest::addColumn<bool>("seekable");
        QTest::addColumn<double>("position");
        QTest::newRow("live-url") << QStringLiteral("rtsp://127.0.0.1:9/live") << false << 0.0;
        QTest::newRow("seekable-url")
            << QStringLiteral("http://127.0.0.1:9/movie.mkv") << true << 12.5;
        QTest::newRow("local-before-loaded") << QStringLiteral("/old/media.mkv") << false << 12.5;
    }

    void rendererRecoveryRespectsStreamSeekability() {
        QFETCH(QString, path);
        QFETCH(bool, seekable);
        QFETCH(double, position);
        MpvController player(true, true);
        QVERIFY2(player.isReady(), qPrintable(player.errorString()));
        MpvControllerTestAccess::primeRecovery(player, path, seekable);
        player.prepareForRendererReset();

        player.resumeAfterRendererReset();

        // Inspect the pending bookmark before asynchronous backend observations.
        QCOMPARE(player.resumePosition(), position);
    }

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

    void staleGenerationEndFileIsIgnored_data() {
        QTest::addColumn<int>("reason");
        QTest::addColumn<int>("error");
        QTest::newRow("error") << int(MPV_END_FILE_REASON_ERROR)
                               << int(MPV_ERROR_INVALID_PARAMETER);
        QTest::newRow("eof") << int(MPV_END_FILE_REASON_EOF) << 0;
        QTest::newRow("stop") << int(MPV_END_FILE_REASON_STOP) << 0;
    }

    void staleGenerationEndFileIsIgnored() {
        QFETCH(int, reason);
        QFETCH(int, error);
        MpvController player(true, true);
        QSignalSpy failed(&player, &MpvController::mediaLoadFailed);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        QSignalSpy finished(&player, &MpvController::playbackFinished);
        MpvControllerTestAccess::primeExistingPlayback(player);
        MpvControllerTestAccess::stageStaleEntry(player, 101, 1);
        mpv_event_end_file end{};
        end.reason = static_cast<mpv_end_file_reason>(reason);
        end.error = error;
        end.playlist_entry_id = 101;

        MpvControllerTestAccess::endFile(player, end);

        QVERIFY(failed.isEmpty());
        QVERIFY(errors.isEmpty());
        QVERIFY(finished.isEmpty());
        QVERIFY(player.state().active);
        QCOMPARE(player.state().path, QStringLiteral("/old/media.mkv"));
    }

    void fileLoadedBeforeStaleOpenReplyIsIgnored() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        MpvControllerTestAccess::primeExistingPlayback(player);
        MpvControllerTestAccess::stageOpen(player, QStringLiteral("/older.mkv"), 41);
        MpvControllerTestAccess::stageOpen(player, QStringLiteral("/newer.mkv"), 42);
        MpvControllerTestAccess::startFile(player, 101);
        MpvControllerTestAccess::fileLoaded(player);
        QVERIFY(loaded.isEmpty());

        MpvControllerTestAccess::commandReply(player, 41, 0, 101);

        QVERIFY(loaded.isEmpty());
        QCOMPARE(MpvControllerTestAccess::pendingOpenCount(player), 1);
    }

    void currentFileLoadedBeforeCommandReplyIsDeliveredAfterCorrelation() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        MpvControllerTestAccess::stageOpen(player, QStringLiteral("/current.mkv"), 41);
        MpvControllerTestAccess::startFile(player, 101);
        MpvControllerTestAccess::fileLoaded(player);
        QVERIFY(loaded.isEmpty());

        MpvControllerTestAccess::commandReply(player, 41, 0, 101);

        QCOMPARE(loaded.count(), 1);
        QVERIFY(player.state().active);
    }

    void endFileBeforeCommandReplyStillResolvesStartedOpen() {
        MpvController player(true, true);
        QSignalSpy failed(&player, &MpvController::mediaLoadFailed);
        MpvControllerTestAccess::stageOpen(player, QStringLiteral("/failed.mkv"), 41);
        MpvControllerTestAccess::startFile(player, 101);
        mpv_event_end_file end{};
        end.reason = MPV_END_FILE_REASON_ERROR;
        end.error = MPV_ERROR_INVALID_PARAMETER;
        end.playlist_entry_id = 101;

        MpvControllerTestAccess::endFile(player, end);
        MpvControllerTestAccess::commandReply(player, 41, 0, 101);

        QCOMPARE(MpvControllerTestAccess::pendingOpenCount(player), 0);
        QCOMPARE(MpvControllerTestAccess::playingPath(player), QStringLiteral("/failed.mkv"));
        QVERIFY(!player.state().active);
        QCOMPARE(failed.count(), 1);
    }

    void reentrantOpenDuringFileLoadedSuppressesOlderNotification() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        MpvControllerTestAccess::stageOpen(player, QStringLiteral("/older.mkv"), 41);
        MpvControllerTestAccess::commandReply(player, 41, 0, 101);
        MpvControllerTestAccess::startFile(player, 101);
        connect(
            &player, &MpvController::stateChanged, &player,
            [&] {
                if (MpvControllerTestAccess::pendingOpenCount(player) == 0)
                    MpvControllerTestAccess::stageOpen(player, QStringLiteral("/newer.mkv"), 42);
            },
            Qt::DirectConnection);

        MpvControllerTestAccess::fileLoaded(player);

        QVERIFY(loaded.isEmpty());
        QCOMPARE(MpvControllerTestAccess::pendingOpenCount(player), 1);
    }

    void reentrantOpenDuringEndFileSuppressesOlderTerminalNotification() {
        MpvController player(true, true);
        QSignalSpy finished(&player, &MpvController::playbackFinished);
        MpvControllerTestAccess::stageOpen(player, QStringLiteral("/older.mkv"), 41);
        MpvControllerTestAccess::commandReply(player, 41, 0, 101);
        MpvControllerTestAccess::startFile(player, 101);
        connect(
            &player, &MpvController::stateChanged, &player,
            [&] {
                if (MpvControllerTestAccess::pendingOpenCount(player) == 0)
                    MpvControllerTestAccess::stageOpen(player, QStringLiteral("/newer.mkv"), 42);
            },
            Qt::DirectConnection);
        mpv_event_end_file end{};
        end.reason = MPV_END_FILE_REASON_EOF;
        end.playlist_entry_id = 101;

        MpvControllerTestAccess::endFile(player, end);

        QVERIFY(finished.isEmpty());
        QCOMPARE(MpvControllerTestAccess::pendingOpenCount(player), 1);
    }

    void reentrantOpenDuringRejectedOpenSuppressesOlderFailure() {
        MpvController player(true, true);
        QSignalSpy failed(&player, &MpvController::mediaLoadFailed);
        MpvControllerTestAccess::stageOpen(player, QStringLiteral("/older.mkv"), 41);
        connect(
            &player, &MpvController::stateChanged, &player,
            [&] {
                if (MpvControllerTestAccess::pendingOpenCount(player) == 0)
                    MpvControllerTestAccess::stageOpen(player, QStringLiteral("/newer.mkv"), 42);
            },
            Qt::DirectConnection);

        MpvControllerTestAccess::commandReply(player, 41, MPV_ERROR_INVALID_PARAMETER);

        QVERIFY(failed.isEmpty());
        QCOMPARE(MpvControllerTestAccess::pendingOpenCount(player), 1);
    }
};

QTEST_GUILESS_MAIN(MpvOpenFailureTest)
#include "mpv_open_failure_test.moc"
