#pragma once

#include <QDialog>
#include <QHash>
#include <QKeySequence>
#include <QList>
#include <QMap>
#include <QObject>
#include <QStringList>
#include <memory>

class QAction;
class QKeySequenceEdit;
class QMenuBar;
class QPushButton;
class QSettings;
class QTreeWidget;
class QTreeWidgetItem;

// Captures the application actions that own a default shortcut and applies
// persistent per-action overrides stored in the "shortcuts" settings group.
// Keeping this outside the dialogs lets the reference dialog stay read-only
// while the editor works on a snapshot of the same actions.
class ShortcutSettings final : public QObject {
    Q_OBJECT
  public:
    explicit ShortcutSettings(QObject *root, const QString &settingsPath = {},
                              QObject *parent = nullptr);
    ~ShortcutSettings() override;

    [[nodiscard]] QList<QAction *> actions() const;
    [[nodiscard]] QKeySequence current(const QAction *action) const;
    [[nodiscard]] QKeySequence defaultShortcut(const QAction *action) const;
    [[nodiscard]] const QMap<QString, QKeySequence> &defaults() const;
    [[nodiscard]] QMap<QString, QKeySequence> current() const;
    [[nodiscard]] QMap<QString, QKeySequence> currentShortcuts() const;
    [[nodiscard]] bool isManaged(const QAction *action) const;
    [[nodiscard]] QStringList conflicts() const;

    bool setShortcut(QAction *action, const QKeySequence &sequence);
    void reset(QAction *action);
    void resetAll();
    QString save();

  private:
    [[nodiscard]] const QAction *sequenceHolder(const QAction *skip,
                                                const QKeySequence &sequence) const;
    void load();

    QObject *root_ = nullptr;
    std::unique_ptr<QSettings> settings_;
    QList<QAction *> actions_;
    QMap<QString, QKeySequence> defaults_;
    QMap<QString, QKeySequence> current_;
};

class ShortcutsDialog final : public QDialog {
    Q_OBJECT
  public:
    explicit ShortcutsDialog(QMenuBar *menuBar, const QList<QAction *> &extraActions = {},
                             QWidget *parent = nullptr);

  private:
    void populateTree();
    void openEditor();

    QMenuBar *menuBar_ = nullptr;
    QList<QAction *> extraActions_;
    QTreeWidget *tree_;
};

class ShortcutEditorDialog final : public QDialog {
    Q_OBJECT
  public:
    explicit ShortcutEditorDialog(ShortcutSettings *settings, QWidget *parent = nullptr);

  private:
    void loadSelected();
    void updateControls();
    void setPending(const QString &objectName, const QKeySequence &sequence);
    void refreshRow(const QString &objectName);
    void commit();
    [[nodiscard]] QKeySequence effectiveCurrent(const QAction *action) const;
    [[nodiscard]] bool hasConflicts(QString *message) const;

    ShortcutSettings *settings_ = nullptr;
    QTreeWidget *tree_ = nullptr;
    QKeySequenceEdit *keyEdit_ = nullptr;
    QPushButton *setButton_ = nullptr;
    QPushButton *clearButton_ = nullptr;
    QPushButton *restoreButton_ = nullptr;
    QHash<QString, QTreeWidgetItem *> rows_;
    QMap<QString, QKeySequence> pending_;
    QHash<QString, QAction *> actionsByName_;
};
