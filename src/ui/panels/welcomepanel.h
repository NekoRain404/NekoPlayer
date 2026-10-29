#pragma once

#include "session/playbackhistorystore.h"
#include <QStringList>
#include <QWidget>

class QAction;
class QBoxLayout;
class QResizeEvent;
class QVBoxLayout;

// Lightweight empty-state surface; actions remain owned by MainWindow.
class WelcomePanel final : public QWidget {
    Q_OBJECT
  public:
    WelcomePanel(QAction *open, QAction *network, QWidget *parent = nullptr);
    void setRecentSources(const QStringList &sources);
    void setResumeEntries(const QList<PlaybackHistoryEntry> &entries);

  signals:
    void recentSourceRequested(const QString &source);
    void resumeSourceRequested(const QString &source);

  protected:
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

  private:
    void updateButtonDirection();
    void updateRecentButtonLabels();
    void updateVerticalDensity();
    void retranslateUi();

    QBoxLayout *buttonsLayout_ = nullptr;
    QVBoxLayout *recentLayout_ = nullptr;
    QVBoxLayout *continueLayout_ = nullptr;
    QStringList recentSources_;
    QList<PlaybackHistoryEntry> resumeEntries_;
};
