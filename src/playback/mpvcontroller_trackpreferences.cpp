#include "mpvcontroller_p.h"

#include <QFileInfo>

void MpvController::Impl::cancelPreferenceRestore() {
    if (!preferenceRestore)
        return;
    if (preferenceRestore->command)
        canceledRestoreRequests.insert(preferenceRestore->command);
    if (preferenceRestore->query)
        canceledRestoreRequests.insert(preferenceRestore->query);
    preferenceRestore.reset();
    emit owner->trackPreferenceRestoreFinished(true);
}

void MpvController::Impl::startPreferenceSubtitleLoad() {
    if (preferenceRestore && (!mediaReady || !state.active || pendingPlayback ||
                              preferenceRestore->generation != openGeneration)) {
        cancelPreferenceRestore();
        return;
    }
    while (preferenceRestore && preferenceRestore->generation == openGeneration &&
           !preferenceRestore->paths.isEmpty()) {
        const auto identity = preferenceRestore->identity;
        const auto path = preferenceRestore->paths.takeFirst();
        const auto [id, status] = submitCommand({"sub-add", path.toUtf8(), "auto"});
        preferenceRestore->command = id;
        report(status);
        if (!preferenceRestore || preferenceRestore->identity != identity || id)
            return;
    }
    queryPreferenceTracks();
}

void MpvController::Impl::queryPreferenceTracks() {
    if (!preferenceRestore || preferenceRestore->generation != openGeneration)
        return;
    preferenceRestore->query = ++request;
    const int status = mpv_get_property_async(client.get(), preferenceRestore->query, "track-list",
                                              MPV_FORMAT_NODE);
    if (status < 0) {
        cancelPreferenceRestore();
        report(status);
    }
}

void MpvController::Impl::startPreferenceSelection() {
    if (!preferenceRestore || preferenceRestore->generation != openGeneration)
        return;
    if (preferenceRestore->selections.isEmpty()) {
        queryPreferenceTracks();
        return;
    }
    const auto [type, id] = preferenceRestore->selections.takeFirst();
    const auto [commandId, status] =
        submitCommand({"set", type == MediaTrack::Type::Audio ? "aid" : "sid",
                       id == 0 ? QByteArray("no") : QByteArray::number(id)});
    preferenceRestore->command = commandId;
    if (!commandId)
        cancelPreferenceRestore();
    report(status);
}

bool MpvController::Impl::finishPreferenceCommand(const mpv_event &event) {
    if (!preferenceRestore || event.reply_userdata != preferenceRestore->command)
        return false;
    const auto identity = preferenceRestore->identity;
    preferenceRestore->command = 0;
    if (!mediaReady || !state.active || pendingPlayback ||
        preferenceRestore->generation != openGeneration ||
        (preferenceRestore->selecting && event.error < 0)) {
        cancelPreferenceRestore();
        report(event.error);
        return true;
    }
    // An unreadable subtitle must not prevent loading the rest or restoring audio.
    report(event.error);
    if (preferenceRestore && preferenceRestore->identity == identity) {
        if (preferenceRestore->selecting)
            startPreferenceSelection();
        else
            startPreferenceSubtitleLoad();
    }
    return true;
}

bool MpvController::Impl::finishPreferenceQuery(const mpv_event &event) {
    if (!preferenceRestore || event.reply_userdata != preferenceRestore->query)
        return false;
    const auto identity = preferenceRestore->identity;
    preferenceRestore->query = 0;
    const auto *property = static_cast<mpv_event_property *>(event.data);
    if (!mediaReady || !state.active || pendingPlayback ||
        preferenceRestore->generation != openGeneration || event.error < 0 || !property ||
        !property->data || property->format != MPV_FORMAT_NODE) {
        cancelPreferenceRestore();
        report(event.error);
        return true;
    }
    auto snapshot = parseMpvTracks(*static_cast<mpv_node *>(property->data));
    if (!preferenceRestore->selecting) {
        applyPreferenceTracks(snapshot);
    } else {
        // Query only after the set-command acknowledgments; publish an owned final snapshot.
        updateTracks(std::move(snapshot));
        if (preferenceRestore && preferenceRestore->identity == identity) {
            if (preferenceRestore->generation != openGeneration || !mediaReady || !state.active ||
                pendingPlayback) {
                cancelPreferenceRestore();
                return true;
            }
            preferenceRestore.reset();
            emit owner->trackPreferenceRestoreFinished(false);
        }
    }
    return true;
}

