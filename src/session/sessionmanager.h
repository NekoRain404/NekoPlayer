#pragma once

#include "playbackhistorystore.h"
#include "sessionstore.h"
#include "trackpreferencestore.h"
#include <QHash>
#include <QObject>

class MpvController;
class PlaylistController;

class SessionManager final : public QObject {
    Q_OBJECT
  public:
    SessionManager(MpvController &player, PlaylistController &playlist, QString filePath,
                   QObject *parent = nullptr);
    [[nodiscard]] SessionLoadResult restore();
    [[nodiscard]] QString save(WindowPreferences window, const QStringList &pendingPaths = {});
    [[nodiscard]] QString mediaDirectory() const;
    [[nodiscard]] MediaLibraryPreferences mediaLibraryPreferences() const;
    void setMediaLibraryPreferences(MediaLibraryPreferences preferences);
    [[nodiscard]] bool resumePlaybackPositionEnabled() const;
    void setResumePlaybackPositionEnabled(bool enabled);
    [[nodiscard]] QList<PlaybackHistoryEntry> playbackHistory() const;
    void rememberMedia(const QString &path);

  signals:
    void configurationErrorOccurred(const QString &message);
    void playbackHistoryChanged();

  private:
    MpvController &player_;
    PlaylistController &playlist_;
    SessionStore store_;
    PlaybackHistoryStore historyStore_;
    TrackPreferenceStore trackPreferenceStore_;
    QHash<QString, double> resumePositions_;
    QHash<QString, PlaybackHistoryEntry> historyEntries_;
    AudioPreferences audio_;
    TuningPreferences tuning_;
    QString mediaDirectory_;
    MediaLibraryPreferences mediaLibraryPreferences_;
    bool resumePlaybackPositionEnabled_ = true;
    QString loadError_;
    QString historyError_;
    bool stopRequested_ = false;
    QList<TrackPreferenceEntry> trackPreferences_;
    QString trackPreferenceError_;
    bool restoringTrackPreference_ = false;
    bool pendingMediaPreference_ = false;
    std::optional<TrackPreferenceEntry> loadedMediaFingerprint_;
    std::optional<qint64> requestedAudioTrackId_;
    std::optional<qint64> requestedSubtitleTrackId_;
    void rememberPlaybackPosition(const QString &path, double position, double duration);
    QString savePlaybackHistory() const;
    void restoreTrackPreference();
    void observeTrackPreference();
    void rememberTrackPreference();
    QString saveTrackPreferences();
};
