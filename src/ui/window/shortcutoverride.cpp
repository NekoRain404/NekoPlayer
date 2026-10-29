#include "ui/window/shortcutoverride.h"

#include <QKeyEvent>

namespace {
bool isActivationKey(const QKeyEvent &event) {
    // Keep modifiers (including keypad Enter) out of the button activation path.
    if (event.modifiers() != Qt::NoModifier && event.modifiers() != Qt::KeypadModifier)
        return false;
    const auto key = event.key();
    return key == Qt::Key_Space || key == Qt::Key_Return || key == Qt::Key_Enter;
}

bool isItemViewKey(const QKeyEvent &event) {
    // Modified combinations stay with the application shortcuts (Ctrl+Up moves
    // the row); Shift is allowed because it belongs to list selection.
    if (event.modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        return false;
    switch (event.key()) {
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
        return true;
    default:
        break;
    }
    // Unmodified printable input drives the view's incremental search.
    const auto text = event.text();
    return !text.isEmpty() && text.front().isPrint();
}
} // namespace

ShortcutOverrideFilter::ShortcutOverrideFilter(Mode mode, QObject *parent)
    : QObject(parent), mode_(mode) {}

bool ShortcutOverrideFilter::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::ShortcutOverride) {
        const auto &key = *static_cast<QKeyEvent *>(event);
        const bool keep = mode_ == Mode::Activation ? isActivationKey(key) : isItemViewKey(key);
        if (keep) {
            event->accept();
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}
