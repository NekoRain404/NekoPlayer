#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"
#include "ui/playlistitemdelegate.h"
#include "ui/playlistpane.h"

#include <QAction>
#include <QApplication>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QSignalSpy>
#include <QStyleOptionViewItem>
#include <QVBoxLayout>
#include <QtTest>

namespace {
QString literal(const char *text) { return QString::fromLatin1(text); }

struct PaneFixture {
    MpvController player{true, true};
    PlaylistController queue{player};
    QAction append;
    QAction previous;
    QAction next;
    QAction clear;
    QWidget host;
    PlaylistPane pane{queue, &append, &previous, &next, &clear};

    PaneFixture() {
        queue.model()->addFiles({literal("alpha.mkv"), literal("beta.mkv"), literal("gamma.mkv")});
        auto *layout = new QVBoxLayout(&host);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(&pane);
        host.resize(360, 480);
        host.show();
        QCoreApplication::processEvents();
    }

    [[nodiscard]] QLineEdit *search() const {
        return pane.findChild<QLineEdit *>(literal("playlistSearchEdit"));
    }
    [[nodiscard]] QListView *view() const { return pane.view(); }
    [[nodiscard]] QString countText() const {
        const auto *count = pane.findChild<QLabel *>(literal("playlistCount"));
        return count ? count->text() : QString{};
    }
    [[nodiscard]] QLabel *emptyHint() const {
        return pane.findChild<QLabel *>(literal("playlistEmptyHint"));
    }
    [[nodiscard]] QAction *removeAction() const {
        return pane.findChild<QAction *>(literal("removePlaylistItemAction"));
    }
};
} // namespace

class PlaylistPaneTest final : public QObject {
    Q_OBJECT
  private slots:
    void searchNarrowsRowsAndShowsFilteredCount() {
        PaneFixture fixture;
        auto *search = fixture.search();
        QVERIFY(search);
        QCOMPARE(search->placeholderText(), QStringLiteral("搜索播放列表"));
        QCOMPARE(search->accessibleName(), QStringLiteral("搜索播放列表"));
        QVERIFY(search->isClearButtonEnabled());
        QVERIFY(fixture.view()->model() == fixture.queue.model());
        QCOMPARE(fixture.countText(), QStringLiteral("3 项"));

        search->setText(literal("beta"));
        QCOMPARE(fixture.view()->model()->rowCount(), 1);
        QCOMPARE(fixture.view()->model()->index(0, 0).data().toString(), literal("beta.mkv"));
        QCOMPARE(fixture.countText(), QStringLiteral("1 / 3 项"));

        search->setText(literal("BETA"));
        QCOMPARE(fixture.view()->model()->rowCount(), 1);
        QCOMPARE(fixture.countText(), QStringLiteral("1 / 3 项"));

        search->setText(literal("missing"));
        QCOMPARE(fixture.view()->model()->rowCount(), 0);
        QCOMPARE(fixture.countText(), QStringLiteral("0 / 3 项"));
    }

    void activationMapsFilteredRowToSourceRow() {
        PaneFixture fixture;
        fixture.search()->setText(literal("beta"));
        auto *view = fixture.view();
        QCOMPARE(view->model()->rowCount(), 1);
        const auto proxyIndex = view->model()->index(0, 0);
        QVERIFY(proxyIndex.isValid());
        QSignalSpy plays(&fixture.pane, &PlaylistPane::playRequested);

        QVERIFY(QMetaObject::invokeMethod(view, "activated", Qt::DirectConnection,
                                          Q_ARG(QModelIndex, proxyIndex)));
        QCOMPARE(plays.count(), 1);
        QCOMPARE(plays.at(0).at(0).toInt(), 1);
    }

    void singleClickMapsFilteredRowToSourceRow() {
        PaneFixture fixture;
        fixture.pane.setSingleClickActivation(true);
        fixture.search()->setText(literal("beta"));
        auto *view = fixture.view();
        const auto proxyIndex = view->model()->index(0, 0);
        QVERIFY(proxyIndex.isValid());
        QSignalSpy plays(&fixture.pane, &PlaylistPane::playRequested);

        QVERIFY(QMetaObject::invokeMethod(view, "clicked", Qt::DirectConnection,
                                          Q_ARG(QModelIndex, proxyIndex)));
        QTRY_COMPARE_WITH_TIMEOUT(plays.count(), 1, 2000);
        QCOMPARE(plays.at(0).at(0).toInt(), 1);
    }

