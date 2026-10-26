#pragma once

#include <QObject>

class QPropertyAnimation;
class QWidget;

class WindowOpacityTransition final : public QObject {
    Q_OBJECT
  public:
    explicit WindowOpacityTransition(QWidget &window, QObject *parent = nullptr);
    void fadeTo(qreal opacity);
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] qreal targetOpacity() const;

  signals:
    void reached(qreal opacity);

  private:
    QWidget &window_;
    QPropertyAnimation *animation_;
};
