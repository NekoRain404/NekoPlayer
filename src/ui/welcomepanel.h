#pragma once

#include <QWidget>

class QAction;
class QBoxLayout;
class QResizeEvent;

// Lightweight empty-state surface; actions remain owned by MainWindow.
class WelcomePanel final : public QWidget {
    Q_OBJECT
  public:
    WelcomePanel(QAction *open, QAction *network, QWidget *parent = nullptr);

  protected:
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

  private:
    void updateButtonDirection();
    void retranslateUi();

    QBoxLayout *buttonsLayout_ = nullptr;
};
