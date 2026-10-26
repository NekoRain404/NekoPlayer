#include "ui/quickactionsdialog.h"

#include <QAction>
#include <QApplication>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest>

class QuickActionsDialogTest final : public QObject {
    Q_OBJECT
  private slots:
    void filtersLiveActionsAndTriggersEnabledOnly() {
        QWidget root;
        QAction open(QStringLiteral("打开文件"), &root);
        open.setShortcut(QKeySequence::Open);
        QAction disabled(QStringLiteral("不可用操作"), &root);
        disabled.setEnabled(false);
        QMenu menu;
        menu.addAction(&open);
        QSignalSpy triggered(&open, &QAction::triggered);

        QuickActionsDialog dialog(&root);
        dialog.show();
        dialog.searchEdit()->setText(QStringLiteral("文件"));
        QCOMPARE(dialog.visibleActionCount(), 1);
        QCOMPARE(dialog.resultsView()->item(0)->data(Qt::UserRole).value<QObject *>(), &open);
        QTest::keyClick(dialog.searchEdit(), Qt::Key_Return);
        QCOMPARE(triggered.size(), 1);
        QVERIFY(dialog.isHidden());
    }

    void disabledActionsStayVisibleButCannotTrigger() {
        QWidget root;
        QAction disabled(QStringLiteral("不可用操作"), &root);
        disabled.setEnabled(false);
        QSignalSpy triggered(&disabled, &QAction::triggered);
        QuickActionsDialog dialog(&root);
        dialog.show();
        dialog.searchEdit()->setText(QStringLiteral("不可用"));
        QCOMPARE(dialog.visibleActionCount(), 1);
        QVERIFY(!dialog.resultsView()->item(0)->flags().testFlag(Qt::ItemIsEnabled));
        QTest::keyClick(dialog.searchEdit(), Qt::Key_Return);
        QCOMPARE(triggered.size(), 0);
        QVERIFY(dialog.isVisible());
    }

    void escapeClosesAndActionUsesCtrlK() {
        QWidget root;
        QuickActionsDialog dialog(&root);
        auto *action = root.findChild<QAction *>(QStringLiteral("quickActionsAction"));
        QVERIFY(action);
        QCOMPARE(action->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_K));
        dialog.show();
        QTest::keyClick(dialog.searchEdit(), Qt::Key_Escape);
        QVERIFY(dialog.isHidden());
    }

    void arrowDownMovesFromSearchIntoResults() {
        QWidget root;
        QAction first(QStringLiteral("第一项"), &root);
        QAction second(QStringLiteral("第二项"), &root);
        QuickActionsDialog dialog(&root);
        dialog.show();
        dialog.searchEdit()->setFocus();
        QCOMPARE(dialog.resultsView()->currentRow(), 0);
        QTest::keyClick(dialog.searchEdit(), Qt::Key_Down);
        QCOMPARE(dialog.resultsView()->currentRow(), 1);
    }

    void actionSnapshotRefreshesBeforeOpening() {
        QWidget root;
        QuickActionsDialog dialog(&root);
        QAction later(QStringLiteral("稍后添加"), &root);
        dialog.refreshActions();
        dialog.searchEdit()->setText(QStringLiteral("添加"));
        QCOMPARE(dialog.visibleActionCount(), 1);
    }
};

QTEST_MAIN(QuickActionsDialogTest)
#include "quick_actions_dialog_test.moc"
