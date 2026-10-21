#pragma once

#include "sessionstore.h"
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
    AudioPreferences audio_;
    QString mediaDirectory_;
    QString loadError_;
};
