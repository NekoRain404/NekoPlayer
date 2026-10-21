#pragma once

#include <QObject>

class MpvController;
class QAction;
class QMenu;
class QWidget;

class SubtitleDisplayMenu final : public QObject {
    Q_OBJECT
  public:
    explicit SubtitleDisplayMenu(MpvController &controller, QWidget *window);
    [[nodiscard]] QMenu *menu() const;
    [[nodiscard]] QAction *settingsAction() const;

  signals:
    void feedback(const QString &message);

  private:
    void update();
    MpvController &controller_;
    QMenu *menu_;
    QAction *visible_;
    QAction *settings_;
};
