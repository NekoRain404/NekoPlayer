#include "ui/dialogs/shortcutsdialog.h"

#include <QAbstractItemView>
#include <QAction>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>
#include <utility>

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

QString displaySequence(const QKeySequence &sequence) {
    return sequence.isEmpty() ? QCoreApplication::translate("ShortcutsDialog", "无")
                              : sequence.toString(QKeySequence::NativeText);
}

QString scopeText(Qt::ShortcutContext context) {
    if (context == Qt::WidgetShortcut)
        return QCoreApplication::translate("ShortcutsDialog", "列表聚焦");
    if (context == Qt::WidgetWithChildrenShortcut)
        return QCoreApplication::translate("ShortcutsDialog", "当前面板");
    return QCoreApplication::translate("ShortcutsDialog", "全局");
}

void configureShortcutTree(QTreeWidget *tree, bool decorated) {
    tree->setColumnCount(3);
    tree->setHeaderLabels({QCoreApplication::translate("ShortcutsDialog", "操作"),
                           QCoreApplication::translate("ShortcutsDialog", "快捷键"),
                           QCoreApplication::translate("ShortcutsDialog", "范围")});
    tree->setRootIsDecorated(decorated);
    tree->setAlternatingRowColors(true);
    tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree->header()->setStretchLastSection(false);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
}

ShortcutSettings *shortcutSettingsFromApplication() {
    auto *application = QCoreApplication::instance();
    return application ? qobject_cast<ShortcutSettings *>(
                             application->property("shortcutSettings").value<QObject *>())
                       : nullptr;
}
} // namespace

ShortcutSettings::ShortcutSettings(QObject *root, const QString &settingsPath, QObject *parent)
    : QObject(parent), root_(root) {
    for (auto *action : root_ ? root_->findChildren<QAction *>() : QList<QAction *>{}) {
        if (!action || action->isSeparator())
            continue;
        const auto objectName = action->objectName();
        const auto shortcut = action->shortcut();
        if (objectName.isEmpty() || shortcut.isEmpty() || defaults_.contains(objectName))
            continue;
        actions_.append(action);
        current_.insert(objectName, shortcut);
        defaults_.insert(objectName, shortcut);
    }
    std::sort(actions_.begin(), actions_.end(), [](const QAction *left, const QAction *right) {
        const auto textOrder = QString::compare(
            withoutMnemonics(left->text()), withoutMnemonics(right->text()), Qt::CaseInsensitive);
        if (textOrder != 0)
            return textOrder < 0;
        return QString::compare(left->objectName(), right->objectName(), Qt::CaseInsensitive) < 0;
    });

    const bool explicitPath = !settingsPath.isEmpty();
    const bool configurationEnabled = qApp && qApp->property("configurationEnabled").toBool();
    if (explicitPath || configurationEnabled) {
        if (explicitPath)
            settings_ = std::make_unique<QSettings>(settingsPath, QSettings::IniFormat);
        else
            settings_ = std::make_unique<QSettings>();
        load();
    }
}

ShortcutSettings::~ShortcutSettings() = default;

QList<QAction *> ShortcutSettings::actions() const { return actions_; }

QKeySequence ShortcutSettings::current(const QAction *action) const {
    return isManaged(action) ? current_.value(action->objectName()) : QKeySequence{};
}

QKeySequence ShortcutSettings::defaultShortcut(const QAction *action) const {
    return isManaged(action) ? defaults_.value(action->objectName()) : QKeySequence{};
}

const QMap<QString, QKeySequence> &ShortcutSettings::defaults() const { return defaults_; }

QMap<QString, QKeySequence> ShortcutSettings::current() const { return current_; }

QMap<QString, QKeySequence> ShortcutSettings::currentShortcuts() const { return current_; }

bool ShortcutSettings::isManaged(const QAction *action) const {
    return action && defaults_.contains(action->objectName());
}

