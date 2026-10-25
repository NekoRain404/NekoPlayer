#pragma once

#include <QObject>
#include <array>

class PlaylistController;
class QAction;
class QMenu;
class QWidget;

class PlaylistMaintenanceMenu final : public QObject {
    Q_OBJECT
  public:
    explicit PlaylistMaintenanceMenu(PlaylistController &playlist, QWidget *dialogParent,
                                     QObject *parent = nullptr);
    [[nodiscard]] QMenu *sortMenu() const;
    [[nodiscard]] QAction *removeMissingAction() const;
    void retranslate();

  signals:
    void feedback(const QString &message);

  private:
    void removeMissingFiles();
    PlaylistController &playlist_;
    QWidget *dialogParent_;
    QMenu *sortMenu_;
    std::array<QAction *, 4> sortActions_{};
    QAction *removeMissingAction_ = nullptr;
};
