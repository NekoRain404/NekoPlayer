#pragma once

#include <QWidget>

class QAction;
class QLabel;
class QSlider;
class QEvent;
struct PlaybackState;

// Presents state and emits intent; knows neither libmpv nor the main window.
class TransportBar final : public QWidget {
    Q_OBJECT
  public:
    TransportBar(QAction *open, QAction *previous, QAction *pause, QAction *next, QAction *stop,
                 QAction *mute, QAction *fullscreen, QWidget *parent = nullptr);
    void present(const PlaybackState &state);

  protected:
    void changeEvent(QEvent *event) override;

  signals:
    void seekRequested(double seconds);
    void volumeRequested(double volume);

  private:
    QSlider *timeline_;
    QSlider *volume_;
    QLabel *time_;
    double duration_ = 0;
};
