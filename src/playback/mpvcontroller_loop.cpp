#include "mpvcontroller_p.h"

const AbLoop &MpvController::abLoop() const { return impl_->abLoop; }

bool MpvController::setLoopStart(double seconds) {
    if (!impl_->mediaReady || !state().active || !state().seekable || !std::isfinite(seconds) ||
        seconds < 0 || seconds >= state().duration || (abLoop().end && seconds >= *abLoop().end))
        return false;
    if (!impl_->command({"set", "ab-loop-a", QByteArray::number(seconds, 'g', 17)}))
        return false;
    if (impl_->abLoop.start != seconds) {
        impl_->abLoop.start = seconds;
        emit abLoopChanged();
    }
    return true;
}

bool MpvController::setLoopEnd(double seconds) {
    if (!impl_->mediaReady || !state().active || !state().seekable || !std::isfinite(seconds) ||
        !abLoop().start || seconds <= *abLoop().start || seconds > state().duration)
        return false;
    if (!impl_->command({"set", "ab-loop-b", QByteArray::number(seconds, 'g', 17)}))
        return false;
    if (impl_->abLoop.end != seconds) {
        impl_->abLoop.end = seconds;
        emit abLoopChanged();
    }
    return true;
}

bool MpvController::clearAbLoop() {
    // Resubmit even if intent is empty: this also repairs a previously rejected
    // clear. Clearing either endpoint disables native looping immediately.
    const auto previous = impl_->abLoop;
    const bool startCleared = impl_->command({"set", "ab-loop-a", "no"}) != 0;
    if (startCleared)
        impl_->abLoop.start.reset();
    const bool endCleared = startCleared && impl_->command({"set", "ab-loop-b", "no"}) != 0;
    if (endCleared)
        impl_->abLoop.end.reset();
    if (previous != impl_->abLoop)
        emit abLoopChanged();
    return startCleared && endCleared;
}

void MpvController::Impl::resetAbLoop() {
    if (recoveringRenderer)
        return;
    if (abLoop == AbLoop{})
        return;
    abLoop = {};
    int startStatus = 0;
    int endStatus = 0;
    if (ready) {
        // Finish old cleanup submissions before any signal can load new media.
        startStatus = submitCommand({"set", "ab-loop-a", "no"}).second;
        endStatus = submitCommand({"set", "ab-loop-b", "no"}).second;
    }
    emit owner->abLoopChanged();
    report(startStatus);
    report(endStatus);
}
