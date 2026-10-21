#pragma once

#include <QDialog>

class MpvController;
class QComboBox;
class QLabel;
class QPushButton;

class PlaybackSettingsDialog final : public QDialog {
    Q_OBJECT
  public:
    explicit PlaybackSettingsDialog(MpvController &player, QWidget *parent = nullptr);

  private:
    void apply();
    void updateApplyButton();
    void updateActualDecoder();
    MpvController &player_;
    QComboBox *hardwareDecoding_;
    QLabel *actualDecoder_;
    QPushButton *applyButton_;
    bool lastActive_ = false;
    QString lastDecoder_;
};
