#include "ui/mainwindow.h"
#include "ui/shortcutsdialog.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QDialogButtonBox>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariant>
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

struct ActionHost {
    QWidget root;
    QAction open{literal("Open"), &root};
    QAction save{literal("Save"), &root};
    QAction panel{literal("Panel"), &root};

    ActionHost() {
        open.setObjectName(QStringLiteral("openAction"));
        open.setShortcut(QKeySequence::Open);
        save.setObjectName(QStringLiteral("saveAction"));
        save.setShortcut(QKeySequence::Save);
        panel.setObjectName(QStringLiteral("panelAction"));
        panel.setShortcut(QKeySequence(Qt::Key_Escape));
        panel.setShortcutContext(Qt::WidgetWithChildrenShortcut);
        root.addAction(&open);
        root.addAction(&save);
        root.addAction(&panel);
    }
};

QString settingsFile(const QTemporaryDir &directory) {
    return directory.filePath(QStringLiteral("shortcuts.ini"));
}

class ApplicationPropertyGuard {
  public:
    explicit ApplicationPropertyGuard(const char *name)
        : name_(name), previous_(qApp->property(name)) {}
    ~ApplicationPropertyGuard() { qApp->setProperty(name_, previous_); }

  private:
    const char *name_;
    QVariant previous_;
};

class ApplicationIdentityGuard {
  public:
    ApplicationIdentityGuard(const QString &organization, const QString &application)
        : organization_(QCoreApplication::organizationName()),
          application_(QCoreApplication::applicationName()) {
        QCoreApplication::setOrganizationName(organization);
        QCoreApplication::setApplicationName(application);
    }
    ~ApplicationIdentityGuard() {
        QCoreApplication::setOrganizationName(organization_);
        QCoreApplication::setApplicationName(application_);
    }

  private:
    QString organization_;
    QString application_;
};

