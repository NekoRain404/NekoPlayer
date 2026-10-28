#pragma once

#include <QDialog>
#include <optional>

class QLineEdit;

class TimeSeekDialog final : public QDialog {
    Q_OBJECT
  public:
    TimeSeekDialog(double position, double duration, QWidget *parent = nullptr);
    [[nodiscard]] std::optional<double> selectedSeconds() const;

  private:
    QLineEdit *input_;
};
