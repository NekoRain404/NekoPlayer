#include "shortcutsdialog.h"

#include <QAbstractItemView>
#include <QAction>
#include <QHeaderView>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QSet>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <functional>

namespace {
QString withoutMnemonics(QString text) {
    QString result;
    result.reserve(text.size());
    for (qsizetype index = 0; index < text.size(); ++index) {
        if (text.at(index) != QLatin1Char('&'))
            result.append(text.at(index));
        else if (index + 1 < text.size() && text.at(index + 1) == QLatin1Char('&')) {
            result.append(QLatin1Char('&'));
            ++index;
        }
    }
    return result;
}
} // namespace

ShortcutsDialog::ShortcutsDialog(QMenuBar *menuBar, const QList<QAction *> &extraActions,
                                 QWidget *parent)
    : QDialog(parent), tree_(new QTreeWidget(this)) {
    setObjectName(QStringLiteral("shortcutsDialog"));
    setWindowTitle(tr("快捷键"));
    setAttribute(Qt::WA_TranslucentBackground, true);

    tree_->setObjectName(QStringLiteral("shortcutsTree"));
    tree_->setColumnCount(3);
    tree_->setHeaderLabels({tr("操作"), tr("快捷键"), tr("范围")});
    tree_->setRootIsDecorated(true);
    tree_->setAlternatingRowColors(true);
    tree_->setSelectionMode(QAbstractItemView::NoSelection);
    tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    const auto shortcutText = [](const QAction *action) {
        QStringList sequences;
        for (const auto &sequence : action->shortcuts()) {
            if (!sequence.isEmpty())
                sequences.append(sequence.toString(QKeySequence::NativeText));
        }
        if (sequences.isEmpty() && !action->shortcut().isEmpty())
            sequences.append(action->shortcut().toString(QKeySequence::NativeText));
        return sequences.join(QStringLiteral(", "));
    };
    const auto scopeText = [this](Qt::ShortcutContext context) {
        if (context == Qt::WidgetShortcut)
            return tr("列表聚焦");
        if (context == Qt::WidgetWithChildrenShortcut)
            return tr("当前面板");
        return tr("全局");
    };
    QSet<const QAction *> listed;
    const auto appendAction = [&](QTreeWidgetItem *parentItem, QAction *action) {
        if (action->isSeparator() || listed.contains(action))
            return;
        const auto shortcuts = shortcutText(action);
        if (shortcuts.isEmpty())
            return;
        listed.insert(action);
        const QStringList columns{withoutMnemonics(action->text()), shortcuts,
                                  scopeText(action->shortcutContext())};
        if (parentItem)
            new QTreeWidgetItem(parentItem, columns);
        else
            new QTreeWidgetItem(tree_, columns);
    };
    std::function<void(QMenu *, QTreeWidgetItem *)> appendMenu;
    appendMenu = [&](QMenu *menu, QTreeWidgetItem *item) {
        for (auto *action : menu->actions()) {
            if (auto *submenu = action->menu()) {
                auto *submenuItem = new QTreeWidgetItem(item);
                submenuItem->setText(0, withoutMnemonics(submenu->title()));
                appendMenu(submenu, submenuItem);
                if (submenuItem->childCount() == 0)
                    delete submenuItem;
                continue;
            }
            appendAction(item, action);
        }
    };
    for (auto *action : menuBar->actions()) {
        if (auto *menu = action->menu()) {
            auto *item = new QTreeWidgetItem(tree_);
            item->setText(0, withoutMnemonics(menu->title()));
            appendMenu(menu, item);
            if (item->childCount() == 0)
                delete item;
        } else {
            appendAction(nullptr, action);
        }
    }
    auto *panelItem = new QTreeWidgetItem(tree_);
    panelItem->setText(0, tr("面板快捷键"));
    for (auto *action : extraActions) {
        if (action)
            appendAction(panelItem, action);
    }
    if (panelItem->childCount() == 0)
        delete panelItem;
    tree_->expandAll();

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(tree_);
    resize(620, 680);
}
