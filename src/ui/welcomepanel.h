#pragma once

#include <QWidget>

class QAction;

// Lightweight empty-state surface; actions remain owned by MainWindow.
class WelcomePanel final : public QWidget {
    Q_OBJECT
  public:
    WelcomePanel(QAction *open, QAction *network, QWidget *parent = nullptr);

  protected:
    void changeEvent(QEvent *event) override;

  private:
    void retranslateUi();
};
