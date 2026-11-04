#pragma once

#include <QDialog>
#include <QList>

class QSlider;
class QLabel;

class EqualizerDialog final : public QDialog {
    Q_OBJECT
  public:
    explicit EqualizerDialog(const QList<double> &gains, QWidget *parent = nullptr);
    [[nodiscard]] QList<double> gains() const;

  private:
    void setGains(const QList<double> &gains);
    void updateValues();
    QList<QSlider *> sliders_;
    QList<QLabel *> values_;
};
