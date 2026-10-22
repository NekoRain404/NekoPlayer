#include "mpvcontroller_p.h"

#include <QPointer>

const VideoFraming &MpvController::videoFraming() const { return impl_->framing; }

void MpvController::Impl::setFramingComponent(const char *name, double value,
                                              double VideoFraming::*member, double minimum,
                                              double maximum) {
    if (!ready || !std::isfinite(value))
        return;
    value = std::clamp(value, minimum, maximum);
    // Resubmit even identical intent: the previous asynchronous command may
    // have failed. Only accepted submissions update the UI's accumulation base.
    const auto [id, status] = submitCommand({"set", name, QByteArray::number(value, 'g', 17)});
    if (!id) {
        report(status);
        return;
    }
    if (framing.*member != value) {
        framing.*member = value;
        emit owner->videoFramingChanged();
    }
}

void MpvController::setVideoZoom(double log2Scale) {
    impl_->setFramingComponent("video-zoom", log2Scale, &VideoFraming::zoom, -2, 3);
}

void MpvController::adjustVideoZoom(double delta) { setVideoZoom(impl_->framing.zoom + delta); }

void MpvController::setVideoPanX(double offset) {
    impl_->setFramingComponent("video-pan-x", offset, &VideoFraming::panX, -1, 1);
}

void MpvController::adjustVideoPanX(double delta) { setVideoPanX(impl_->framing.panX + delta); }

void MpvController::setVideoPanY(double offset) {
    impl_->setFramingComponent("video-pan-y", offset, &VideoFraming::panY, -1, 1);
}

void MpvController::adjustVideoPanY(double delta) { setVideoPanY(impl_->framing.panY + delta); }

void MpvController::resetVideoFraming() {
    if (!impl_->ready)
        return;
    const auto previous = impl_->framing;
    std::array<int, 3> statuses;
    int index = 0;
    for (const auto &[name, member] : {std::pair{"video-zoom", &VideoFraming::zoom},
                                       std::pair{"video-pan-x", &VideoFraming::panX},
                                       std::pair{"video-pan-y", &VideoFraming::panY}}) {
        const auto [id, status] = impl_->submitCommand({"set", name, "0"});
        statuses[index++] = status;
        if (id)
            impl_->framing.*member = 0;
    }
    // Submit/cache the entire reset before signals can reenter with new intent.
    const QPointer<MpvController> guard(this);
    if (impl_->framing != previous)
        emit videoFramingChanged();
    for (const auto status : statuses) {
        if (!guard)
            return;
        guard->impl_->report(status);
    }
}
