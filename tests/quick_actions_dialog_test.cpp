#include "ui/quickactionsdialog.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPixmap>
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

    void rowsReuseActionIconAndMenuContextWhileDeduplicating() {
        QWidget root;
        QMenu playback(QStringLiteral("播放"), &root);
        QAction action(QStringLiteral("播放/暂停"), &root);
        QPixmap iconPixmap(8, 8);
        iconPixmap.fill(Qt::red);
        action.setIcon(QIcon(iconPixmap));
        playback.addAction(&action);
        QMenu context(QStringLiteral("上下文菜单"), &root);
        context.addAction(&action);

        QuickActionsDialog dialog(&root);
        dialog.refreshActions();
        QCOMPARE(dialog.visibleActionCount(), 1);
        const auto *item = dialog.resultsView()->item(0);
        QVERIFY(item->text().contains(QStringLiteral("播放")));
        QVERIFY(item->text().contains(QStringLiteral("上下文菜单")));
        QVERIFY(!item->icon().isNull());
        QCOMPARE(item->data(Qt::UserRole).value<QObject *>(), &action);
    }

    void emptyStateTracksWhetherActionsMatch() {
        QWidget root;
        QAction action(QStringLiteral("打开文件"), &root);
        QuickActionsDialog dialog(&root);
        auto *empty = dialog.findChild<QLabel *>(QStringLiteral("quickActionsEmptyState"));
        QVERIFY(empty);
        dialog.show();
        dialog.searchEdit()->setText(QStringLiteral("does-not-exist"));
        QVERIFY(empty->isVisible());
        QVERIFY(dialog.resultsView()->isHidden());
        dialog.searchEdit()->clear();
        QVERIFY(empty->isHidden());
        QVERIFY(dialog.resultsView()->isVisible());
    }

    void substringMatchesPrecedeOrderedFuzzyMatches() {
        QWidget root;
        QAction fuzzy(QStringLiteral("打开网络地址"), &root);
        QAction substring(QStringLiteral("最近网地地址"), &root);
        QAction outOfOrder(QStringLiteral("地址网络"), &root);
        QuickActionsDialog dialog(&root);
        dialog.show();
        dialog.searchEdit()->setText(QStringLiteral("网地"));
        QCOMPARE(dialog.visibleActionCount(), 2);
        QCOMPARE(dialog.resultsView()->item(0)->data(Qt::UserRole).value<QObject *>(), &substring);
        QCOMPARE(dialog.resultsView()->item(1)->data(Qt::UserRole).value<QObject *>(), &fuzzy);
    }

    void closingRestoresPreviouslyFocusedControl() {
        QWidget root;
        QLineEdit originalFocus(&root);
        root.show();
        originalFocus.setFocus();
        QTRY_COMPARE(QApplication::focusWidget(), &originalFocus);
        QuickActionsDialog dialog(&root);
        auto *open = root.findChild<QAction *>(QStringLiteral("quickActionsAction"));
        QVERIFY(open);
        open->trigger();
        QTRY_COMPARE(QApplication::focusWidget(), dialog.searchEdit());
        dialog.hide();
        QTRY_COMPARE(QApplication::focusWidget(), &originalFocus);
    }

    void refreshUsesCurrentActionTextAndIcon() {
        QWidget root;
        QAction action(QStringLiteral("旧标题"), &root);
        QuickActionsDialog dialog(&root);
        action.setText(QStringLiteral("新标题"));
        QPixmap iconPixmap(8, 8);
        iconPixmap.fill(Qt::blue);
        action.setIcon(QIcon(iconPixmap));
        dialog.refreshActions();
        QCOMPARE(dialog.resultsView()->item(0)->text(), QStringLiteral("新标题"));
        QVERIFY(!dialog.resultsView()->item(0)->icon().isNull());
    }

    void doubleClickingResultTriggersItsAction() {
        QWidget root;
        QAction action(QStringLiteral("播放/暂停"), &root);
        QSignalSpy triggered(&action, &QAction::triggered);
        QuickActionsDialog dialog(&root);
        dialog.show();
        const QRect itemRect = dialog.resultsView()->visualItemRect(dialog.resultsView()->item(0));
        QTest::mouseClick(dialog.resultsView()->viewport(), Qt::LeftButton, Qt::NoModifier,
                          itemRect.center());
        QTest::mouseDClick(dialog.resultsView()->viewport(), Qt::LeftButton, Qt::NoModifier,
                           itemRect.center());
        QCOMPARE(triggered.size(), 1);
        QVERIFY(dialog.isHidden());
    }
};

QTEST_MAIN(QuickActionsDialogTest)
#include "quick_actions_dialog_test.moc"
