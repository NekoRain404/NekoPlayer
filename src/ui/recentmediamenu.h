#pragma once

#include <QObject>

class QAction;
class QMenu;
class QWidget;
class RecentMediaHistory;

class RecentMediaMenu final : public QObject {
    Q_OBJECT
  public:
    RecentMediaMenu(RecentMediaHistory &history, QWidget *window);
    [[nodiscard]] QMenu *menu() const;

  signals:
    void openRequested(const QString &source);
    void cleared();
    void feedback(const QString &message);

  private:
    void rebuild();
    RecentMediaHistory &history_;
    QMenu *menu_;
    QAction *reopen_;
};