    void editingActionsDisableWhileFilteringAndRestoreAfterClearing() {
        PaneFixture fixture;
        fixture.queue.model()->setCurrentIndex(0);
        const auto actions = fixture.pane.editingActions();
        QCOMPARE(actions.size(), 3);
        QVERIFY(actions.at(0)->isEnabled());
        QVERIFY(!actions.at(1)->isEnabled());
        QVERIFY(actions.at(2)->isEnabled());

        fixture.search()->setText(literal("beta"));
        for (auto *action : actions)
            QVERIFY(!action->isEnabled());

        fixture.search()->clear();
        QVERIFY(actions.at(0)->isEnabled());
        QVERIFY(!actions.at(1)->isEnabled());
        QVERIFY(actions.at(2)->isEnabled());
    }

    void multiSelectionEnablesRemoveAndDisablesMoveActions() {
        PaneFixture fixture;
        fixture.queue.model()->setCurrentIndex(1);
        auto *view = fixture.view();
        auto *selection = view->selectionModel();
        QVERIFY(selection);
        QCOMPARE(selection->selectedRows().size(), 1);

        const auto actions = fixture.pane.editingActions();
        QVERIFY(actions.at(0)->isEnabled());
        QVERIFY(actions.at(1)->isEnabled());
        QVERIFY(actions.at(2)->isEnabled());

        // Extending the selection does not move the current index, so the
        // action state must also follow selectionChanged.
        selection->select(view->model()->index(0, 0),
                          QItemSelectionModel::Select | QItemSelectionModel::Rows);
        QCOMPARE(selection->selectedRows().size(), 2);
        QCOMPARE(view->currentIndex().row(), 1);
        QVERIFY(actions.at(0)->isEnabled());
        QVERIFY(!actions.at(1)->isEnabled());
        QVERIFY(!actions.at(2)->isEnabled());

        selection->clearSelection();
        selection->select(view->model()->index(2, 0),
                          QItemSelectionModel::Select | QItemSelectionModel::Rows);
        QCOMPARE(selection->selectedRows().size(), 1);
        QVERIFY(actions.at(0)->isEnabled());
        QVERIFY(actions.at(1)->isEnabled());
        QVERIFY(!actions.at(2)->isEnabled());
    }

    void bulkRemoveSelectedRowsLeavesCurrentEntry() {
        PaneFixture fixture;
        fixture.queue.model()->setCurrentIndex(1);
        auto *view = fixture.view();
        auto *selection = view->selectionModel();
        selection->clearSelection();
        selection->select(view->model()->index(0, 0),
                          QItemSelectionModel::Select | QItemSelectionModel::Rows);
        selection->select(view->model()->index(2, 0),
                          QItemSelectionModel::Select | QItemSelectionModel::Rows);
        QCOMPARE(selection->selectedRows().size(), 2);

        auto *remove = fixture.removeAction();
        QVERIFY(remove);
        remove->trigger();

        QCOMPARE(fixture.queue.model()->rowCount(), 1);
        QCOMPARE(fixture.queue.model()->currentIndex(), 0);
        QCOMPARE(fixture.queue.model()->index(0, 0).data().toString(), literal("beta.mkv"));
        QCOMPARE(fixture.countText(), QStringLiteral("1 项"));
    }

    void removingCurrentSelectedRowEmptiesQueue() {
        PaneFixture fixture;
        fixture.queue.model()->setCurrentIndex(1);
        auto *view = fixture.view();
        view->selectAll();
        QCOMPARE(view->selectionModel()->selectedRows().size(), 3);

        auto *remove = fixture.removeAction();
        QVERIFY(remove);
        remove->trigger();

        QCOMPARE(fixture.queue.model()->rowCount(), 0);
        QCOMPARE(fixture.queue.model()->currentIndex(), -1);
        QCOMPARE(fixture.countText(), QStringLiteral("0 项"));
        auto *hint = fixture.emptyHint();
        QVERIFY(hint);
        QVERIFY(!hint->isHidden());
        QCOMPARE(hint->text(), QStringLiteral("播放列表为空\n打开文件或文件夹，或拖入媒体"));
    }

