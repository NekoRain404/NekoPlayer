#include "playback/mpv/mpvcontroller_p.h"

const VideoPresentation &MpvController::videoPresentation() const { return impl_->presentation; }

void MpvController::setVideoAspect(VideoAspect aspect) {
    const char *ratio = nullptr;
    switch (aspect) {
    case VideoAspect::Source:
    case VideoAspect::Stretch:
        ratio = "no";
        break;
    case VideoAspect::Ratio4x3:
        ratio = "4:3";
        break;
    case VideoAspect::Ratio16x9:
        ratio = "16:9";
        break;
    case VideoAspect::Ratio16x10:
        ratio = "16:10";
        break;
    case VideoAspect::Ratio21x9:
        ratio = "21:9";
        break;
    default:
        return;
    }
    // Only fixed, validated options reach the backend. Record accepted intent,
    // not execution success; asynchronous failures use the normal error signal.
    // Always resubmit both setters so reselecting a choice also repairs a
    // partially enqueued request after reply-queue exhaustion.
    if (impl_->command({"set", "video-aspect-override", ratio}) &&
        impl_->command({"set", "keepaspect", aspect == VideoAspect::Stretch ? "no" : "yes"}) &&
        aspect != impl_->presentation.aspect) {
        impl_->presentation.aspect = aspect;
        emit videoPresentationChanged();
    }
}

void MpvController::setVideoRotation(int clockwiseDegrees) {
    if (clockwiseDegrees % 90 != 0)
        return;
    const int rotation = (clockwiseDegrees % 360 + 360) % 360;
    if (rotation == impl_->presentation.rotation)
        return;
    if (impl_->command({"set", "video-rotate", QByteArray::number(rotation)})) {
        impl_->presentation.rotation = rotation;
        emit videoPresentationChanged();
    }
}

void MpvController::rotateVideoClockwise() { setVideoRotation(impl_->presentation.rotation + 90); }

void MpvController::rotateVideoCounterClockwise() {
    setVideoRotation(impl_->presentation.rotation - 90);
}
