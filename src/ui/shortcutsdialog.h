#pragma once

#include <QDialog>

class QMenuBar;
class QTreeWidget;

class ShortcutsDialog final : public QDialog {
    Q_OBJECT
  public:
    explicit ShortcutsDialog(QMenuBar *menuBar, QWidget *parent = nullptr);

  private:
    QTreeWidget *tree_;
};
