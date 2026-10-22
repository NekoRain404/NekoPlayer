#pragma once

#include <QObject>
#include <QStringList>
#include <functional>

class FolderScanJob;
class QAction;
class QMenu;
class QProgressDialog;
class QWidget;

class FolderMenu final : public QObject {
    Q_OBJECT
  public:
    FolderMenu(QWidget *window, std::function<QString()> initialDirectory);
    [[nodiscard]] QMenu *menu() const;
    [[nodiscard]] bool recursiveEnabled() const;
    void start(const QString &directory, bool append, bool recursive);
    void start(const QStringList &directories, bool append, bool recursive);
    void cancel();

  signals:
    void filesReady(const QStringList &paths, bool append);
    void feedback(const QString &message);
    void errorOccurred(const QString &message);

  private:
    void chooseDirectory(bool append);
    QWidget *window_;
    QMenu *menu_;
    QAction *recursiveAction_;
    QAction *cancelAction_;
    QProgressDialog *progress_;
    FolderScanJob *job_;
    std::function<QString()> initialDirectory_;
    bool append_ = false;
};
