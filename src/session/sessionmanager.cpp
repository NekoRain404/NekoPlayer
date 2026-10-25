#include "sessionmanager.h"
#include "playback/mediasource.h"
#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"

#include <QDateTime>
#include <QFileInfo>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <utility>

SessionManager::SessionManager(MpvController &player, PlaylistController &playlist,
                               QString filePath, QObject *parent)
    : QObject(parent), player_(player), playlist_(playlist), store_(filePath),
      historyStore_(filePath + QStringLiteral(".history.ini")),
      trackPreferenceStore_(filePath + QStringLiteral(".tracks.ini")) {
    // Persist user intent immediately; initial asynchronous mpv observations can be stale.
    connect(&player_, &MpvController::volumeRequested, this,
            [this](double volume) { audio_.volume = volume; });
    connect(&player_, &MpvController::muteRequested, this,
            [this](bool muted) { audio_.muted = muted; });
    connect(&player_, &MpvController::audioDevicePreferenceChanged, this,
            [this](const QString &name) { audio_.outputDevice = name; });
    connect(&player_, &MpvController::audioDevicesChanged, this,
            [this] { player_.applyAudioDevicePreference(audio_.outputDevice); });
    connect(&player_, &MpvController::tuningRequested, this,
            [this](double speed, double audioDelay, double subtitleDelay) {
                tuning_ = {speed, audioDelay, subtitleDelay};
            });
    connect(&playlist_, &PlaylistController::playbackPositionSnapshot, this,
            &SessionManager::rememberPlaybackPosition);
    connect(&playlist_, &PlaylistController::playbackPositionSnapshot, this,
            [this] { rememberTrackPreference(); });
    connect(&player_, &MpvController::stopRequested, this, [this] {
        rememberTrackPreference();
        pendingMediaPreference_ = false;
        stopRequested_ = true;
        const auto path = PlaybackHistoryStore::normalizedPath(playlist_.model()->currentPath());
        if (!path.isEmpty())
            resumePositions_.remove(path);
    });
    connect(&player_, &MpvController::fileOpenRequested, this, [this] {
        stopRequested_ = false;
        pendingMediaPreference_ = true;
        loadedMediaFingerprint_.reset();
        requestedAudioTrackId_.reset();
        requestedSubtitleTrackId_.reset();
    });
    connect(&player_, &MpvController::trackSelectionRequested, this,
            [this](MediaTrack::Type type, qint64 id) {
                auto &target = type == MediaTrack::Type::Audio ? requestedAudioTrackId_
                                                               : requestedSubtitleTrackId_;
                target = id;
                rememberTrackPreference();
            });
    connect(&player_, &MpvController::subtitleLoadRequested, this,
            [this] { requestedSubtitleTrackId_.reset(); });
    connect(&player_, &MpvController::audioLoadRequested, this,
            [this] { requestedAudioTrackId_.reset(); });
    connect(&player_, &MpvController::fileLoaded, this, &SessionManager::restoreTrackPreference);
    connect(&player_, &MpvController::tracksChanged, this, &SessionManager::observeTrackPreference);
    connect(&player_, &MpvController::trackPreferenceRestoreFinished, this, [this](bool canceled) {
        restoringTrackPreference_ = false;
        if (!canceled)
            rememberTrackPreference();
    });
    connect(&playlist_, &PlaylistController::mediaPlaybackFinished, this,
            [this](const QString &path) {
                const auto normalized = PlaybackHistoryStore::normalizedPath(path);
                if (normalized.isEmpty())
                    return;
                historyEntries_.remove(normalized);
                resumePositions_.remove(normalized);
                savePlaybackHistory();
                emit playbackHistoryChanged();
            });
    playlist_.setResumePositionResolver([this](const QString &path) {
        return resumePlaybackPositionEnabled_
                   ? resumePositions_.value(PlaybackHistoryStore::normalizedPath(path), 0.0)
                   : 0.0;
    });
}

