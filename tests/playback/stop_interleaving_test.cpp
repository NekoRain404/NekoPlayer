#include "playback/mpv/mpvcontroller.h"
#include "playback/mpv/mpvcontroller_p.h"

#include <QSignalSpy>
#include <QtTest>

// Regression coverage for events that are already queued when the user stops.
// mpv delivers END_FILE/FILE_LOADED asynchronously; after stop() they must not
// be attributed to the current (stopped) generation.
class StopInterleavingTest final : public QObject {
    Q_OBJECT
  private slots:
    void queuedEofForInvalidatedEntryDoesNotFinishPlayback() {
        MpvController player(true, true);
        MpvController::Impl impl(&player);
        impl.playingEntry = 7;
        impl.openGenerationsByEntry.insert(7, impl.openGeneration);
        impl.invalidateEntry(7);
        impl.playingEntry = -1;

        QSignalSpy finished(&player, &MpvController::playbackFinished);
        mpv_event_end_file end{};
        end.playlist_entry_id = 7;
        end.reason = MPV_END_FILE_REASON_EOF;
        impl.handleEndFile(end);
        QCOMPARE(finished.count(), 0);
    }

    void queuedFileLoadedAfterStopDoesNotActivate() {
        MpvController player(true, true);
        MpvController::Impl impl(&player);
        impl.playingEntry = -1;
        impl.invalidateEntry(5);

        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        impl.handleFileLoaded();
        QCOMPARE(loaded.count(), 0);
    }

    void currentEntryEofStillFinishesPlayback() {
        MpvController player(true, true);
        MpvController::Impl impl(&player);
        impl.playingEntry = 9;
        impl.openGenerationsByEntry.insert(9, impl.openGeneration);

        QSignalSpy finished(&player, &MpvController::playbackFinished);
        mpv_event_end_file end{};
        end.playlist_entry_id = 9;
        end.reason = MPV_END_FILE_REASON_EOF;
        impl.handleEndFile(end);
        QCOMPARE(finished.count(), 1);
    }
};

QTEST_GUILESS_MAIN(StopInterleavingTest)
#include "stop_interleaving_test.moc"
