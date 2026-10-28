#pragma once

#include <QList>
#include <QObject>

class MpvController;
class QMenu;
class QWidget;
class QAction;
class QActionGroup;
class QTimer;

class NavigationMenu final : public QObject {
    Q_OBJECT
  public:
    NavigationMenu(MpvController &player, QWidget *window);
    [[nodiscard]] QMenu *menu() const;
    void retranslate();
    // Test seam: arms the same session-only timer with a short duration.
    void setSleepTimerForTest(int milliseconds);

  signals:
    void feedback(const QString &message);
    void seekFeedback(const QString &message);

  private:
    QString sleepTimerText(int seconds) const;
    void selectSleepTimer(QAction *action);
    void cancelSleepTimer();
    void sleepTimerExpired();
    void rebuildChapters();
    void updateState();
    void chooseTime();
    MpvController &player_;
    QWidget *window_;
    QMenu *menu_;
    QMenu *chaptersMenu_;
    QActionGroup *chapterGroup_;
    QMenu *sleepMenu_;
    QAction *sleepOff_;
    QList<QAction *> sleepActions_;
    QTimer *sleepTimer_;
    QAction *seekTime_;
    QAction *previous_;
    QAction *next_;
    QAction *previousFrame_;
    QAction *nextFrame_;
    QList<QAction *> seekActions_;
    int presentedChapter_ = -2;
    int presentedNavigationChapter_ = -2;
    bool presentedSeekable_ = false;
    bool presentedTimeSeekable_ = false;
    bool presentedFrameForward_ = false;
    bool presentedFrameBackward_ = false;
};