SessionLoadResult SessionManager::restore() {
    auto loaded = store_.load();
    loadError_ = loaded.error;
    if (!loadError_.isEmpty())
        return loaded;
    audio_ = loaded.state.audio;
    const auto trackPreferences = trackPreferenceStore_.load();
    trackPreferences_ = trackPreferences.entries;
    trackPreferenceError_ = trackPreferences.error;
    if (!trackPreferenceError_.isEmpty())
        emit configurationErrorOccurred(trackPreferenceError_);
    player_.applyAudioDevicePreference(audio_.outputDevice);
    tuning_ = loaded.state.tuning;
    const auto restoredTuning = tuning_;
    const auto history = historyStore_.load();
    historyError_ = history.error;
    for (const auto &entry : history.entries) {
        const auto normalized = PlaybackHistoryStore::normalizedPath(entry.path);
        historyEntries_.insert(normalized, entry);
        resumePositions_.insert(normalized, entry.position);
    }
    emit playbackHistoryChanged();
    mediaDirectory_ = loaded.state.window.mediaDirectory;
    mediaLibraryPreferences_ = loaded.state.mediaLibrary;
    resumePlaybackPositionEnabled_ = loaded.state.resumePlaybackPosition;
    seekStepSeconds_ = loaded.state.seekStepSeconds;
    interaction_ = loaded.state.interaction.normalized();
    playlist_.restoreFiles(loaded.state.paths, loaded.state.currentIndex,
                           resumePlaybackPositionEnabled_ ? loaded.state.resumePosition : 0);
    player_.setVolume(audio_.volume);
    player_.setMuted(audio_.muted);
    player_.setHardwareDecodingEnabled(loaded.state.hardwareDecodingEnabled);
    player_.setSubtitlePreferences(loaded.state.subtitles);
    player_.setPlaybackSpeed(restoredTuning.playbackSpeed);
    player_.setAudioDelay(restoredTuning.audioDelay);
    player_.setSubtitleDelay(restoredTuning.subtitleDelay);
    player_.setVideoColorAdjustment(loaded.state.videoColor);
    return loaded;
}

QString SessionManager::save(WindowPreferences window, const QStringList &pendingPaths) {
    if (!loadError_.isEmpty())
        return loadError_;
    rememberTrackPreference();
    const auto &state = player_.state();
    if (!stopRequested_ && state.active && state.seekable &&
        state.path == playlist_.model()->currentPath()) {
        const auto position = player_.resumePosition();
        rememberPlaybackPosition(state.path, position, state.duration);
    }
    window.mediaDirectory = mediaDirectory_;
    const auto position = !resumePlaybackPositionEnabled_ ? 0.0
                          : player_.state().path == playlist_.model()->currentPath()
                              ? player_.resumePosition()
                              : playlist_.pendingResumePosition();
    SessionState saved;
    saved.audio = audio_;
    saved.window = std::move(window);
    saved.paths = playlist_.model()->paths() + pendingPaths;
    saved.currentIndex = playlist_.model()->currentIndex();
    saved.hardwareDecodingEnabled = player_.hardwareDecodingEnabled();
    saved.subtitles = player_.subtitlePreferences();
    saved.resumePosition = position;
    saved.tuning = tuning_;
    saved.videoColor = player_.videoColorAdjustment();
    saved.mediaLibrary = mediaLibraryPreferences_;
    saved.resumePlaybackPosition = resumePlaybackPositionEnabled_;
    saved.seekStepSeconds = seekStepSeconds_;
    saved.interaction = interaction_;
    const auto sessionError = store_.save(saved);
    const auto historyError = savePlaybackHistory();
    if (!sessionError.isEmpty())
        return sessionError;
    return !historyError.isEmpty() ? historyError : trackPreferenceError_;
}

void SessionManager::rememberPlaybackPosition(const QString &path, double position,
                                              double duration) {
    if (MediaSource::isNetwork(path) || !historyError_.isEmpty())
        return;
    const auto normalized = PlaybackHistoryStore::normalizedPath(path);
    if (normalized.isEmpty() || !std::isfinite(duration) || duration <= 0)
        return;
    if (!std::isfinite(position) || position <= 0) {
        historyEntries_.remove(normalized);
        resumePositions_.remove(normalized);
    } else {
        const PlaybackHistoryEntry entry{normalized, position, duration,
                                         QDateTime::currentMSecsSinceEpoch()};
        historyEntries_.insert(normalized, entry);
        resumePositions_.insert(normalized, position);
        while (historyEntries_.size() > PlaybackHistoryStore::MaximumEntries) {
            const auto oldest =
                std::min_element(historyEntries_.cbegin(), historyEntries_.cend(),
                                 [](const auto &left, const auto &right) {
                                     return left.lastPlayedUtcMs < right.lastPlayedUtcMs;
                                 });
            const auto oldestPath = oldest.key();
            historyEntries_.remove(oldestPath);
            resumePositions_.remove(oldestPath);
        }
    }
    savePlaybackHistory();
    emit playbackHistoryChanged();
}

