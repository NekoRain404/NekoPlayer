#pragma once

#include <QTimer>
#include <QWidget>

class MpvController;
class QLabel;
class QFrame;
class QProgressBar;
class QEvent;

// Event-driven presentation only; owns no playback or graphics resources.
class PlaybackOverlay final : public QWidget {
    Q_OBJECT
  public:
    explicit PlaybackOverlay(MpvController &controller, QWidget *parent);
    void setInformationVisible(bool visible);
    bool showSeek(const QString &message, int durationMs = 1800);
    void clearSeek();

  protected:
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

  private:
    void present();
    void arrangePanels();
    void updateVisibility();
    MpvController &controller_;
    QLabel *information_;
    QFrame *seekPanel_;
    QLabel *seekLabel_;
    QProgressBar *progress_;
    QTimer seekTimer_;
    QString seekMessage_;
    bool informationVisible_ = false;
    bool seekVisible_ = false;
};
