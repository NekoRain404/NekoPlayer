#pragma once

#include "playbackhistorystore.h"
#include "sessionstore.h"
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
    void rememberMedia(const QString &path);

  private:
    MpvController &player_;
    PlaylistController &playlist_;
    SessionStore store_;
    PlaybackHistoryStore historyStore_;
    QHash<QString, double> resumePositions_;
    QHash<QString, PlaybackHistoryEntry> historyEntries_;
    AudioPreferences audio_;
    TuningPreferences tuning_;
    QString mediaDirectory_;
    QString loadError_;
    QString historyError_;
    bool stopRequested_ = false;
    void rememberPlaybackPosition(const QString &path, double position, double duration);
    QString savePlaybackHistory() const;
};
