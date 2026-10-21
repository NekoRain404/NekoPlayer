#pragma once

#include <QDialog>

class MpvController;
class QPlainTextEdit;

class MediaInformationDialog final : public QDialog {
    Q_OBJECT
  public:
    explicit MediaInformationDialog(MpvController &player, QWidget *parent = nullptr);

  private:
    void updateState();
    void updateReport();
    MpvController &player_;
    QPlainTextEdit *report_;
    bool lastActive_ = false;
    QString lastPath_;
    QString lastDecoder_;
};
