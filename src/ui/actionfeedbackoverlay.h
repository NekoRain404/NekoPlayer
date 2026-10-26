#pragma once

#include <QPropertyAnimation>
#include <QTimer>
#include <QWidget>

class QPaintEvent;

class ActionFeedbackOverlay final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal contentOpacity READ contentOpacity WRITE setContentOpacity)
  public:
    explicit ActionFeedbackOverlay(QWidget *parent = nullptr);
    void showMessage(const QString &message, int durationMs);
    void clearMessage();
    [[nodiscard]] qreal contentOpacity() const;

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    void animateContentTo(qreal opacity);
    void setContentOpacity(qreal opacity);

    QString message_;
    QTimer timer_;
    qreal contentOpacity_ = 0.0;
    QPropertyAnimation *animation_;
};
