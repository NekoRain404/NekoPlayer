#include "sessionmanager.h"
#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"

#include <QDateTime>
#include <QFileInfo>
#include <algorithm>
#include <cmath>
#include <utility>

SessionManager::SessionManager(MpvController &player, PlaylistController &playlist,
                               QString filePath, QObject *parent)
    : QObject(parent), player_(player), playlist_(playlist), store_(filePath),
      historyStore_(filePath + QStringLiteral(".history.ini")) {
    // Persist user intent immediately; initial asynchronous mpv observations can be stale.
    connect(&player_, &MpvController::volumeRequested, this,
            [this](double volume) { audio_.volume = volume; });
    connect(&player_, &MpvController::muteRequested, this,
            [this](bool muted) { audio_.muted = muted; });
    connect(&playlist_, &PlaylistController::playbackPositionSnapshot, this,
            &SessionManager::rememberPlaybackPosition);
    connect(&player_, &MpvController::stopRequested, this, [this] {
        stopRequested_ = true;
        const auto path = PlaybackHistoryStore::normalizedPath(playlist_.model()->currentPath());
        if (!path.isEmpty())
            resumePositions_.remove(path);
    });
    connect(&player_, &MpvController::fileOpenRequested, this, [this] { stopRequested_ = false; });
    connect(&playlist_, &PlaylistController::mediaPlaybackFinished, this,
            [this](const QString &path) {
                const auto normalized = PlaybackHistoryStore::normalizedPath(path);
                if (normalized.isEmpty())
                    return;
                historyEntries_.remove(normalized);
                resumePositions_.remove(normalized);
                savePlaybackHistory();
            });
    playlist_.setResumePositionResolver([this](const QString &path) {
        return resumePositions_.value(PlaybackHistoryStore::normalizedPath(path), 0.0);
    });
}

SessionLoadResult SessionManager::restore() {
    auto loaded = store_.load();
    loadError_ = loaded.error;
    if (!loadError_.isEmpty())
        return loaded;
    audio_ = loaded.state.audio;
    const auto history = historyStore_.load();
    historyError_ = history.error;
    for (const auto &entry : history.entries) {
        const auto normalized = PlaybackHistoryStore::normalizedPath(entry.path);
        historyEntries_.insert(normalized, entry);
        resumePositions_.insert(normalized, entry.position);
    }
    mediaDirectory_ = loaded.state.window.mediaDirectory;
    playlist_.restoreFiles(loaded.state.paths, loaded.state.currentIndex,
                           loaded.state.resumePosition);
    player_.setVolume(audio_.volume);
    player_.setMuted(audio_.muted);
    player_.setHardwareDecodingEnabled(loaded.state.hardwareDecodingEnabled);
    player_.setSubtitlePreferences(loaded.state.subtitles);
    return loaded;
}

QString SessionManager::save(WindowPreferences window, const QStringList &pendingPaths) {
    if (!loadError_.isEmpty())
        return loadError_;
    const auto &state = player_.state();
    if (!stopRequested_ && state.active && state.seekable &&
        state.path == playlist_.model()->currentPath()) {
        const auto position = player_.resumePosition();
        rememberPlaybackPosition(state.path, position, state.duration);
    }
    window.mediaDirectory = mediaDirectory_;
    const auto position = player_.state().path == playlist_.model()->currentPath()
                              ? player_.resumePosition()
                              : playlist_.pendingResumePosition();
    const auto sessionError =
        store_.save({audio_, std::move(window), playlist_.model()->paths() + pendingPaths,
                     playlist_.model()->currentIndex(), player_.hardwareDecodingEnabled(),
                     player_.subtitlePreferences(), position});
    const auto historyError = savePlaybackHistory();
    return !sessionError.isEmpty() ? sessionError : historyError;
}

void SessionManager::rememberPlaybackPosition(const QString &path, double position,
                                              double duration) {
    if (!historyError_.isEmpty())
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

QString SessionManager::mediaDirectory() const { return mediaDirectory_; }
void SessionManager::rememberMedia(const QString &path) {
    if (!path.isEmpty())
        mediaDirectory_ = QFileInfo(path).absolutePath();
}
