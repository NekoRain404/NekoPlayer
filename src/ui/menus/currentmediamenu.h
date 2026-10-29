#pragma once

#include <QObject>

class QAction;
class QMenu;
class QWidget;
class MpvController;

class CurrentMediaMenu final : public QObject {
    Q_OBJECT
  public:
    CurrentMediaMenu(MpvController &controller, QWidget *window);
    [[nodiscard]] QMenu *menu() const;

  signals:
    void feedback(const QString &message);
    void errorOccurred(const QString &message);

  private:
    void updateAvailability();
    MpvController &controller_;
    QMenu *menu_;
    QAction *copySource_;
    QAction *copyTime_;
    QAction *openDirectory_;
};
