#pragma once

#include <QObject>

class MpvController;
class QAction;
class QMenu;
class QWidget;

class ScreenshotMenu final : public QObject {
    Q_OBJECT
  public:
    explicit ScreenshotMenu(MpvController &controller, QWidget *window);
    [[nodiscard]] QMenu *menu() const;

  signals:
    void feedback(const QString &message);

  private:
    void chooseDestination(bool includeSubtitles);
    void updateActions();
    MpvController &controller_;
    QWidget *window_;
    QMenu *menu_;
    QAction *withSubtitles_;
    QAction *withoutSubtitles_;
    QString directory_;
};
