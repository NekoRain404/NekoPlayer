#pragma once

#include <QObject>

class QKeyEvent;

// Restores standard keyboard behaviour for widgets whose keys a window-level
// single-key shortcut would otherwise steal. Qt sends a ShortcutOverride event
// to the focus widget before matching shortcuts; accepting it lets the widget
// keep the key.
class ShortcutOverrideFilter final : public QObject {
  public:
    enum class Mode {
        Activation, // Space/Return activate the focused button.
        ItemView,   // Navigation keys and type-ahead belong to the list.
    };

    explicit ShortcutOverrideFilter(Mode mode, QObject *parent = nullptr);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    Mode mode_;
};
