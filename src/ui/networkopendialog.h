#pragma once

#include <QDialog>

class QLineEdit;

class NetworkOpenDialog final : public QDialog {
    Q_OBJECT
  public:
    explicit NetworkOpenDialog(QWidget *parent = nullptr);
    [[nodiscard]] QString mediaUrl() const;

  private:
    QLineEdit *address_ = nullptr;
};
