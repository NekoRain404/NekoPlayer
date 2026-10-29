#include "playback/mpv/mpvcontroller_p.h"

bool MpvController::canStepFrameForward() const {
    return isReady() && !impl_->pendingPlayback && impl_->mediaReady &&
           !impl_->recoveringRenderer && state().active && impl_->hasMotionVideo;
}

bool MpvController::canStepFrameBackward() const {
    return canStepFrameForward() && state().seekable;
}

void MpvController::stepFrameForward() {
    if (!canStepFrameForward())
        return;
    const auto [id, status] = impl_->submitCommand({"frame-step"});
    if (id)
        impl_->cancelChapterRequest();
    impl_->report(status);
}

void MpvController::stepFrameBackward() {
    if (!canStepFrameBackward())
        return;
    const auto [id, status] = impl_->submitCommand({"frame-back-step"});
    if (id)
        impl_->cancelChapterRequest();
    impl_->report(status);
}

int MpvController::chapterNavigationIndex() const {
    return impl_->requestedChapter.value_or(state().currentChapter);
}

void MpvController::selectChapter(int index) { impl_->selectChapter(index); }

void MpvController::nextChapter() { selectChapter(chapterNavigationIndex() + 1); }

void MpvController::previousChapter() { selectChapter(chapterNavigationIndex() - 1); }

void MpvController::Impl::cancelChapterRequest() {
    const bool pending = requestedChapter.has_value();
    requestedChapter.reset();
    chapterCommand = chapterQuery = 0;
    chapterSeekStarted = chapterQuerySettled = false;
    if (pending)
        emit owner->chapterNavigationChanged();
}

void MpvController::Impl::selectChapter(int index) {
    if (!state.active || !state.seekable)
        return;
    const auto chapter =
        std::find_if(chapters.cbegin(), chapters.cend(),
                     [index](const MediaChapter &chapter) { return chapter.index == index; });
    if (chapter == chapters.cend())
        return;
    // mpv 0.41 can leave chapter unavailable if an ordinary seek supersedes
    // a queued native chapter seek. Use the owned chapter timestamp instead.
    const auto id =
        command({"seek", QByteArray::number(chapter->start, 'g', 17), "absolute+exact"});
    if (id) {
        requestedChapter = index;
        chapterCommand = id;
        chapterQuery = 0;
        chapterSeekStarted = chapterQuerySettled = false;
        emit owner->chapterNavigationChanged();
    }
}

void MpvController::Impl::confirmChapterRequest(const mpv_event &event) {
    if (!chapterCommand || event.reply_userdata != chapterCommand)
        return;
    chapterCommand = 0;
    if (event.error < 0) {
        cancelChapterRequest();
        return;
    }
    queryChapterRequest();
}

void MpvController::Impl::queryChapterRequest(bool settled) {
    if (!requestedChapter || chapterCommand)
        return;
    // A seek reply only acknowledges queueing. Confirm no-ops now, and query
    // again after playback restarts if the seek has not reached its target yet.
    chapterQuery = ++request;
    chapterQuerySettled = settled;
    const int status =
        mpv_get_property_async(client.get(), chapterQuery, "chapter", MPV_FORMAT_INT64);
    if (status < 0)
        cancelChapterRequest();
    report(status);
}

bool MpvController::Impl::finishChapterQuery(const mpv_event &event) {
    if (!chapterQuery || event.reply_userdata != chapterQuery)
        return false;
    const bool settled = chapterQuerySettled;
    chapterQuery = 0;
    chapterQuerySettled = false;
    const auto &value = *static_cast<mpv_event_property *>(event.data);
    if (event.error >= 0 && value.format == MPV_FORMAT_INT64 && value.data)
        observeChapter(*static_cast<int64_t *>(value.data), settled);
    if (event.error < 0)
        cancelChapterRequest();
    emit owner->stateChanged();
    report(event.error);
    return true;
}

void MpvController::Impl::observeChapter(int64_t index, bool settled) {
    state.currentChapter =
        (state.active || loading) && index >= 0 && index < INT_MAX ? static_cast<int>(index) : -1;
    // Shared timestamps or frame alignment can land in a different chapter.
    // A restart after this command's SEEK confirms that actual destination.
    if (settled || (!chapterCommand && requestedChapter == state.currentChapter))
        cancelChapterRequest();
}