const QAction *ShortcutSettings::sequenceHolder(const QAction *skip,
                                                const QKeySequence &sequence) const {
    if (!skip || sequence.isEmpty())
        return nullptr;
    const auto context = skip->shortcutContext();
    for (auto *action : actions_) {
        if (action != skip && action->shortcutContext() == context &&
            current_.value(action->objectName()) == sequence)
            return action;
    }
    return nullptr;
}

void ShortcutSettings::load() {
    settings_->beginGroup(QStringLiteral("shortcuts"));
    for (auto entry = defaults_.cbegin(); entry != defaults_.cend(); ++entry) {
        if (!settings_->contains(entry.key()))
            continue;
        const auto text = settings_->value(entry.key()).toString();
        if (text.isEmpty())
            continue;
        const auto sequence = text == QLatin1String("none")
                                  ? QKeySequence{}
                                  : QKeySequence::fromString(text, QKeySequence::PortableText);
        if (text != QLatin1String("none") && sequence.isEmpty())
            continue;
        current_[entry.key()] = sequence;
        for (auto *action : actions_) {
            if (action->objectName() == entry.key())
                action->setShortcut(sequence);
        }
    }
    settings_->endGroup();
}

bool ShortcutSettings::setShortcut(QAction *action, const QKeySequence &sequence) {
    if (!isManaged(action) || sequenceHolder(action, sequence))
        return false;
    current_[action->objectName()] = sequence;
    action->setShortcut(sequence);
    return true;
}

void ShortcutSettings::reset(QAction *action) {
    if (!isManaged(action))
        return;
    const auto sequence = defaults_.value(action->objectName());
    if (current_.value(action->objectName()) == sequence)
        return;
    current_[action->objectName()] = sequence;
    action->setShortcut(sequence);
}

void ShortcutSettings::resetAll() {
    for (auto *action : actions_)
        reset(action);
}

QStringList ShortcutSettings::conflicts() const {
    QStringList result;
    for (auto *action : actions_) {
        const auto *other = sequenceHolder(action, current_.value(action->objectName()));
        if (other) {
            result.append(
                QCoreApplication::translate("ShortcutSettings", "“%1”与“%2”冲突（%3）")
                    .arg(withoutMnemonics(action->text()), withoutMnemonics(other->text()),
                         current_.value(action->objectName()).toString(QKeySequence::NativeText)));
        }
    }
    result.removeDuplicates();
    return result;
}

QString ShortcutSettings::save() {
    if (!settings_)
        return {};
    settings_->beginGroup(QStringLiteral("shortcuts"));
    for (auto *action : actions_) {
        const auto objectName = action->objectName();
        const auto sequence = current_.value(objectName);
        if (sequence == defaults_.value(objectName))
            settings_->remove(objectName);
        else
            settings_->setValue(objectName, sequence.isEmpty()
                                                ? QStringLiteral("none")
                                                : sequence.toString(QKeySequence::PortableText));
    }
    settings_->endGroup();
    settings_->sync();
    return settings_->status() == QSettings::NoError
               ? QString{}
               : QCoreApplication::translate("ShortcutSettings", "无法保存快捷键设置");
}

ShortcutsDialog::ShortcutsDialog(QMenuBar *menuBar, const QList<QAction *> &extraActions,
                                 QWidget *parent)
    : QDialog(parent), menuBar_(menuBar), extraActions_(extraActions),
      tree_(new QTreeWidget(this)) {
    setObjectName(QStringLiteral("shortcutsDialog"));
    setWindowTitle(tr("快捷键"));
    setAttribute(Qt::WA_TranslucentBackground, true);
    tree_->setObjectName(QStringLiteral("shortcutsTree"));
    tree_->setSelectionMode(QAbstractItemView::NoSelection);
    configureShortcutTree(tree_, true);
    populateTree();

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(tree_);
    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch(1);
    if (shortcutSettingsFromApplication()) {
        auto *editButton = new QPushButton(tr("编辑快捷键…"), this);
        editButton->setObjectName(QStringLiteral("editShortcutsButton"));
        connect(editButton, &QPushButton::clicked, this, &ShortcutsDialog::openEditor);
        buttonRow->addWidget(editButton);
    }
    layout->addLayout(buttonRow);
    resize(620, 680);
}

