#pragma once

#include <QTimer>
#include <QWidget>

class QPaintEvent;

class ActionFeedbackOverlay final : public QWidget {
    Q_OBJECT
  public:
    explicit ActionFeedbackOverlay(QWidget *parent = nullptr);
    void showMessage(const QString &message, int durationMs);
    void clearMessage();

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    QString message_;
    QTimer timer_;
};
