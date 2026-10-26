#pragma once

#include <QObject>

class QGraphicsOpacityEffect;
class QPropertyAnimation;
class QVBoxLayout;
class QWidget;

// Moves the existing transport above the video without touching the GL widget.
class FullscreenControls final : public QObject {
    Q_OBJECT
  public:
    FullscreenControls(QWidget &transport, QWidget &video, QVBoxLayout &normalLayout,
                       QObject *parent = nullptr);
    void enter();
    void leave();
    void reveal();
    void fadeOut();
    [[nodiscard]] bool interactionActive() const;

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void arrange();
    void animateTo(qreal opacity);
    void finishFadeOut();
    QWidget &transport_;
    QWidget &video_;
    QWidget *normalParent_;
    QVBoxLayout &normalLayout_;
    QGraphicsOpacityEffect *opacity_;
    QPropertyAnimation *animation_;
    int normalIndex_ = -1;
    bool active_ = false;
    bool keyboardFocus_ = false;
};