    void filteredMultiSelectionMapsSafelyAndKeepsEditingDisabled() {
        PaneFixture fixture;
        fixture.queue.model()->setCurrentIndex(0);
        fixture.search()->setText(literal("a"));
        auto *view = fixture.view();
        QVERIFY(view->model() != fixture.queue.model());
        QCOMPARE(view->model()->rowCount(), 3);

        auto *selection = view->selectionModel();
        selection->clearSelection();
        selection->select(view->model()->index(0, 0),
                          QItemSelectionModel::Select | QItemSelectionModel::Rows);
        selection->select(view->model()->index(2, 0),
                          QItemSelectionModel::Select | QItemSelectionModel::Rows);
        QCOMPARE(selection->selectedRows().size(), 2);
        for (auto *action : fixture.pane.editingActions())
            QVERIFY(!action->isEnabled());

        fixture.search()->clear();
        QVERIFY(view->model() == fixture.queue.model());
        QCOMPARE(fixture.queue.model()->rowCount(), 3);
        selection = view->selectionModel();
        QCOMPARE(selection->selectedRows().size(), 1);
        QCOMPARE(selection->selectedRows().first().row(), 0);
    }

    void modifiedClicksDoNotSchedulePlayback() {
        PaneFixture fixture;
        fixture.pane.setSingleClickActivation(true);
        fixture.queue.model()->setCurrentIndex(0);
        auto *view = fixture.view();
        auto *selection = view->selectionModel();
        const auto first = view->model()->index(0, 0);
        const auto second = view->model()->index(1, 0);
        const auto third = view->model()->index(2, 0);
        const auto firstPoint = view->visualRect(first).center();
        const auto secondPoint = view->visualRect(second).center();
        const auto thirdPoint = view->visualRect(third).center();
        QCOMPARE(view->indexAt(firstPoint), first);
        QCOMPARE(view->indexAt(secondPoint), second);
        QCOMPARE(view->indexAt(thirdPoint), third);

        QSignalSpy plays(&fixture.pane, &PlaylistPane::playRequested);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::ControlModifier, secondPoint, 10);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::ShiftModifier, thirdPoint, 10);
        QTest::qWait(QApplication::doubleClickInterval() + 100);
        QCOMPARE(plays.count(), 0);
        QCOMPARE(selection->selectedRows().size(), 3);

        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, firstPoint, 10);
        QTRY_COMPARE_WITH_TIMEOUT(plays.count(), 1, 2000);
        QCOMPARE(plays.at(0).at(0).toInt(), 0);
    }

    void emptyHintTracksEmptyQueueAndFilters() {
        PaneFixture fixture;
        auto *hint = fixture.emptyHint();
        QVERIFY(hint);
        QCOMPARE(hint->objectName(), QStringLiteral("playlistEmptyHint"));
        QVERIFY(hint->wordWrap());
        QCOMPARE(hint->alignment(), Qt::Alignment(Qt::AlignCenter));
        QCOMPARE(hint->focusPolicy(), Qt::NoFocus);
        QVERIFY(!hint->accessibleName().isEmpty());
        QVERIFY(hint->isHidden());

        fixture.search()->setText(literal("zzz"));
        QVERIFY(!hint->isHidden());
        QCOMPARE(hint->text(), QStringLiteral("没有匹配项"));

        fixture.search()->clear();
        QVERIFY(hint->isHidden());

        fixture.queue.clear();
        QVERIFY(!hint->isHidden());
        QCOMPARE(hint->text(), QStringLiteral("播放列表为空\n打开文件或文件夹，或拖入媒体"));

        fixture.queue.model()->addFiles({literal("delta.mkv")});
        QVERIFY(hint->isHidden());

        fixture.queue.clear();
        QVERIFY(!hint->isHidden());
        QCOMPARE(hint->text(), QStringLiteral("播放列表为空\n打开文件或文件夹，或拖入媒体"));
    }

    void emptyHintHidesWhenFilteredRowsAppear() {
        PaneFixture fixture;
        auto *hint = fixture.emptyHint();
        fixture.search()->setText(literal("zzz"));
        QVERIFY(!hint->isHidden());

        fixture.queue.model()->addFiles({literal("zzz.mkv")});
        QVERIFY(hint->isHidden());
        QCOMPARE(fixture.view()->model()->rowCount(), 1);
        QCOMPARE(fixture.countText(), QStringLiteral("1 / 4 项"));
    }

    void playbackEntryChangeClearsStaleSelection() {
        PaneFixture fixture;
        fixture.queue.model()->setCurrentIndex(1);
        auto *view = fixture.view();
        auto *selection = view->selectionModel();
        selection->select(view->model()->index(0, 0),
                          QItemSelectionModel::Select | QItemSelectionModel::Rows);
        selection->select(view->model()->index(2, 0),
                          QItemSelectionModel::Select | QItemSelectionModel::Rows);
        QCOMPARE(selection->selectedRows().size(), 3);

        fixture.queue.model()->setCurrentIndex(2);
        QCOMPARE(selection->selectedRows().size(), 1);
        QVERIFY(selection->isSelected(view->model()->index(2, 0)));
        QCOMPARE(view->currentIndex().row(), 2);
    }

    void clearingRestoresFullQueueDragDropAndCurrentRow() {
        PaneFixture fixture;
        fixture.queue.model()->setCurrentIndex(1);
        QCOMPARE(fixture.view()->currentIndex().row(), 1);
        auto *search = fixture.search();

        search->setText(literal("beta"));
        QCOMPARE(fixture.view()->dragDropMode(), QAbstractItemView::NoDragDrop);
        QCOMPARE(fixture.view()->model()->rowCount(), 1);
        QCOMPARE(fixture.view()->currentIndex().data().toString(), literal("beta.mkv"));

        search->clear();
        QVERIFY(fixture.view()->model() == fixture.queue.model());
        QCOMPARE(fixture.view()->dragDropMode(), QAbstractItemView::InternalMove);
        QCOMPARE(fixture.view()->model()->rowCount(), 3);
        QCOMPARE(fixture.view()->currentIndex().row(), 1);
        QCOMPARE(fixture.countText(), QStringLiteral("3 项"));
    }

    void escapeClearsSearchAndRestoresSourceModel() {
        PaneFixture fixture;
        auto *search = fixture.search();
        fixture.host.activateWindow();
        search->setFocus();
        QCoreApplication::processEvents();
        search->setText(literal("beta"));
        QCOMPARE(fixture.view()->model()->rowCount(), 1);

        QTest::keyClick(search, Qt::Key_Escape);
        QVERIFY(search->text().isEmpty());
        QVERIFY(fixture.view()->model() == fixture.queue.model());
        QCOMPARE(fixture.view()->model()->rowCount(), 3);
    }

    void hiddenCurrentRowClearsViewSelection() {
        PaneFixture fixture;
        fixture.queue.model()->setCurrentIndex(2);
        QCOMPARE(fixture.view()->currentIndex().row(), 2);

        fixture.search()->setText(literal("alpha"));
        QCOMPARE(fixture.view()->model()->rowCount(), 1);
        QVERIFY(!fixture.view()->currentIndex().isValid());

        fixture.search()->clear();
        QVERIFY(fixture.view()->model() == fixture.queue.model());
        QCOMPARE(fixture.view()->currentIndex().row(), 2);
    }

    void currentRowChangeWhileFilteringSelectsMappedProxyRow() {
        PaneFixture fixture;
        fixture.queue.model()->setCurrentIndex(0);
        fixture.search()->setText(literal("a"));
        auto *view = fixture.view();
        QVERIFY(view->model() != fixture.queue.model());
        QCOMPARE(view->model()->rowCount(), 3);
        QCOMPARE(view->currentIndex().row(), 0);

        fixture.queue.model()->setCurrentIndex(2);
        QCOMPARE(view->currentIndex().row(), 2);
        QCOMPARE(view->currentIndex().data().toString(), literal("gamma.mkv"));
        QCOMPARE(view->currentIndex().data(PlaylistModel::CurrentRole).toBool(), true);
        QVERIFY(view->currentIndex().model() == view->model());
    }

    void findShortcutFocusesAndSelectsSearch() {
        PaneFixture fixture;
        auto *search = fixture.search();
        auto *view = fixture.view();
        search->setText(literal("beta"));
        fixture.host.activateWindow();
        view->setFocus();
        QCoreApplication::processEvents();

        QTest::keyClick(view, Qt::Key_F, Qt::ControlModifier);
        QVERIFY(search->hasFocus());
        QCOMPARE(search->selectedText(), literal("beta"));
    }

    void shortcutActionsFocusAndClearSearch() {
        PaneFixture fixture;
        auto *search = fixture.search();
        const auto actions = fixture.pane.shortcutActions();
        QCOMPARE(actions.size(), 2);

        auto *focusAction = fixture.pane.findChild<QAction *>(literal("focusPlaylistSearchAction"));
        auto *clearAction = fixture.pane.findChild<QAction *>(literal("clearPlaylistSearchAction"));
        QVERIFY(focusAction);
        QVERIFY(clearAction);
        QCOMPARE(focusAction->text(), QStringLiteral("搜索播放列表"));
        QCOMPARE(focusAction->shortcut(), QKeySequence(QKeySequence::Find));
        QCOMPARE(focusAction->shortcutContext(), Qt::WidgetWithChildrenShortcut);
        QCOMPARE(clearAction->text(), QStringLiteral("清除播放列表搜索"));
        QCOMPARE(clearAction->shortcut(), QKeySequence(Qt::Key_Escape));
        QCOMPARE(clearAction->shortcutContext(), Qt::WidgetWithChildrenShortcut);
        QVERIFY(fixture.pane.actions().contains(focusAction));
        QVERIFY(fixture.pane.actions().contains(clearAction));

        search->setText(literal("beta"));
        fixture.host.activateWindow();
        fixture.view()->setFocus();
        QCoreApplication::processEvents();
        focusAction->trigger();
        QVERIFY(search->hasFocus());
        QCOMPARE(search->selectedText(), literal("beta"));

        clearAction->trigger();
        QVERIFY(search->text().isEmpty());
        QVERIFY(fixture.view()->model() == fixture.queue.model());
        QCOMPARE(fixture.view()->model()->rowCount(), 3);
    }

    void modelResetClearsSearchAndShowsReplacementQueue() {
        PaneFixture fixture;
        fixture.search()->setText(literal("beta"));
        QCOMPARE(fixture.view()->model()->rowCount(), 1);

        fixture.queue.restoreFiles({literal("delta.mkv"), literal("epsilon.mkv")}, 0, 0);

        QVERIFY(fixture.search()->text().isEmpty());
        QVERIFY(fixture.view()->model() == fixture.queue.model());
        QCOMPARE(fixture.view()->model()->rowCount(), 2);
        QCOMPARE(fixture.countText(), QStringLiteral("2 项"));
    }

    void playingRowStaysBoldWhenVisible() {
        PaneFixture fixture;
        fixture.queue.model()->setCurrentIndex(1);
        fixture.search()->setText(literal("beta"));
        auto *view = fixture.view();
        QCOMPARE(view->model()->rowCount(), 1);
        const auto index = view->model()->index(0, 0);
        QCOMPARE(index.data(PlaylistModel::CurrentRole).toBool(), true);

        auto *delegate = qobject_cast<PlaylistItemDelegate *>(view->itemDelegate());
        QVERIFY(delegate);
        QStyleOptionViewItem option;
        delegate->initStyleOption(&option, index);
        QVERIFY(option.font.bold());
        QVERIFY(option.text.startsWith(QStringLiteral("▶ ")));
    }
};

QTEST_MAIN(PlaylistPaneTest)
#include "playlist_pane_test.moc"
