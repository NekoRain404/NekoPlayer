#pragma once

#include <QPoint>
#include <QRect>
#include <QWidget>

class QAction;
class QFrame;
class QLabel;
class QSlider;
class QEvent;
class QResizeEvent;
struct PlaybackState;

// Presents state and emits intent; knows neither libmpv nor the main window.
class TransportBar final : public QWidget {
    Q_OBJECT
  public:
    TransportBar(QAction *open, QAction *previous, QAction *pause, QAction *next, QAction *stop,
                 QAction *mute, QAction *fullscreen, QWidget *parent = nullptr);
    void present(const PlaybackState &state);
    [[nodiscard]] QRect timelineGlobalGeometry() const;

  protected:
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

  signals:
    void seekRequested(double seconds);
    void volumeRequested(double volume);
    void wheelSeekRequested(int direction);
    void timelineHovered(double seconds, QPoint globalPosition);
    void timelineHoverEnded();

  private:
    void applyResponsiveMetrics();
    QSlider *timeline_;
    QSlider *volume_;
    QLabel *time_;
    QFrame *timeDivider_;
    QFrame *audioDivider_;
    double duration_ = 0;
};
