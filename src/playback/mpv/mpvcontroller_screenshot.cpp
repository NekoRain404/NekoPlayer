#include "playback/mpv/mpvcontroller_p.h"

bool MpvController::canTakeScreenshot() const {
    return isReady() && !impl_->pendingPlayback && impl_->mediaReady && !impl_->loading &&
           !impl_->recoveringRenderer && impl_->state.active && impl_->information.videoWidth > 0 &&
           impl_->information.videoHeight > 0 && !screenshotPending();
}

bool MpvController::screenshotPending() const { return impl_->screenshotCommand != 0; }

bool MpvController::takeScreenshot(const QString &absolutePath, bool includeSubtitles) {
    const QFileInfo destination(absolutePath);
    const auto extension = destination.suffix().toLower();
    if (!canTakeScreenshot() || !destination.isAbsolute() || absolutePath.contains(QChar::Null) ||
        (extension != QLatin1String("png") && extension != QLatin1String("jpg") &&
         extension != QLatin1String("jpeg")))
        return false;

    // The client array API keeps filenames literal: no input.conf/property expansion.
    const auto [id, status] = impl_->submitCommand(
        {"screenshot-to-file", absolutePath.toUtf8(), includeSubtitles ? "subtitles" : "video"});
    if (!id) {
        impl_->report(status);
        return false;
    }
    impl_->screenshotCommand = id;
    impl_->screenshotPath = absolutePath;
    emit screenshotPendingChanged();
    return true;
}

bool MpvController::Impl::finishScreenshot(const mpv_event &event) {
    if (!screenshotCommand || event.reply_userdata != screenshotCommand)
        return false;
    const auto path = std::exchange(screenshotPath, {});
    const bool success = event.error >= 0;
    screenshotCommand = 0;
    report(event.error);
    // Slots may enqueue another screenshot; do not clear operation state after notifying.
    emit owner->screenshotPendingChanged();
    emit owner->screenshotFinished(path, success);
    return true;
}
