#include "mpvcontroller_p.h"

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
    if (pending)
        emit owner->chapterNavigationChanged();
}

void MpvController::Impl::selectChapter(int index) {
    if (!state.active || !state.seekable)
        return;
    const bool valid =
        std::any_of(chapters.cbegin(), chapters.cend(),
                    [index](const MediaChapter &chapter) { return chapter.index == index; });
    if (!valid)
        return;
    const auto id = command({"set", "chapter", QByteArray::number(index)});
    if (id) {
        requestedChapter = index;
        chapterCommand = id;
        chapterQuery = 0;
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
    // Setting mpv's chapter property updates its seek intent immediately. Query
    // after the latest command: observers can omit no-ops or coalesce fast clicks.
    chapterQuery = ++request;
    const int status =
        mpv_get_property_async(client.get(), chapterQuery, "chapter", MPV_FORMAT_INT64);
    if (status < 0)
        cancelChapterRequest();
    report(status);
}

bool MpvController::Impl::finishChapterQuery(const mpv_event &event) {
    if (!chapterQuery || event.reply_userdata != chapterQuery)
        return false;
    const auto &value = *static_cast<mpv_event_property *>(event.data);
    if (event.error >= 0 && value.format == MPV_FORMAT_INT64 && value.data)
        observeChapter(*static_cast<int64_t *>(value.data));
    // Finish mutations before signals: a receiver may issue the next request.
    cancelChapterRequest();
    emit owner->stateChanged();
    report(event.error);
    return true;
}
