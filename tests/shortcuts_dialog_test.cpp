#include "ui/shortcutsdialog.h"

#include <QAbstractItemView>
#include <QAction>
#include <QKeySequence>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QtTest>

namespace {
QString literal(const char *text) { return QString::fromLatin1(text); }

QTreeWidget *treeOf(ShortcutsDialog &dialog) {
    return dialog.findChild<QTreeWidget *>(QStringLiteral("shortcutsTree"));
}

QList<QTreeWidgetItem *> rowsWithText(QTreeWidget *tree, const QString &text) {
    QList<QTreeWidgetItem *> rows;
    for (int top = 0; top < tree->topLevelItemCount(); ++top) {
        auto *item = tree->topLevelItem(top);
        if (item->text(0) == text)
            rows.append(item);
        for (int child = 0; child < item->childCount(); ++child) {
            if (item->child(child)->text(0) == text)
                rows.append(item->child(child));
        }
    }
    return rows;
}

QString native(const QKeySequence &sequence) { return sequence.toString(QKeySequence::NativeText); }
} // namespace

class ShortcutsDialogTest final : public QObject {
    Q_OBJECT
  private slots:
    void listsOnlyShortcutActionsWithMenuScopes() {
        QMenuBar menuBar;
        auto *file = menuBar.addMenu(literal("File(&F)"));
        auto *open = file->addAction(literal("Open"));
        open->setShortcut(QKeySequence::Open);
        file->addAction(literal("No Shortcut"));

        auto *edit = menuBar.addMenu(literal("Edit(&E)"));
        auto *rename = edit->addAction(literal("Rename"));
        rename->setShortcut(QKeySequence(Qt::Key_X));
        rename->setShortcutContext(Qt::WidgetShortcut);
        auto *panel = edit->addAction(literal("Panel"));
        panel->setShortcut(QKeySequence(Qt::Key_Y));
        panel->setShortcutContext(Qt::WidgetWithChildrenShortcut);

        auto *nested = edit->addMenu(literal("Nested(&N)"));
        auto *nestedAction = nested->addAction(literal("Nested Action"));
        nestedAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));

        auto *empty = edit->addMenu(literal("Empty"));
        empty->addAction(literal("Also No Shortcut"));

        ShortcutsDialog dialog(&menuBar);
        auto *tree = treeOf(dialog);
        QVERIFY(tree);
        QCOMPARE(dialog.objectName(), QStringLiteral("shortcutsDialog"));
        QCOMPARE(dialog.windowTitle(), QStringLiteral("快捷键"));
        QCOMPARE(tree->columnCount(), 3);
        QCOMPARE(tree->headerItem()->text(0), QStringLiteral("操作"));
        QCOMPARE(tree->headerItem()->text(1), QStringLiteral("快捷键"));
        QCOMPARE(tree->headerItem()->text(2), QStringLiteral("范围"));
        QCOMPARE(tree->selectionMode(), QAbstractItemView::NoSelection);

        QCOMPARE(tree->topLevelItemCount(), 2);
        auto *fileItem = tree->topLevelItem(0);
        QCOMPARE(fileItem->text(0), QStringLiteral("File(F)"));
        QCOMPARE(fileItem->childCount(), 1);
        QCOMPARE(fileItem->child(0)->text(0), QStringLiteral("Open"));
        QCOMPARE(fileItem->child(0)->text(1), native(QKeySequence(QKeySequence::Open)));
        QCOMPARE(fileItem->child(0)->text(2), QStringLiteral("全局"));

        auto *editItem = tree->topLevelItem(1);
        QCOMPARE(editItem->text(0), QStringLiteral("Edit(E)"));
        QCOMPARE(editItem->childCount(), 3);
        QCOMPARE(editItem->child(0)->text(0), QStringLiteral("Rename"));
        QCOMPARE(editItem->child(0)->text(1), native(QKeySequence(Qt::Key_X)));
        QCOMPARE(editItem->child(0)->text(2), QStringLiteral("列表聚焦"));
        QCOMPARE(editItem->child(1)->text(0), QStringLiteral("Panel"));
        QCOMPARE(editItem->child(1)->text(2), QStringLiteral("当前面板"));
        QCOMPARE(editItem->child(2)->text(0), QStringLiteral("Nested(N)"));
        QCOMPARE(editItem->child(2)->childCount(), 1);
        QCOMPARE(editItem->child(2)->child(0)->text(0), QStringLiteral("Nested Action"));
        QCOMPARE(editItem->child(2)->child(0)->text(1), native(QKeySequence(Qt::CTRL | Qt::Key_K)));
        QCOMPARE(editItem->child(2)->child(0)->text(2), QStringLiteral("全局"));

        QVERIFY(rowsWithText(tree, QStringLiteral("No Shortcut")).isEmpty());
        QVERIFY(rowsWithText(tree, QStringLiteral("Also No Shortcut")).isEmpty());
        QVERIFY(rowsWithText(tree, QStringLiteral("Empty")).isEmpty());
    }

    void sharedActionAppearsOnceAtItsFirstMenu() {
        QMenuBar menuBar;
        auto *first = menuBar.addMenu(literal("First"));
        auto *second = menuBar.addMenu(literal("Second"));
        QAction shared(literal("Shared"));
        shared.setShortcut(QKeySequence::Save);
        first->addAction(&shared);
        second->addAction(&shared);

        ShortcutsDialog dialog(&menuBar);
        auto *tree = treeOf(dialog);
        QCOMPARE(rowsWithText(tree, QStringLiteral("Shared")).size(), 1);
        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("First"));
        QCOMPARE(tree->topLevelItem(0)->childCount(), 1);
    }

    void joinsMultipleShortcutsUsingNativeText() {
        QMenuBar menuBar;
        auto *file = menuBar.addMenu(literal("File"));
        auto *action = file->addAction(literal("Two Keys"));
        action->setShortcuts(
            {QKeySequence(Qt::CTRL | Qt::Key_K), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K)});

        ShortcutsDialog dialog(&menuBar);
        const auto rows = rowsWithText(treeOf(dialog), QStringLiteral("Two Keys"));
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.constFirst()->text(1),
                 native(QKeySequence(Qt::CTRL | Qt::Key_K)) + QStringLiteral(", ") +
                     native(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K)));
    }

    void newDialogReflectsActionsAddedLater() {
        QMenuBar menuBar;
        auto *menu = menuBar.addMenu(literal("Menu"));
        ShortcutsDialog before(&menuBar);
        QCOMPARE(treeOf(before)->topLevelItemCount(), 0);

        auto *late = menu->addAction(literal("Late"));
        late->setShortcut(QKeySequence(Qt::Key_Z));

        ShortcutsDialog after(&menuBar);
        QCOMPARE(treeOf(after)->topLevelItemCount(), 1);
        QCOMPARE(rowsWithText(treeOf(after), QStringLiteral("Late")).size(), 1);
    }

    void extraActionsAppearUnderPanelSectionWithoutDuplicates() {
        QMenuBar menuBar;
        auto *playlist = menuBar.addMenu(literal("Playlist"));
        QAction menuAction(literal("Menu Search"));
        menuAction.setShortcut(QKeySequence::Find);
        playlist->addAction(&menuAction);

        QAction focusAction(QStringLiteral("搜索播放列表"));
        focusAction.setShortcut(QKeySequence::Find);
        focusAction.setShortcutContext(Qt::WidgetWithChildrenShortcut);
        QAction clearAction(QStringLiteral("清除播放列表搜索"));
        clearAction.setShortcut(QKeySequence(Qt::Key_Escape));
        clearAction.setShortcutContext(Qt::WidgetWithChildrenShortcut);
        QAction noShortcut(literal("No Shortcut"));

        ShortcutsDialog dialog(&menuBar, {&menuAction, &focusAction, &clearAction, &noShortcut});
        auto *tree = treeOf(dialog);
        QCOMPARE(tree->topLevelItemCount(), 2);
        QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("Playlist"));
        QCOMPARE(tree->topLevelItem(0)->childCount(), 1);

        auto *panel = tree->topLevelItem(1);
        QCOMPARE(panel->text(0), QStringLiteral("面板快捷键"));
        QCOMPARE(panel->childCount(), 2);
        QCOMPARE(panel->child(0)->text(0), QStringLiteral("搜索播放列表"));
        QCOMPARE(panel->child(0)->text(2), QStringLiteral("当前面板"));
        QCOMPARE(panel->child(1)->text(0), QStringLiteral("清除播放列表搜索"));
        QCOMPARE(panel->child(1)->text(1), native(QKeySequence(Qt::Key_Escape)));
        QCOMPARE(panel->child(1)->text(2), QStringLiteral("当前面板"));
        QCOMPARE(rowsWithText(tree, QStringLiteral("Menu Search")).size(), 1);
        QVERIFY(rowsWithText(tree, QStringLiteral("No Shortcut")).isEmpty());
    }

    void emptyExtraActionsPrunePanelSection() {
        QMenuBar menuBar;
        auto *file = menuBar.addMenu(literal("File"));
        auto *open = file->addAction(literal("Open"));
        open->setShortcut(QKeySequence::Open);

        ShortcutsDialog dialog(&menuBar, {});
        auto *tree = treeOf(dialog);
        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("File"));
        QVERIFY(rowsWithText(tree, QStringLiteral("面板快捷键")).isEmpty());
    }
};

QTEST_MAIN(ShortcutsDialogTest)
#include "shortcuts_dialog_test.moc"
