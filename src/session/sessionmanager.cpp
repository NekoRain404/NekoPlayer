#include "sessionmanager.h"
#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"

#include <QFileInfo>
#include <utility>

SessionManager::SessionManager(MpvController &player, PlaylistController &playlist,
                               QString filePath, QObject *parent)
    : QObject(parent), player_(player), playlist_(playlist), store_(std::move(filePath)) {
    // Persist user intent immediately; initial asynchronous mpv observations can be stale.
    connect(&player_, &MpvController::volumeRequested, this,
            [this](double volume) { audio_.volume = volume; });
    connect(&player_, &MpvController::muteRequested, this,
            [this](bool muted) { audio_.muted = muted; });
}

SessionLoadResult SessionManager::restore() {
    auto loaded = store_.load();
    loadError_ = loaded.error;
    if (!loadError_.isEmpty())
        return loaded;
    audio_ = loaded.state.audio;
    mediaDirectory_ = loaded.state.window.mediaDirectory;
    playlist_.restoreFiles(loaded.state.paths, loaded.state.currentIndex);
    player_.setVolume(audio_.volume);
    player_.setMuted(audio_.muted);
    player_.setHardwareDecodingEnabled(loaded.state.hardwareDecodingEnabled);
    return loaded;
}

QString SessionManager::save(WindowPreferences window, const QStringList &pendingPaths) {
    if (!loadError_.isEmpty())
        return loadError_;
    window.mediaDirectory = mediaDirectory_;
    return store_.save({audio_, std::move(window), playlist_.model()->paths() + pendingPaths,
                        playlist_.model()->currentIndex(), player_.hardwareDecodingEnabled()});
}
QString SessionManager::mediaDirectory() const { return mediaDirectory_; }
void SessionManager::rememberMedia(const QString &path) {
    if (!path.isEmpty())
        mediaDirectory_ = QFileInfo(path).absolutePath();
}