class SettingsPathGuard {
  public:
    explicit SettingsPathGuard(const QString &directory)
        : previousFormat_(QSettings::defaultFormat()) {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory);
    }
    ~SettingsPathGuard() {
        QSettings::setDefaultFormat(previousFormat_);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, QString());
    }

  private:
    QSettings::Format previousFormat_;
};
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

        ShortcutsDialog dialog(&menuBar,
                               {&menuAction, nullptr, &focusAction, &clearAction, &noShortcut});
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

    void shortcutSettingsCaptureDefaultsAndApplyStoredOverrides() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QKeySequence custom(Qt::CTRL | Qt::Key_O);
        {
            QSettings store(settingsFile(directory), QSettings::IniFormat);
            store.beginGroup(QStringLiteral("shortcuts"));
            store.setValue(QStringLiteral("openAction"),
                           custom.toString(QKeySequence::PortableText));
            store.setValue(QStringLiteral("saveAction"), QStringLiteral("none"));
        }
        ActionHost host;
        ShortcutSettings settings(&host.root, settingsFile(directory));
        QCOMPARE(settings.actions().size(), 3);
        QCOMPARE(settings.actions().at(0), &host.open);
        QCOMPARE(settings.actions().at(1), &host.panel);
        QCOMPARE(settings.actions().at(2), &host.save);
        QCOMPARE(settings.defaults().value(QStringLiteral("openAction")),
                 QKeySequence(QKeySequence::Open));
        QCOMPARE(settings.current(&host.open), custom);
        QCOMPARE(host.open.shortcut(), custom);
        QVERIFY(settings.current(&host.save).isEmpty());
        QVERIFY(host.save.shortcut().isEmpty());
    }

    void shortcutSettingsRejectConflictsOnlyWithinContext() {
        QTemporaryDir directory;
        ActionHost host;
        ShortcutSettings settings(&host.root, settingsFile(directory));
        const QKeySequence custom(Qt::CTRL | Qt::Key_R);
        QVERIFY(settings.setShortcut(&host.open, custom));
        QVERIFY(!settings.setShortcut(&host.save, custom));
        QCOMPARE(settings.current(&host.save), QKeySequence(QKeySequence::Save));
        QVERIFY(settings.setShortcut(&host.panel, custom));
        QVERIFY(settings.conflicts().isEmpty());
        QVERIFY(settings.setShortcut(&host.open, QKeySequence{}));
        QVERIFY(settings.setShortcut(&host.save, QKeySequence{}));
        QVERIFY(settings.setShortcut(&host.save, custom));
        settings.reset(&host.save);
        QCOMPARE(host.save.shortcut(), QKeySequence(QKeySequence::Save));
        QVERIFY(settings.setShortcut(&host.open, custom));
        settings.resetAll();
        QCOMPARE(host.open.shortcut(), QKeySequence(QKeySequence::Open));
        QCOMPARE(host.save.shortcut(), QKeySequence(QKeySequence::Save));
        QCOMPARE(host.panel.shortcut(), QKeySequence(Qt::Key_Escape));
    }

    void shortcutSettingsReportStoredConflicts() {
        QTemporaryDir directory;
        const QKeySequence custom(Qt::CTRL | Qt::Key_R);
        {
            QSettings store(settingsFile(directory), QSettings::IniFormat);
            store.beginGroup(QStringLiteral("shortcuts"));
            store.setValue(QStringLiteral("openAction"),
                           custom.toString(QKeySequence::PortableText));
            store.setValue(QStringLiteral("saveAction"),
                           custom.toString(QKeySequence::PortableText));
        }
        ActionHost host;
        ShortcutSettings settings(&host.root, settingsFile(directory));
        QCOMPARE(host.open.shortcut(), custom);
        QCOMPARE(host.save.shortcut(), custom);
        QVERIFY(!settings.conflicts().isEmpty());
    }

    void shortcutSettingsPersistOverridesAndClears() {
        QTemporaryDir directory;
        const auto path = settingsFile(directory);
        const QKeySequence custom(Qt::CTRL | Qt::SHIFT | Qt::Key_O);
        {
            ActionHost host;
            ShortcutSettings settings(&host.root, path);
            QVERIFY(settings.setShortcut(&host.open, custom));
            QVERIFY(settings.setShortcut(&host.save, QKeySequence{}));
            QCOMPARE(settings.save(), QString());
        }
        ActionHost host;
        ShortcutSettings reloaded(&host.root, path);
        QCOMPARE(reloaded.current(&host.open), custom);
        QCOMPARE(reloaded.current(&host.save), QKeySequence{});
        QCOMPARE(reloaded.current(&host.panel), QKeySequence(Qt::Key_Escape));
        reloaded.reset(&host.open);
        QCOMPARE(reloaded.save(), QString());
        ShortcutSettings again(&host.root, path);
        QCOMPARE(again.current(&host.open), QKeySequence(QKeySequence::Open));
    }

    void shortcutEditorAppliesPendingEditsOnAccept() {
        QTemporaryDir directory;
        ActionHost host;
        ShortcutSettings settings(&host.root, settingsFile(directory));
        ShortcutEditorDialog editor(&settings);
        auto *tree = editor.findChild<QTreeWidget *>(QStringLiteral("shortcutEditorTree"));
        auto *keyEdit = editor.findChild<QKeySequenceEdit *>(QStringLiteral("shortcutKeyEdit"));
        auto *setButton = editor.findChild<QPushButton *>(QStringLiteral("setShortcutButton"));
        auto *restoreButton =
            editor.findChild<QPushButton *>(QStringLiteral("restoreDefaultShortcutButton"));
        auto *buttons =
            editor.findChild<QDialogButtonBox *>(QStringLiteral("shortcutEditorButtons"));
        QVERIFY(tree && keyEdit && setButton && restoreButton && buttons);
        QCOMPARE(tree->topLevelItemCount(), 3);
        QTreeWidgetItem *saveRow = nullptr;
        for (int row = 0; row < tree->topLevelItemCount(); ++row) {
            auto *item = tree->topLevelItem(row);
            if (item->data(0, Qt::UserRole).toString() == QLatin1String("saveAction"))
                saveRow = item;
        }
        QVERIFY(saveRow);
        tree->setCurrentItem(saveRow);
        QVERIFY(!restoreButton->isEnabled());
        const QKeySequence custom(Qt::CTRL | Qt::Key_R);
        keyEdit->setKeySequence(custom);
        setButton->click();
        QCOMPARE(host.save.shortcut(), QKeySequence(QKeySequence::Save));
        QVERIFY(restoreButton->isEnabled());
        QCOMPARE(saveRow->text(1), native(custom));
        buttons->button(QDialogButtonBox::Ok)->click();
        QCOMPARE(editor.result(), int(QDialog::Accepted));
        QCOMPARE(host.save.shortcut(), custom);
        ShortcutSettings persisted(&host.root, settingsFile(directory));
        QCOMPARE(persisted.current(&host.save), custom);
    }

    void shortcutEditorCancelDiscardsPendingEdits() {
        QTemporaryDir directory;
        ActionHost host;
        ShortcutSettings settings(&host.root, settingsFile(directory));
        ShortcutEditorDialog editor(&settings);
        auto *tree = editor.findChild<QTreeWidget *>(QStringLiteral("shortcutEditorTree"));
        auto *keyEdit = editor.findChild<QKeySequenceEdit *>(QStringLiteral("shortcutKeyEdit"));
        auto *setButton = editor.findChild<QPushButton *>(QStringLiteral("setShortcutButton"));
        auto *buttons =
            editor.findChild<QDialogButtonBox *>(QStringLiteral("shortcutEditorButtons"));
        QVERIFY(tree && keyEdit && setButton && buttons);
        tree->setCurrentItem(tree->topLevelItem(0));
        keyEdit->setKeySequence(QKeySequence(Qt::CTRL | Qt::Key_R));
        setButton->click();
        buttons->button(QDialogButtonBox::Cancel)->click();
        QCOMPARE(editor.result(), int(QDialog::Rejected));
        QCOMPARE(host.open.shortcut(), QKeySequence(QKeySequence::Open));
        ShortcutSettings reloaded(&host.root, settingsFile(directory));
        QCOMPARE(reloaded.current(&host.open), QKeySequence(QKeySequence::Open));
    }

    void shortcutsDialogShowsEditorOnlyWhenSettingsArePublished() {
        QMenuBar menuBar;
        auto *file = menuBar.addMenu(literal("File"));
        auto *open = file->addAction(literal("Open"));
        open->setShortcut(QKeySequence::Open);
        QTemporaryDir directory;
        ActionHost host;
        ShortcutSettings settings(&host.root, settingsFile(directory));
        ApplicationPropertyGuard guard("shortcutSettings");
        qApp->setProperty("shortcutSettings", QVariant{});
        ShortcutsDialog readOnly(&menuBar);
        QVERIFY(!readOnly.findChild<QPushButton *>(QStringLiteral("editShortcutsButton")));
        qApp->setProperty("shortcutSettings",
                          QVariant::fromValue(static_cast<QObject *>(&settings)));
        ShortcutsDialog editable(&menuBar);
        auto *editButton = editable.findChild<QPushButton *>(QStringLiteral("editShortcutsButton"));
        QVERIFY(editButton);
        QCOMPARE(editButton->text(), QStringLiteral("编辑快捷键…"));
    }

    void mainWindowAppliesStoredOverrideAndPublishesSettings() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SettingsPathGuard pathGuard(directory.path());
        ApplicationIdentityGuard identityGuard(QStringLiteral("NekoPlayerTest"),
                                               QStringLiteral("shortcuts_dialog_test"));
        ApplicationPropertyGuard settingsGuard("shortcutSettings");
        ApplicationPropertyGuard configGuard("configurationEnabled");
        const QKeySequence custom(Qt::CTRL | Qt::SHIFT | Qt::Key_O);
        {
            QSettings store;
            store.beginGroup(QStringLiteral("shortcuts"));
            store.setValue(QStringLiteral("openAction"),
                           custom.toString(QKeySequence::PortableText));
        }
        qApp->setProperty("configurationEnabled", true);
        {
            MainWindow window(true);
            auto *settings = qobject_cast<ShortcutSettings *>(
                qApp->property("shortcutSettings").value<QObject *>());
            QVERIFY(settings);
            auto *open = window.findChild<QAction *>(QStringLiteral("openAction"));
            QVERIFY(open);
            QVERIFY(settings->isManaged(open));
            QCOMPARE(open->shortcut(), custom);
        }
    }
};

QTEST_MAIN(ShortcutsDialogTest)
#include "shortcuts_dialog_test.moc"
