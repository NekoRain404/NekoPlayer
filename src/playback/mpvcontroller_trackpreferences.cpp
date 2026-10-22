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

void MpvController::Impl::startPreferenceExternalLoad() {
    if (preferenceRestore && (!mediaReady || !state.active || pendingPlayback ||
                              preferenceRestore->generation != openGeneration)) {
        cancelPreferenceRestore();
        return;
    }
    while (preferenceRestore && preferenceRestore->generation == openGeneration &&
           !preferenceRestore->paths.isEmpty()) {
        const auto identity = preferenceRestore->identity;
        const auto [type, path] = preferenceRestore->paths.takeFirst();
        const auto [id, status] = submitCommand(
            {type == MediaTrack::Type::Audio ? "audio-add" : "sub-add", path.toUtf8(), "auto"});
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
    // An unreadable external file must not prevent loading the rest or restoring selections.
    report(event.error);
    if (preferenceRestore && preferenceRestore->identity == identity) {
        if (preferenceRestore->selecting)
            startPreferenceSelection();
        else
            startPreferenceExternalLoad();
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
    const auto selectPreference = [&](MediaTrack::Type type, std::optional<qint64> internalId,
                                      const QString &path, int stream) {
        if (internalId) {
            const auto id = *internalId;
            const bool exists =
                id == 0 ||
                std::any_of(snapshot.cbegin(), snapshot.cend(), [type, id](const auto &track) {
                    return track.type == type && !track.external && track.id == id;
                });
            if (exists)
                select(type, id);
            return;
        }
        if (path.isEmpty() || stream < 0)
            return;
        const auto canonical = QFileInfo(path).canonicalFilePath();
        if (canonical.isEmpty())
            return;
        int ordinal = 0;
        for (const auto &track : snapshot) {
            if (track.type != type || !track.external ||
                QFileInfo(track.externalPath).canonicalFilePath() != canonical)
                continue;
            if (ordinal++ == stream) {
                select(type, track.id);
                break;
            }
        }
    };
    selectPreference(MediaTrack::Type::Audio, preference.audioTrackId,
                     preference.selectedExternalAudioPath, preference.selectedExternalAudioStream);
    selectPreference(MediaTrack::Type::Subtitle, preference.subtitleTrackId,
                     preference.selectedExternalSubtitlePath,
                     preference.selectedExternalSubtitleStream);
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
    QList<std::pair<MediaTrack::Type, QString>> unavailablePaths;
    const auto appendPaths = [&](MediaTrack::Type type, QStringList paths,
                                 const QString &selected) {
        if (!selected.isEmpty())
            paths.append(selected);
        for (const auto &path : paths) {
            const QFileInfo info(path);
            const auto canonical = info.canonicalFilePath();
            if (!info.exists() || !info.isFile() || canonical.isEmpty()) {
                if (!unavailablePaths.contains({type, path}))
                    unavailablePaths.append({type, path});
                continue;
            }
            if (uniquePaths.contains(canonical))
                continue;
            uniquePaths.insert(canonical);
            restore.paths.append({type, canonical});
        }
    };
    appendPaths(MediaTrack::Type::Audio, preference.externalAudioPaths,
                preference.selectedExternalAudioPath);
    appendPaths(MediaTrack::Type::Subtitle, preference.externalSubtitlePaths,
                preference.selectedExternalSubtitlePath);
    impl_->preferenceRestore = std::move(restore);
    for (const auto &[type, path] : unavailablePaths) {
        emit errorOccurred((type == MediaTrack::Type::Audio ? tr("外挂音轨文件不可用：%1")
                                                            : tr("外挂字幕文件不可用：%1"))
                               .arg(path));
        if (!impl_->preferenceRestore || impl_->preferenceRestore->identity != identity)
            return;
    }
    impl_->startPreferenceExternalLoad();
}

bool MpvController::trackRestorationPending() const {
    return impl_->preferenceRestore.has_value() || !impl_->tracksToRestore.isEmpty();
}
