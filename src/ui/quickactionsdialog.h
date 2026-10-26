#pragma once

#include <QDialog>
#include <QPointer>

class QAction;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QObject;

class QuickActionsDialog final : public QDialog {
    Q_OBJECT
  public:
    explicit QuickActionsDialog(QWidget *actionRoot, QWidget *parent = nullptr);

    void refreshActions();
    [[nodiscard]] QLineEdit *searchEdit() const;
    [[nodiscard]] QListWidget *resultsView() const;
    [[nodiscard]] int visibleActionCount() const;

  protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void filterActions();
    void trigger(QListWidgetItem *item);

    QPointer<QWidget> actionRoot_;
    QPointer<QWidget> previousFocus_;
    QAction *openAction_;
    QLineEdit *search_;
    QListWidget *results_;
    QLabel *emptyState_;
};
