#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>

class MpvController;

// Routes one complete desktop drop; validation precedes any side effects.
class LocalFileDrop final : public QObject {
    Q_OBJECT
  public:
    explicit LocalFileDrop(MpvController &player, QObject *parent = nullptr);
    [[nodiscard]] bool handleUrls(const QList<QUrl> &urls);

  signals:
    void mediaFilesDropped(const QStringList &paths);
    void errorOccurred(const QString &message);

  private:
    MpvController &player_;
};
