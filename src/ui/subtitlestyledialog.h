#pragma once

#include "playback/subtitlepreferences.h"

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class MpvController;

// Editing surface for the durable subtitle appearance fields. The dialog applies
// on OK and leaves the controller untouched on Cancel, matching the other
// appearance dialogs.
class SubtitleStyleDialog final : public QDialog {
    Q_OBJECT
  public:
    explicit SubtitleStyleDialog(MpvController &controller, QWidget *parent = nullptr);

  private:
    QPushButton *createColorButton(const QString &color, const char *objectName);
    void chooseColor(QPushButton *button);
    SubtitlePreferences draft() const;
    void setControls(const SubtitlePreferences &preferences);

    MpvController &controller_;
    QPushButton *textColor_ = nullptr;
    QPushButton *outlineColor_ = nullptr;
    QPushButton *backgroundColor_ = nullptr;
    QDoubleSpinBox *outlineSize_ = nullptr;
    QDoubleSpinBox *shadowOffset_ = nullptr;
    QComboBox *codepage_ = nullptr;
};
