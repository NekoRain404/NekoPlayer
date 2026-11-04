#pragma once

#include "playback/mpv/mpvcontroller.h"

#include <QDialog>

class QLineEdit;
class QPlainTextEdit;

class NetworkOpenDialog final : public QDialog {
    Q_OBJECT
  public:
    explicit NetworkOpenDialog(QWidget *parent = nullptr);
    [[nodiscard]] QString mediaUrl() const;
    [[nodiscard]] NetworkOptions networkOptions() const;
    void setNetworkOptions(const NetworkOptions &options);

  private:
    QLineEdit *address_ = nullptr;
    QLineEdit *proxy_ = nullptr;
    QPlainTextEdit *headers_ = nullptr;
    QLineEdit *cookies_ = nullptr;
};
