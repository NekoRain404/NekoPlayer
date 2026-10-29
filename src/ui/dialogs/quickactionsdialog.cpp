#include "ui/dialogs/quickactionsdialog.h"
#include "ui/theme/appearancetokens.h"

#include <QAction>
#include <QApplication>
#include <QDialogButtonBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QMenu>
#include <QSet>
#include <QShowEvent>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>

namespace {
int fuzzyGapScore(const QString &text, const QString &query) {
    qsizetype next = 0;
    int gaps = 0;
    for (const auto character : query) {
        const auto found = text.indexOf(character, next, Qt::CaseInsensitive);
        if (found < 0)
            return -1;
        gaps += static_cast<int>(found - next);
        next = found + 1;
    }
    return gaps;
}

struct ActionMatch {
    QAction *action;
    int matchType;
    int gapScore;
    int sourceOrder;
};
} // namespace

QuickActionsDialog::QuickActionsDialog(QWidget *actionRoot, QWidget *parent)
    : QDialog(parent ? parent : actionRoot), actionRoot_(actionRoot),
      openAction_(new QAction(tr("快速操作…"), actionRoot)), search_(new QLineEdit(this)),
      results_(new QListWidget(this)), emptyState_(new QLabel(tr("没有匹配的操作"), this)) {
    const auto tokens = appearanceTokens(AppearancePreferences::Theme::Dark);
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
    search_->setMinimumHeight(tokens.controlHeight);
    search_->installEventFilter(this);
    results_->setObjectName(QStringLiteral("quickActionsResults"));
    results_->setAccessibleName(tr("操作结果"));
    results_->setUniformItemSizes(true);
    results_->setSpacing(tokens.spaceUnit);
    emptyState_->setObjectName(QStringLiteral("quickActionsEmptyState"));
    emptyState_->setAlignment(Qt::AlignCenter);
    emptyState_->setAccessibleName(tr("没有匹配的操作"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(tokens.spaceUnit * 4, tokens.spaceUnit * 4, tokens.spaceUnit * 4,
                               tokens.spaceUnit * 4);
    layout->setSpacing(tokens.spaceUnit * 3);
    layout->addWidget(search_);
    layout->addWidget(results_, 1);
    layout->addWidget(emptyState_, 1);
    connect(openAction_, &QAction::triggered, this, [this] {
        previousFocus_ = QApplication::focusWidget();
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
    if (!previousFocus_)
        previousFocus_ = QApplication::focusWidget();
    QDialog::showEvent(event);
}

void QuickActionsDialog::hideEvent(QHideEvent *event) {
    QDialog::hideEvent(event);
    const auto focusTarget = previousFocus_;
    previousFocus_.clear();
    if (focusTarget && focusTarget != this) {
        QTimer::singleShot(0, this, [focusTarget] {
            if (focusTarget && focusTarget->isVisible()) {
                focusTarget->window()->activateWindow();
                focusTarget->setFocus(Qt::OtherFocusReason);
            }
        });
    }
}

bool QuickActionsDialog::eventFilter(QObject *watched, QEvent *event) {
    if (watched == search_ && event->type() == QEvent::KeyPress && results_->count() > 0) {
        const auto key = static_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Down || key == Qt::Key_Up) {
            const int step = key == Qt::Key_Down ? 1 : -1;
            const int row = qBound(0, results_->currentRow() + step, results_->count() - 1);
            results_->setCurrentRow(row);
            results_->setFocus(Qt::TabFocusReason);
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void QuickActionsDialog::filterActions() {
    if (!actionRoot_)
        return;
    const auto query = search_->text().trimmed();
    results_->clear();
    QSet<QAction *> seen;
    QList<ActionMatch> matches;
    const auto actions = actionRoot_->findChildren<QAction *>();
    for (auto it = actionChangeConnections_.begin(); it != actionChangeConnections_.end();) {
        if (!actions.contains(it.key())) {
            QObject::disconnect(it.value());
            it = actionChangeConnections_.erase(it);
        } else {
            ++it;
        }
    }
    int sourceOrder = 0;
    for (auto *action : actions) {
        if (!action || action == openAction_ || action->isSeparator() || seen.contains(action))
            continue;
        if (action->menu())
            continue;
        seen.insert(action);
        if (!actionChangeConnections_.contains(action)) {
            actionChangeConnections_.insert(action, connect(action, &QAction::changed, this,
                                                            &QuickActionsDialog::filterActions));
        }
        const auto text = action->text().remove(QLatin1Char('&'));
        const auto shortcut = action->shortcut().toString(QKeySequence::NativeText);
        int matchType = 0;
        int gapScore = 0;
        if (!query.isEmpty() && !text.contains(query, Qt::CaseInsensitive) &&
            !shortcut.contains(query, Qt::CaseInsensitive)) {
            gapScore = fuzzyGapScore(text, query);
            if (gapScore < 0)
                gapScore = fuzzyGapScore(shortcut, query);
            if (gapScore < 0)
                continue;
            matchType = 1;
        }
        matches.push_back({action, matchType, gapScore, sourceOrder++});
    }
    std::stable_sort(matches.begin(), matches.end(),
                     [](const ActionMatch &left, const ActionMatch &right) {
                         if (left.matchType != right.matchType)
                             return left.matchType < right.matchType;
                         if (left.gapScore != right.gapScore)
                             return left.gapScore < right.gapScore;
                         return left.sourceOrder < right.sourceOrder;
                     });
    for (const auto &match : matches) {
        auto *action = match.action;
        if (!action)
            continue;
        const auto text = action->text().remove(QLatin1Char('&'));
        const auto shortcut = action->shortcut().toString(QKeySequence::NativeText);
        auto *item = new QListWidgetItem(text, results_);
        item->setIcon(action->icon());
        item->setData(Qt::UserRole, QVariant::fromValue(static_cast<QObject *>(action)));
        item->setFlags(action->isEnabled() ? item->flags() : item->flags() & ~Qt::ItemIsEnabled);
        QStringList contexts;
        for (auto *associated : action->associatedObjects()) {
            if (auto *menu = qobject_cast<QMenu *>(associated);
                menu && !menu->title().isEmpty() && !contexts.contains(menu->title()))
                contexts.push_back(menu->title());
        }
        QStringList details = contexts;
        if (!shortcut.isEmpty())
            details.push_back(shortcut);
        if (!details.isEmpty())
            item->setToolTip(details.join(QStringLiteral(" · ")));
        if (!contexts.isEmpty())
            item->setText(contexts.join(QStringLiteral(" · ")) + QStringLiteral("  ›  ") + text);
    }
    const bool hasResults = results_->count() > 0;
    results_->setVisible(hasResults);
    emptyState_->setVisible(!hasResults);
    if (hasResults)
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
