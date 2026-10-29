#include "playback/mpv/mpvcontroller_p.h"

void MpvController::Impl::cancelInformationQueries() {
    for (auto &property : informationQueries)
        property = 0;
}

void MpvController::Impl::scheduleInformationRefresh() {
    if (informationRefreshQueued || !mediaReady)
        return;
    informationRefreshQueued = true;
    // Drain coalesced property notifications before requesting the current
    // decoder parameters; FILE_LOADED can precede decoder configuration.
    QMetaObject::invokeMethod(
        owner,
        [this] {
            informationRefreshQueued = false;
            if (mediaReady && !shuttingDown)
                refreshMediaInformation();
        },
        Qt::QueuedConnection);
}

void MpvController::Impl::refreshMediaInformation() {
    // Observations may omit unchanged values across file loads. Refresh once,
    // asynchronously, after loading; live track changes still use observations.
    cancelInformationQueries();
    for (const auto &[property, name, format] : MpvPrivate::informationProperties) {
        const auto id = ++request;
        if (mpv_get_property_async(client.get(), id, name, format) >= 0)
            informationQueries.insert(id, property);
    }
}

bool MpvController::Impl::finishInformationQuery(const mpv_event &event) {
    const auto it = informationQueries.find(event.reply_userdata);
    if (it == informationQueries.end())
        return false;
    const auto property = it.value();
    informationQueries.erase(it);
    if (property) {
        auto value =
            event.data ? *static_cast<mpv_event_property *>(event.data) : mpv_event_property{};
        // Failed GET replies can retain the requested format and zeroed data.
        // Unlike observations, they must be normalized using the error code.
        if (event.error < 0) {
            value.format = MPV_FORMAT_NONE;
            value.data = nullptr;
        }
        auto informationEvent = event;
        informationEvent.reply_userdata = property;
        informationEvent.data = &value;
        updateMediaInformation(informationEvent);
    }
    // Unavailable metadata (e.g. audio parameters for video-only media) is normal.
    return true;
}

bool MpvController::Impl::updateMediaInformation(const mpv_event &event) {
    if (event.reply_userdata < FileFormat || event.reply_userdata > AudioSampleFormat)
        return false;
    if (!mediaReady)
        return true;
    const auto &property = *static_cast<mpv_event_property *>(event.data);
    const auto text = [&property] {
        return property.data && property.format == MPV_FORMAT_STRING
                   ? QString::fromUtf8(*static_cast<char **>(property.data))
                   : QString{};
    };
    const auto integer = [&property](qint64 unavailable = 0) -> qint64 {
        if (!property.data || property.format != MPV_FORMAT_INT64)
            return unavailable;
        return std::max<qint64>(*static_cast<int64_t *>(property.data), unavailable);
    };
    const auto previous = information;
    switch (event.reply_userdata) {
    case FileFormat:
        information.fileFormat = text();
        break;
    case FileSize:
        information.fileSize = integer(-1);
        break;
    case VideoCodec:
        information.videoCodec = text();
        break;
    case VideoWidth:
        information.videoWidth = integer();
        break;
    case VideoHeight:
        information.videoHeight = integer();
        break;
    case VideoPixelFormat:
        information.videoPixelFormat = text();
        break;
    case VideoPrimaries:
        information.videoPrimaries = text();
        break;
    case VideoTransfer:
        information.videoTransfer = text();
        break;
    case ContainerFrameRate: {
        const double fps = property.data && property.format == MPV_FORMAT_DOUBLE
                               ? *static_cast<double *>(property.data)
                               : 0;
        information.containerFrameRate = std::isfinite(fps) && fps > 0 ? fps : 0;
        break;
    }
    case AudioCodec:
        information.audioCodec = text();
        break;
    case AudioSampleRate:
        information.audioSampleRate = integer();
        break;
    case AudioChannels:
        information.audioChannels = text();
        break;
    case AudioSampleFormat:
        information.audioSampleFormat = text();
        break;
    default:
        return false;
    }
    if (previous != information)
        emit owner->mediaInformationChanged();
    return true;
}
