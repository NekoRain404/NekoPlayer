#include "playback/mpv/mpvcontroller_p.h"

#include <algorithm>

qint64 MpvController::secondarySubtitleTrack() const { return impl_->secondarySid; }

void MpvController::selectSecondarySubtitleTrack(qint64 id) { impl_->selectSecondaryTrack(id); }

void MpvController::Impl::selectSecondaryTrack(qint64 id) {
    if (!state.active || pendingPlayback)
        return;
    const auto selected =
        std::find_if(tracks.cbegin(), tracks.cend(), [id](const MediaTrack &track) {
            return track.type == MediaTrack::Type::Subtitle && track.id == id;
        });
    if (id != 0 && selected == tracks.cend()) {
        error = MpvController::tr("所选轨道不存在");
        emit owner->errorOccurred(error);
        return;
    }
    if (secondarySid == id)
        return;
    // mpv accepts "no" for the numeric track properties; selecting the same
    // track as the primary subtitle is allowed and stacks both renderings.
    command({"set", "secondary-sid", id == 0 ? QByteArray("no") : QByteArray::number(id)});
    secondarySid = id;
    secondarySidToRestore = 0;
    emit owner->secondarySubtitleChanged();
}
