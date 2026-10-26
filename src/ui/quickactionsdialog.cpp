#include "quickactionsdialog.h"

#include <QAction>
#include <QApplication>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSet>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWidget>

QuickActionsDialog::QuickActionsDialog(QWidget *actionRoot, QWidget *parent)
    : QDialog(parent ? parent : actionRoot), actionRoot_(actionRoot),
      openAction_(new QAction(tr("快速操作…"), actionRoot)), search_(new QLineEdit(this)),
      results_(new QListWidget(this)) {
    setObjectName(QStringLiteral("quickActionsDialog"));
    setWindowTitle(tr("快速操作"));
    setModal(false);
    resize(480, 420);
    openAction_->setObjectName(QStringLiteral("quickActionsAction"));
    openAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    openAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    openAction_->setToolTip(openAction_->text());
    actionRoot_->addAction(openAction_);

    search_->setObjectName(QStringLiteral("quickActionsSearch"));
    search_->setPlaceholderText(tr("搜索命令或快捷键"));
    search_->setAccessibleName(tr("搜索快速操作"));
    results_->setObjectName(QStringLiteral("quickActionsResults"));
    results_->setAccessibleName(tr("操作结果"));
    results_->setUniformItemSizes(true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    layout->addWidget(search_);
    layout->addWidget(results_, 1);
    connect(openAction_, &QAction::triggered, this, [this] {
        refreshActions();
        show();
        raise();
        search_->setFocus(Qt::ShortcutFocusReason);
        search_->selectAll();
    });
    connect(search_, &QLineEdit::textChanged, this, &QuickActionsDialog::filterActions);
    connect(search_, &QLineEdit::returnPressed, this, [this] { trigger(results_->currentItem()); });
    connect(results_, &QListWidget::itemActivated, this, &QuickActionsDialog::trigger);
    refreshActions();
}

void QuickActionsDialog::refreshActions() { filterActions(); }

QLineEdit *QuickActionsDialog::searchEdit() const { return search_; }

QListWidget *QuickActionsDialog::resultsView() const { return results_; }

int QuickActionsDialog::visibleActionCount() const { return results_->count(); }

void QuickActionsDialog::showEvent(QShowEvent *event) {
    previousFocus_ = QApplication::focusWidget();
    QDialog::showEvent(event);
}

void QuickActionsDialog::hideEvent(QHideEvent *event) {
    QDialog::hideEvent(event);
    if (previousFocus_ && previousFocus_ != this)
        previousFocus_->setFocus(Qt::OtherFocusReason);
    previousFocus_.clear();
}

void QuickActionsDialog::filterActions() {
    if (!actionRoot_)
        return;
    const auto query = search_->text().trimmed();
    results_->clear();
    QSet<QAction *> seen;
    const auto actions = actionRoot_->findChildren<QAction *>();
    for (auto *action : actions) {
        if (!action || action == openAction_ || action->isSeparator() || seen.contains(action))
            continue;
        seen.insert(action);
        const auto text = action->text().remove(QLatin1Char('&'));
        const auto shortcut = action->shortcut().toString(QKeySequence::NativeText);
        if (!query.isEmpty() && !text.contains(query, Qt::CaseInsensitive) &&
            !shortcut.contains(query, Qt::CaseInsensitive))
            continue;
        auto *item = new QListWidgetItem(text, results_);
        item->setData(Qt::UserRole, QVariant::fromValue(static_cast<QObject *>(action)));
        item->setFlags(action->isEnabled() ? item->flags() : item->flags() & ~Qt::ItemIsEnabled);
        if (!shortcut.isEmpty())
            item->setToolTip(shortcut);
    }
    if (results_->count() > 0)
        results_->setCurrentRow(0);
}

void QuickActionsDialog::trigger(QListWidgetItem *item) {
    if (!item)
        return;
    auto *action = qobject_cast<QAction *>(item->data(Qt::UserRole).value<QObject *>());
    if (!action || !action->isEnabled())
        return;
    hide();
    action->trigger();
}
