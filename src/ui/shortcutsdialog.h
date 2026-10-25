#pragma once

#include <QDialog>
#include <QList>

class QAction;
class QMenuBar;
class QTreeWidget;

class ShortcutsDialog final : public QDialog {
    Q_OBJECT
  public:
    explicit ShortcutsDialog(QMenuBar *menuBar, const QList<QAction *> &extraActions = {},
                             QWidget *parent = nullptr);

  private:
    QTreeWidget *tree_;
};
