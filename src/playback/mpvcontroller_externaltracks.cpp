#include "mpvcontroller_p.h"

#include <QDir>

void MpvController::addAudio(const QString &path) {
    impl_->addExternalTrack(MediaTrack::Type::Audio, path);
}

void MpvController::addSubtitle(const QString &path) {
    impl_->addExternalTrack(MediaTrack::Type::Subtitle, path);
}

void MpvController::Impl::addExternalTrack(MediaTrack::Type type, const QString &path) {
    if (!state.active || pendingPlayback || path.isEmpty())
        return;
    const auto generation = openGeneration;
    const auto identity = restoreIdentity + 1;
    cancelTrackRestore();
    if (generation != openGeneration || identity != restoreIdentity || !state.active ||
        pendingPlayback)
        return;
    if (type == MediaTrack::Type::Audio)
        emit owner->audioLoadRequested();
    else
        emit owner->subtitleLoadRequested();
    if (generation != openGeneration || identity != restoreIdentity || !state.active ||
        pendingPlayback)
        return;
    command({type == MediaTrack::Type::Audio ? "audio-add" : "sub-add",
             QFileInfo(path).absoluteFilePath().toUtf8(), "select"});
}

void MpvController::Impl::queryRestoredTracks() {
    if (restoreGeneration != openGeneration || !mediaReady || pendingPlayback)
        return;
    restoreQuery = ++request;
    const int status =
        mpv_get_property_async(client.get(), restoreQuery, "track-list", MPV_FORMAT_NODE);
    if (status < 0) {
        restoreQuery = 0;
        cancelTrackRestore();
    }
    report(status);
}

void MpvController::Impl::restoreExternalTracks() {
    if (tracksToRestore.isEmpty())
        return;
    restoreGeneration = openGeneration;
    ++restoreIdentity;
    externalPathsToRestore.clear();
    QSet<QString> paths;
    // Either add command imports all streams from a container. Load each file once,
    // then restore each kind's selected stream by its ordinal within that file.
    for (auto &track : tracksToRestore) {
        if (!track.external || track.externalPath.isEmpty())
            continue;
        const QFileInfo file(track.externalPath);
        const auto canonical = file.canonicalFilePath();
        track.externalPath =
            canonical.isEmpty() ? QDir::cleanPath(file.absoluteFilePath()) : canonical;
        if (!paths.contains(track.externalPath)) {
            paths.insert(track.externalPath);
            externalPathsToRestore.append({track.type, track.externalPath});
        }
    }
    restoreNextExternalTrack();
}

void MpvController::Impl::restoreNextExternalTrack() {
    if (restoreGeneration != openGeneration || !mediaReady || !state.active || pendingPlayback) {
        cancelTrackRestore();
        return;
    }
    // Both add commands spawn backend workers. The last query must follow their replies.
    const auto identity = restoreIdentity;
    while (!externalPathsToRestore.isEmpty()) {
        const auto [type, path] = externalPathsToRestore.takeFirst();
        const auto [id, status] = submitCommand(
            {type == MediaTrack::Type::Audio ? "audio-add" : "sub-add", path.toUtf8(), "auto"});
        restoreCommand = id;
        report(status);
        if (identity != restoreIdentity || restoreGeneration != openGeneration || id)
            return;
    }
    queryRestoredTracks();
}

void MpvController::Impl::restoreSelections(const QList<MediaTrack> &current) {
    if (restoreGeneration != openGeneration || !mediaReady || pendingPlayback) {
        cancelTrackRestore();
        return;
    }
    const auto identity = restoreIdentity;
    for (const auto type : {MediaTrack::Type::Audio, MediaTrack::Type::Subtitle}) {
        const auto id = trackIdForRestore(tracksToRestore, current, type);
        command({"set", type == MediaTrack::Type::Audio ? "aid" : "sid",
                 id ? QByteArray::number(id) : QByteArray("no")});
        if (identity != restoreIdentity || restoreGeneration != openGeneration)
            return;
    }
    applyingRestoredSelections = true;
    updateTracks(current);
}
