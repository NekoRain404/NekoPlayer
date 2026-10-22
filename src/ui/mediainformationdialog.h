#pragma once

#include <QDialog>

class MpvController;
class QPlainTextEdit;
class QPushButton;
class QEvent;

class MediaInformationDialog final : public QDialog {
    Q_OBJECT
  public:
    explicit MediaInformationDialog(MpvController &player, QWidget *parent = nullptr);

  protected:
    void changeEvent(QEvent *event) override;

  private:
    void updateState();
    void updateReport();
    MpvController &player_;
    QPlainTextEdit *report_;
    QPushButton *copy_;
    bool lastActive_ = false;
    QString lastPath_;
    QString lastDecoder_;
};