void ShortcutsDialog::populateTree() {
    tree_->clear();
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
    if (menuBar_) {
        for (auto *action : menuBar_->actions()) {
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
    }
    auto *panelItem = new QTreeWidgetItem(tree_);
    panelItem->setText(0, tr("面板快捷键"));
    for (auto *action : extraActions_) {
        if (action)
            appendAction(panelItem, action);
    }
    if (panelItem->childCount() == 0)
        delete panelItem;
    tree_->expandAll();
}

void ShortcutsDialog::openEditor() {
    auto *settings = shortcutSettingsFromApplication();
    if (!settings)
        return;
    ShortcutEditorDialog editor(settings, this);
    if (editor.exec() == QDialog::Accepted)
        populateTree();
}

ShortcutEditorDialog::ShortcutEditorDialog(ShortcutSettings *settings, QWidget *parent)
    : QDialog(parent), settings_(settings), tree_(new QTreeWidget(this)),
      keyEdit_(new QKeySequenceEdit(this)), setButton_(new QPushButton(tr("设置"), this)),
      clearButton_(new QPushButton(tr("清除"), this)),
      restoreButton_(new QPushButton(tr("恢复默认"), this)) {
    setObjectName(QStringLiteral("shortcutEditorDialog"));
    setWindowTitle(tr("编辑快捷键"));
    tree_->setObjectName(QStringLiteral("shortcutEditorTree"));
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    configureShortcutTree(tree_, false);
    keyEdit_->setObjectName(QStringLiteral("shortcutKeyEdit"));
    setButton_->setObjectName(QStringLiteral("setShortcutButton"));
    clearButton_->setObjectName(QStringLiteral("clearShortcutButton"));
    restoreButton_->setObjectName(QStringLiteral("restoreDefaultShortcutButton"));

    for (auto *action : settings_->actions()) {
        const auto objectName = action->objectName();
        auto *item = new QTreeWidgetItem(tree_);
        item->setText(0, withoutMnemonics(action->text()));
        item->setData(0, Qt::UserRole, objectName);
        item->setText(2, scopeText(action->shortcutContext()));
        rows_.insert(objectName, item);
        actionsByName_.insert(objectName, action);
        refreshRow(objectName);
    }
    connect(tree_, &QTreeWidget::currentItemChanged, this, [this] { loadSelected(); });
    connect(setButton_, &QPushButton::clicked, this, [this] {
        if (auto *item = tree_->currentItem())
            setPending(item->data(0, Qt::UserRole).toString(), keyEdit_->keySequence());
    });
    connect(clearButton_, &QPushButton::clicked, this, [this] {
        if (auto *item = tree_->currentItem())
            setPending(item->data(0, Qt::UserRole).toString(), QKeySequence{});
    });
    connect(restoreButton_, &QPushButton::clicked, this, [this] {
        auto *item = tree_->currentItem();
        auto *action =
            item ? actionsByName_.value(item->data(0, Qt::UserRole).toString()) : nullptr;
        if (action)
            setPending(action->objectName(), settings_->defaultShortcut(action));
    });

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->setObjectName(QStringLiteral("shortcutEditorButtons"));
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("确定"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ShortcutEditorDialog::commit);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &ShortcutEditorDialog::reject);

    auto *keyRow = new QHBoxLayout;
    keyRow->addWidget(new QLabel(tr("快捷键："), this));
    keyRow->addWidget(keyEdit_, 1);
    keyRow->addWidget(setButton_);
    keyRow->addWidget(clearButton_);
    keyRow->addWidget(restoreButton_);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(tree_, 1);
    layout->addLayout(keyRow);
    layout->addWidget(buttonBox);
    if (tree_->topLevelItemCount() > 0)
        tree_->setCurrentItem(tree_->topLevelItem(0));
    loadSelected();
    resize(560, 520);
}