QString SessionManager::savePlaybackHistory() const {
    if (!historyError_.isEmpty())
        return historyError_;
    QList<PlaybackHistoryEntry> entries;
    entries.reserve(historyEntries_.size());
    for (const auto &entry : historyEntries_)
        entries.append(entry);
    return historyStore_.save(entries);
}

void SessionManager::restoreTrackPreference() {
    // Renderer recovery reloads the same media without a new open request.
    if (!std::exchange(pendingMediaPreference_, false))
        return;
    if (!player_.state().active || MediaSource::isNetwork(player_.state().path) ||
        player_.state().path != playlist_.model()->currentPath())
        return;
    const auto path = TrackPreferenceStore::normalizedPath(player_.state().path);
    if (path.isEmpty())
        return;
    TrackPreferenceEntry fingerprint;
    fingerprint.mediaPath = path;
    const QFileInfo info(path);
    fingerprint.fileSize = info.size();
    fingerprint.modifiedUtcMs = info.lastModified().toUTC().toMSecsSinceEpoch();
    loadedMediaFingerprint_ = fingerprint;
    if (!trackPreferenceError_.isEmpty())
        return;
    const auto found = std::find_if(
        trackPreferences_.cbegin(), trackPreferences_.cend(), [&path](const auto &entry) {
            return entry.mediaPath == path && TrackPreferenceStore::matchesFingerprint(entry);
        });
    if (found == trackPreferences_.cend()) {
        restoringTrackPreference_ = false;
        rememberTrackPreference();
        return;
    }
    restoringTrackPreference_ = true;
    player_.restoreTrackPreference(found->selection);
}

void SessionManager::rememberTrackPreference() {
    if (MediaSource::isNetwork(player_.state().path) || restoringTrackPreference_ ||
        player_.trackRestorationPending() || pendingMediaPreference_ || !loadedMediaFingerprint_ ||
        !TrackPreferenceStore::matchesFingerprint(*loadedMediaFingerprint_) ||
        !trackPreferenceError_.isEmpty() || !player_.state().active || player_.tracks().isEmpty() ||
        player_.state().path != playlist_.model()->currentPath())
        return;
    const auto path = TrackPreferenceStore::normalizedPath(player_.state().path);
    if (path.isEmpty() || loadedMediaFingerprint_->mediaPath != path)
        return;
    auto entry = *loadedMediaFingerprint_;
    entry.lastUsedUtcMs = QDateTime::currentMSecsSinceEpoch();
    const auto &tracks = player_.tracks();
    const auto requestedTrack = [&tracks](MediaTrack::Type type, const std::optional<qint64> &id) {
        const auto selected =
            std::find_if(tracks.cbegin(), tracks.cend(), [type](const auto &track) {
                return track.type == type && track.selected;
            });
        const auto selectedId = selected == tracks.cend() ? 0 : selected->id;
        return id.value_or(selectedId);
    };
    const auto audioId = requestedTrack(MediaTrack::Type::Audio, requestedAudioTrackId_);
    const auto subtitleId = requestedTrack(MediaTrack::Type::Subtitle, requestedSubtitleTrackId_);
    const auto externalTrackPath = [](const MediaTrack &track) {
        const QFileInfo info(track.externalPath);
        const auto canonical = info.canonicalFilePath();
        return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
    };
    // A populated track snapshot with no selected track represents explicit off.
    entry.selection.audioTrackId = audioId;
    entry.selection.subtitleTrackId = subtitleId;
    const auto rememberExternalTracks = [&](MediaTrack::Type type, qint64 selectedId,
                                            std::optional<qint64> &internalId, QStringList &paths,
                                            QString &selectedPath, int &selectedOrdinal) {
        QSet<QString> externalPaths;
        for (const auto &track : tracks) {
            if (track.type != type || !track.external || track.externalPath.isEmpty())
                continue;
            const auto externalPath = externalTrackPath(track);
            if (externalPath.isEmpty())
                continue;
            if (!externalPaths.contains(externalPath)) {
                externalPaths.insert(externalPath);
                paths.append(externalPath);
            }
            if (track.id == selectedId) {
                internalId.reset();
                selectedPath = externalPath;
                selectedOrdinal = 0;
                for (const auto &prior : tracks) {
                    if (&prior == &track)
                        break;
                    if (prior.type == type && prior.external &&
                        externalTrackPath(prior) == externalPath)
                        ++selectedOrdinal;
                }
            }
        }
    };
    rememberExternalTracks(MediaTrack::Type::Audio, audioId, entry.selection.audioTrackId,
                           entry.selection.externalAudioPaths,
                           entry.selection.selectedExternalAudioPath,
                           entry.selection.selectedExternalAudioStream);
    rememberExternalTracks(MediaTrack::Type::Subtitle, subtitleId, entry.selection.subtitleTrackId,
                           entry.selection.externalSubtitlePaths,
                           entry.selection.selectedExternalSubtitlePath,
                           entry.selection.selectedExternalSubtitleStream);
    const auto old = std::find_if(trackPreferences_.begin(), trackPreferences_.end(),
                                  [&path](const auto &item) { return item.mediaPath == path; });
    if (old == trackPreferences_.end())
        trackPreferences_.append(entry);
    else
        *old = entry;
    const auto error = saveTrackPreferences();
    if (!error.isEmpty()) {
        trackPreferenceError_ = error;
        emit configurationErrorOccurred(error);
    }
}

