#include "mpvcontroller_p.h"

void MpvController::Impl::updateProperty(const mpv_event &event) {
    if (updateMediaInformation(event))
        return;
    const auto &value = *static_cast<mpv_event_property *>(event.data);
    if (event.reply_userdata == VideoImage) {
        hasMotionVideo =
            value.data && (state.active || loading) && !*static_cast<int *>(value.data);
        emit owner->stateChanged();
        return;
    }
    if (event.reply_userdata == Chapters) {
        updateChapters(value.data && (state.active || loading)
                           ? parseMpvChapters(*static_cast<mpv_node *>(value.data))
                           : QList<MediaChapter>{});
        return;
    }
    if (event.reply_userdata == Tracks) {
        updateTracks(value.data && (state.active || loading)
                         ? parseMpvTracks(*static_cast<mpv_node *>(value.data))
                         : QList<MediaTrack>{});
        return;
    }
    if (!value.data) {
        switch (event.reply_userdata) {
        case Position:
            state.position = 0;
            break;
        case Duration:
            state.duration = 0;
            break;
        case Seekable:
            state.seekable = false;
            break;
        case HardwareDecoder:
            state.hardwareDecoder.clear();
            break;
        case Chapter:
            state.currentChapter = -1;
            break;
        default:
            return;
        }
        emit owner->stateChanged();
        return;
    }
    switch (event.reply_userdata) {
    case Chapter:
        observeChapter(*static_cast<int64_t *>(value.data));
        break;
    case Speed:
        observeTuning(speed, *static_cast<double *>(value.data));
        return;
    case AudioDelay:
        observeTuning(audioDelay, *static_cast<double *>(value.data));
        return;
    case SubtitleDelay:
        observeTuning(subtitleDelay, *static_cast<double *>(value.data));
        return;
    case Position:
        if (state.active) {
            state.position = *static_cast<double *>(value.data);
            if (mediaReady && !openingCommand && playingEntry == openingEntry)
                openingPosition.reset();
        }
        break;
    case Duration:
        if (state.active)
            state.duration = *static_cast<double *>(value.data);
        break;
    case Paused:
        state.paused = *static_cast<int *>(value.data) != 0;
        break;
    case Volume:
        state.volume = *static_cast<double *>(value.data);
        break;
    case Muted:
        state.muted = *static_cast<int *>(value.data) != 0;
        break;
    case Seekable:
        state.seekable = state.active && *static_cast<int *>(value.data) != 0;
        break;
    case HardwareDecoder:
        state.hardwareDecoder = QString::fromUtf8(*static_cast<char **>(value.data));
        break;
    default:
        return;
    }
    emit owner->stateChanged();
}

void MpvController::Impl::drainEvents() {
    if (shuttingDown)
        return;
    while (const auto *event = mpv_wait_event(client.get(), 0)) {
        switch (event->event_id) {
        case MPV_EVENT_NONE:
            return;
        case MPV_EVENT_START_FILE:
            playingEntry = static_cast<mpv_event_start_file *>(event->data)->playlist_entry_id;
            resetPlayback();
            loading = true;
            emit owner->stateChanged();
            break;
        case MPV_EVENT_FILE_LOADED:
            loading = false;
            recoveringRenderer = false;
            state.active = true;
            mediaReady = true;
            error.clear();
            scheduleInformationRefresh();
            restoreExternalTracks();
            emit owner->stateChanged();
            emit owner->fileLoaded();
            break;
        case MPV_EVENT_END_FILE: {
            const auto &end = *static_cast<mpv_event_end_file *>(event->data);
            if (end.playlist_entry_id == openingEntry)
                openingPosition.reset();
            const bool expectedReset = recoveringRenderer && end.error == MPV_ERROR_VO_INIT_FAILED;
            if (end.reason == MPV_END_FILE_REASON_ERROR && !expectedReset) {
                recoveringRenderer = false;
                recovery.reset();
            }
            resetPlayback();
            emit owner->stateChanged();
            if (end.reason == MPV_END_FILE_REASON_ERROR && !expectedReset) {
                emit owner->mediaLoadFailed(QString::fromUtf8(mpv_error_string(end.error)));
                report(end.error);
            }
            if (end.reason == MPV_END_FILE_REASON_EOF)
                emit owner->playbackFinished();
            break;
        }
        case MPV_EVENT_PROPERTY_CHANGE:
            updateProperty(*event);
            break;
        case MPV_EVENT_AUDIO_RECONFIG:
        case MPV_EVENT_VIDEO_RECONFIG:
            scheduleInformationRefresh();
            break;
        case MPV_EVENT_SEEK:
            chapterSeekStarted = requestedChapter.has_value() && !chapterCommand;
            break;
        case MPV_EVENT_PLAYBACK_RESTART:
            queryChapterRequest(chapterSeekStarted);
            break;
        case MPV_EVENT_COMMAND_REPLY:
            if (event->reply_userdata == openingCommand && openingCommand) {
                openingCommand = 0;
                const auto &result = static_cast<mpv_event_command *>(event->data)->result;
                if (event->error < 0) {
                    openingPosition.reset();
                } else if (result.format == MPV_FORMAT_NODE_MAP) {
                    const auto &map = *result.u.list;
                    for (int index = 0; index < map.num; ++index) {
                        if (QByteArrayView(map.keys[index]) == "playlist_entry_id" &&
                            map.values[index].format == MPV_FORMAT_INT64)
                            openingEntry = map.values[index].u.int64;
                    }
                }
            }
            confirmChapterRequest(*event);
            if (canceledRestoreRequests.remove(event->reply_userdata))
                break;
            if (event->reply_userdata == restoreCommand && restoreCommand) {
                restoreCommand = 0;
                restoreNextSubtitle();
            }
            report(event->error);
            break;
        case MPV_EVENT_GET_PROPERTY_REPLY:
            if (finishInformationQuery(*event))
                break;
            if (finishChapterQuery(*event))
                break;
            if (finishTuningQuery(*event))
                break;
            if (canceledRestoreRequests.remove(event->reply_userdata))
                break;
            if (event->reply_userdata == restoreQuery && restoreQuery) {
                restoreQuery = 0;
                const auto &property = *static_cast<mpv_event_property *>(event->data);
                if (event->error >= 0 && property.data)
                    restoreSelections(parseMpvTracks(*static_cast<mpv_node *>(property.data)));
                else
                    cancelTrackRestore();
            }
            report(event->error);
            break;
        case MPV_EVENT_SET_PROPERTY_REPLY:
            confirmTuning(*event);
            report(event->error);
            break;
        case MPV_EVENT_SHUTDOWN:
            ready = false;
            openingPosition.reset();
            openingCommand = 0;
            recoveringRenderer = false;
            resetPlayback();
            emit owner->stateChanged();
            return;
        default:
            break;
        }
    }
}