void MpvController::Impl::applyPreferenceTracks(const QList<MediaTrack> &snapshot) {
    if (!preferenceRestore || preferenceRestore->generation != openGeneration)
        return;
    const auto preference = preferenceRestore->preference;
    const auto select = [this](MediaTrack::Type type, qint64 id) {
        preferenceRestore->selections.append({type, id});
    };
    if (preference.audioTrackId) {
        const auto id = *preference.audioTrackId;
        const bool exists =
            id == 0 || std::any_of(snapshot.cbegin(), snapshot.cend(), [id](const auto &track) {
                return track.type == MediaTrack::Type::Audio && !track.external && track.id == id;
            });
        if (exists)
            select(MediaTrack::Type::Audio, id);
    }
    if (preference.subtitleTrackId) {
        const auto id = *preference.subtitleTrackId;
        const bool exists =
            id == 0 || std::any_of(snapshot.cbegin(), snapshot.cend(), [id](const auto &track) {
                return track.type == MediaTrack::Type::Subtitle && !track.external &&
                       track.id == id;
            });
        if (exists)
            select(MediaTrack::Type::Subtitle, id);
    } else if (!preference.selectedExternalSubtitlePath.isEmpty() &&
               preference.selectedExternalSubtitleStream >= 0) {
        int ordinal = 0;
        for (const auto &track : snapshot) {
            if (track.type != MediaTrack::Type::Subtitle || !track.external ||
                track.externalPath != preference.selectedExternalSubtitlePath)
                continue;
            if (ordinal++ == preference.selectedExternalSubtitleStream) {
                select(MediaTrack::Type::Subtitle, track.id);
                break;
            }
        }
    }
    preferenceRestore->selecting = true;
    startPreferenceSelection();
}

void MpvController::restoreTrackPreference(const TrackSelectionPreference &preference) {
    if (!state().active || !impl_->mediaReady) {
        emit trackPreferenceRestoreFinished(true);
        return;
    }
    const auto identity = ++impl_->request;
    impl_->cancelPreferenceRestore();
    if (impl_->request != identity || !state().active || !impl_->mediaReady)
        return;
    Impl::TrackPreferenceRestore restore;
    restore.preference = preference;
    restore.generation = impl_->openGeneration;
    restore.identity = identity;
    QSet<QString> uniquePaths;
    for (const auto &track : tracks()) {
        if (track.external && !track.externalPath.isEmpty())
            uniquePaths.insert(QFileInfo(track.externalPath).canonicalFilePath());
    }
    QStringList unavailablePaths;
    auto paths = preference.externalSubtitlePaths;
    if (!preference.selectedExternalSubtitlePath.isEmpty())
        paths.append(preference.selectedExternalSubtitlePath);
    for (const auto &path : paths) {
        const QFileInfo info(path);
        const auto canonical = info.canonicalFilePath();
        if (!info.exists() || !info.isFile() || canonical.isEmpty()) {
            unavailablePaths.append(path);
            continue;
        }
        if (uniquePaths.contains(canonical))
            continue;
        uniquePaths.insert(canonical);
        restore.paths.append(canonical);
    }
    impl_->preferenceRestore = std::move(restore);
    for (const auto &path : unavailablePaths) {
        emit errorOccurred(tr("外挂字幕文件不可用：%1").arg(path));
        if (!impl_->preferenceRestore || impl_->preferenceRestore->identity != identity)
            return;
    }
    impl_->startPreferenceSubtitleLoad();
}

bool MpvController::trackRestorationPending() const {
    return impl_->preferenceRestore.has_value() || !impl_->tracksToRestore.isEmpty();
}