void SessionManager::observeTrackPreference() {
    // A request can match an old snapshot before the queued command runs.
    // Clear pending intent only when a new observation confirms it.
    for (const auto type : {MediaTrack::Type::Audio, MediaTrack::Type::Subtitle}) {
        auto &requested =
            type == MediaTrack::Type::Audio ? requestedAudioTrackId_ : requestedSubtitleTrackId_;
        const auto selected = std::find_if(
            player_.tracks().cbegin(), player_.tracks().cend(),
            [type](const auto &track) { return track.type == type && track.selected; });
        const auto selectedId = selected == player_.tracks().cend() ? 0 : selected->id;
        if (requested == selectedId)
            requested.reset();
    }
    rememberTrackPreference();
}

QString SessionManager::saveTrackPreferences() {
    trackPreferences_.removeIf(
        [](const auto &entry) { return !TrackPreferenceStore::matchesFingerprint(entry); });
    std::stable_sort(trackPreferences_.begin(), trackPreferences_.end(),
                     [](const auto &left, const auto &right) {
                         return left.lastUsedUtcMs > right.lastUsedUtcMs;
                     });
    if (trackPreferences_.size() > TrackPreferenceStore::MaximumEntries)
        trackPreferences_.resize(TrackPreferenceStore::MaximumEntries);
    return trackPreferenceStore_.save(trackPreferences_);
}

QString SessionManager::mediaDirectory() const { return mediaDirectory_; }

bool SessionManager::resumePlaybackPositionEnabled() const {
    return resumePlaybackPositionEnabled_;
}

int SessionManager::seekStepSeconds() const { return seekStepSeconds_; }

void SessionManager::setSeekStepSeconds(int seconds) {
    if (seconds >= seekStepSecondsMinimum && seconds <= seekStepSecondsMaximum)
        seekStepSeconds_ = seconds;
}

void SessionManager::setResumePlaybackPositionEnabled(bool enabled) {
    resumePlaybackPositionEnabled_ = enabled;
}

MediaLibraryPreferences SessionManager::mediaLibraryPreferences() const {
    return mediaLibraryPreferences_;
}

InteractionPreferences SessionManager::interactionPreferences() const { return interaction_; }

void SessionManager::setInteractionPreferences(InteractionPreferences preferences) {
    interaction_ = preferences.normalized();
}

void SessionManager::setMediaLibraryPreferences(MediaLibraryPreferences preferences) {
    mediaLibraryPreferences_ = preferences;
}

QList<PlaybackHistoryEntry> SessionManager::playbackHistory() const {
    QList<PlaybackHistoryEntry> entries;
    entries.reserve(historyEntries_.size());
    for (const auto &entry : historyEntries_)
        entries.append(entry);
    std::sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
        return left.lastPlayedUtcMs > right.lastPlayedUtcMs;
    });
    return entries;
}
void SessionManager::rememberMedia(const QString &path) {
    const auto source = MediaSource::normalize(path);
    if (!source.isEmpty() && !MediaSource::isNetwork(source))
        mediaDirectory_ = QFileInfo(source).absolutePath();
}
