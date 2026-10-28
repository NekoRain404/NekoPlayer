#include "ui/shortcutoverride.h"

#include <QApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QListView>
#include <QToolButton>
#include <QtTest>

class ShortcutOverrideTest final : public QObject {
    Q_OBJECT
  private slots:
    void activationKeysStayWithFocusedButtons() {
        QToolButton button;
        button.installEventFilter(
            new ShortcutOverrideFilter(ShortcutOverrideFilter::Mode::Activation, &button));

        QKeyEvent space(QEvent::ShortcutOverride, Qt::Key_Space, Qt::NoModifier,
                        QStringLiteral(" "));
        QCoreApplication::sendEvent(&button, &space);
        QVERIFY(space.isAccepted());

        QKeyEvent letter(QEvent::ShortcutOverride, Qt::Key_M, Qt::NoModifier,
                         QStringLiteral("m"));
        QCoreApplication::sendEvent(&button, &letter);
        QVERIFY(!letter.isAccepted());
    }

    void itemViewKeepsNavigationAndTypeAhead() {
        QListView view;
        view.installEventFilter(
            new ShortcutOverrideFilter(ShortcutOverrideFilter::Mode::ItemView, &view));

        for (const auto key : {Qt::Key_Up, Qt::Key_Down, Qt::Key_Left, Qt::Key_Right,
                               Qt::Key_Home, Qt::Key_End, Qt::Key_PageUp, Qt::Key_PageDown}) {
            QKeyEvent event(QEvent::ShortcutOverride, key, Qt::NoModifier, QString());
            QCoreApplication::sendEvent(&view, &event);
            QVERIFY2(event.isAccepted(), qPrintable(QKeySequence(key).toString()));
        }

        QKeyEvent typed(QEvent::ShortcutOverride, Qt::Key_S, Qt::NoModifier, QStringLiteral("s"));
        QCoreApplication::sendEvent(&view, &typed);
        QVERIFY(typed.isAccepted());

        QKeyEvent move(QEvent::ShortcutOverride, Qt::Key_Up, Qt::ControlModifier, QString());
        QCoreApplication::sendEvent(&view, &move);
        QVERIFY(!move.isAccepted());

        QKeyEvent tab(QEvent::ShortcutOverride, Qt::Key_Tab, Qt::NoModifier, QString());
        QCoreApplication::sendEvent(&view, &tab);
        QVERIFY(!tab.isAccepted());

        QKeyEvent escape(QEvent::ShortcutOverride, Qt::Key_Escape, Qt::NoModifier, QString());
        QCoreApplication::sendEvent(&view, &escape);
        QVERIFY(!escape.isAccepted());
    }
};

QTEST_MAIN(ShortcutOverrideTest)
#include "shortcut_override_test.moc"