QKeySequence ShortcutEditorDialog::effectiveCurrent(const QAction *action) const {
    const auto objectName = action->objectName();
    return pending_.contains(objectName) ? pending_.value(objectName) : settings_->current(action);
}

void ShortcutEditorDialog::refreshRow(const QString &objectName) {
    auto *item = rows_.value(objectName);
    auto *action = actionsByName_.value(objectName);
    if (!item || !action)
        return;
    item->setText(1, displaySequence(effectiveCurrent(action)));
    const auto defaultText =
        tr("默认：%1").arg(displaySequence(settings_->defaultShortcut(action)));
    item->setToolTip(0, defaultText);
    item->setToolTip(1, defaultText);
}

void ShortcutEditorDialog::setPending(const QString &objectName, const QKeySequence &sequence) {
    if (!actionsByName_.contains(objectName))
        return;
    pending_.insert(objectName, sequence);
    refreshRow(objectName);
    if (auto *item = rows_.value(objectName))
        tree_->setCurrentItem(item);
    keyEdit_->setKeySequence(sequence);
    updateControls();
}

void ShortcutEditorDialog::loadSelected() {
    auto *item = tree_->currentItem();
    auto *action = item ? actionsByName_.value(item->data(0, Qt::UserRole).toString()) : nullptr;
    if (action)
        keyEdit_->setKeySequence(effectiveCurrent(action));
    else
        keyEdit_->clear();
    updateControls();
}

void ShortcutEditorDialog::updateControls() {
    auto *item = tree_->currentItem();
    auto *action = item ? actionsByName_.value(item->data(0, Qt::UserRole).toString()) : nullptr;
    keyEdit_->setEnabled(action);
    setButton_->setEnabled(action);
    clearButton_->setEnabled(action);
    restoreButton_->setEnabled(action &&
                               effectiveCurrent(action) != settings_->defaultShortcut(action));
}

bool ShortcutEditorDialog::hasConflicts(QString *message) const {
    QMap<QString, QAction *> seen;
    for (auto *action : settings_->actions()) {
        const auto sequence = effectiveCurrent(action);
        if (sequence.isEmpty())
            continue;
        const auto key = QString::number(static_cast<int>(action->shortcutContext())) +
                         sequence.toString(QKeySequence::PortableText);
        auto *other = seen.value(key);
        if (!other) {
            seen.insert(key, action);
            continue;
        }
        if (message) {
            *message = tr("“%1”与“%2”都使用了 %3。")
                           .arg(withoutMnemonics(action->text()), withoutMnemonics(other->text()),
                                sequence.toString(QKeySequence::NativeText));
        }
        return true;
    }
    return false;
}

void ShortcutEditorDialog::commit() {
    QString conflict;
    if (hasConflicts(&conflict)) {
        QMessageBox::warning(this, tr("快捷键冲突"), conflict);
        return;
    }
    QStringList changedActions;
    for (const auto &objectName : pending_.keys()) {
        auto *action = actionsByName_.value(objectName);
        if (action && pending_.value(objectName) != settings_->current(action))
            changedActions.append(objectName);
    }
    // Clear every edited action first so swapping two shortcuts cannot be
    // rejected by the transient overlap between the two assignments.
    for (const auto &objectName : std::as_const(changedActions)) {
        if (auto *action = actionsByName_.value(objectName))
            settings_->setShortcut(action, QKeySequence{});
    }
    for (const auto &objectName : std::as_const(changedActions)) {
        auto *action = actionsByName_.value(objectName);
        if (action && !settings_->setShortcut(action, pending_.value(objectName))) {
            QMessageBox::warning(
                this, tr("快捷键冲突"),
                tr("无法应用“%1”的快捷键。").arg(withoutMnemonics(action->text())));
            return;
        }
    }
    if (const auto error = settings_->save(); !error.isEmpty())
        QMessageBox::warning(this, tr("保存失败"), error);
    accept();
}
