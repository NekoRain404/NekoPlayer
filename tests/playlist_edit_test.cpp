#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"
#include "session/sessionmanager.h"

#include <QAbstractItemModelTester>
#include <QFile>
#include <QMimeData>
#include <QPersistentModelIndex>
#include <QPixmap>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

class PlaylistEditTest final : public QObject {
    Q_OBJECT
  private slots:
    void initTestCase() {
        // Qt's model tester probes GUI-role metatypes even under QCoreApplication.
        // Register metadata only; no QPixmap or GUI application is constructed.
        qRegisterMetaType<QPixmap>();
    }

    void movingRowsPreservesDuplicateIdentity() {
        PlaylistModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.addFiles({"a.mkv", "b.mkv", "a.mkv", "c.mkv"});
        model.setCurrentIndex(2);
        QPersistentModelIndex current(model.index(2));
        QPersistentModelIndex first(model.index(0));
        QVERIFY(model.moveRow({}, 0, {}, 4));
        QCOMPARE(model.currentIndex(), 1);
        QCOMPARE(current.row(), 1);
        QCOMPARE(first.row(), 3);
        QCOMPARE(model.data(model.index(0)).toString(), QStringLiteral("b.mkv"));
        QVERIFY(model.moveRow({}, 1, {}, 0));
        QCOMPARE(model.currentIndex(), 0);
        QCOMPARE(current.row(), 0);
        QCOMPARE(first.row(), 3);
        QVERIFY(model.moveRow({}, 3, {}, 1));
        QCOMPARE(first.row(), 1);
        QCOMPARE(current.row(), 0);
        QVERIFY(!model.moveRow({}, -1, {}, 0));
        QVERIFY(!model.moveRow({}, 0, {}, 5));
        QVERIFY(!model.moveRow({}, 0, {}, 0));
        QVERIFY(!model.moveRow({}, 0, {}, 1));
        QVERIFY(!model.moveRows({}, 0, 2, {}, 4));
        QVERIFY(!model.moveRow(model.index(0), 0, {}, 3));
    }

    void removalNotifiesReplacementEvenAtSameIndex() {
        PlaylistModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.addFiles({"a.mkv", "b.mkv", "c.mkv"});
        model.setCurrentIndex(1);
        QSignalSpy changed(&model, &PlaylistModel::currentIndexChanged);
        QSignalSpy data(&model, &PlaylistModel::dataChanged);
        QVERIFY(model.removeAt(1));
        QCOMPARE(model.currentIndex(), 1);
        QCOMPARE(model.data(model.index(1)).toString(), QStringLiteral("c.mkv"));
        QVERIFY(model.data(model.index(1), PlaylistModel::CurrentRole).toBool());
        QCOMPARE(changed.count(), 1);
        QVERIFY(!data.isEmpty());
        QVERIFY(model.removeAt(1));
        QCOMPARE(model.currentIndex(), 0);
        QVERIFY(model.removeAt(0));
        QCOMPARE(model.currentIndex(), -1);
    }

    void internalDropsUseLiveIdentityAndRejectForeignData() {
        PlaylistModel model;
        model.addFiles({"a.mkv", "b.mkv", "c.mkv", "a.mkv"});
        model.setCurrentIndex(0);
        std::unique_ptr<QMimeData> drag(model.mimeData({model.index(3)}));
        QVERIFY(drag);
        QCOMPARE(model.supportedDropActions(), Qt::DropActions(Qt::MoveAction));
        QVERIFY(model.removeAt(1)); // The drag must follow its entry across concurrent edits.
        QVERIFY(model.dropMimeData(drag.get(), Qt::MoveAction, 0, 0, {}));
        QCOMPARE(model.currentIndex(), 1);
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.data(model.index(2)).toString(), QStringLiteral("c.mkv"));
        QVERIFY(!model.dropMimeData(drag.get(), Qt::CopyAction, 0, 0, {}));
        QVERIFY(!model.dropMimeData(drag.get(), Qt::MoveAction, 0, 0, model.index(0)));
        PlaylistModel other;
        other.addFiles({"other.mkv"});
        std::unique_ptr<QMimeData> foreign(other.mimeData({other.index(0)}));
        QVERIFY(!model.dropMimeData(foreign.get(), Qt::MoveAction, 0, 0, {}));
        QMimeData forged;
        forged.setData(model.mimeTypes().first(), "0");
        QVERIFY(!model.dropMimeData(&forged, Qt::MoveAction, 0, 0, {}));
        QVERIFY(model.removeAt(0));
        QVERIFY(!model.dropMimeData(drag.get(), Qt::MoveAction, -1, 0, {}));
        std::unique_ptr<QMimeData> last(model.mimeData({model.index(0)}));
        QVERIFY(model.dropMimeData(last.get(), Qt::MoveAction, -1, 0, {}));
        QCOMPARE(model.currentIndex(), 1);
    }

    void editingOtherRowsKeepsPlaybackAndSavesNewOrder() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        const auto second = temporary.filePath(QStringLiteral("second.mkv"));
        const auto third = temporary.filePath(QStringLiteral("third.mkv"));
        QVERIFY(QFile::copy(media, second));
        QVERIFY(QFile::copy(media, third));
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        queue.restoreFiles({media, second, third, media}, 1);
        queue.playAt(1);
        QTRY_COMPARE(loaded.count(), 1);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused && player.state().seekable);
        player.seekAbsolute(1.5);
        QTRY_VERIFY(qAbs(player.state().position - 1.5) < 0.08);
        queue.moveItem(1, 3);
        QCOMPARE(queue.model()->currentIndex(), 3);
        queue.removeAt(0);
        QCOMPARE(queue.model()->currentIndex(), 2);
        QCOMPARE(queue.model()->paths(), QStringList({third, media, second}));
        QVERIFY(player.state().active && player.state().paused);
        QCOMPARE(player.state().path, second);
        QVERIFY(qAbs(player.state().position - 1.5) < 0.08);
        SessionManager session(player, queue, temporary.filePath("session.ini"));
        QVERIFY(session.save({}).isEmpty());
        const auto saved = SessionStore(temporary.filePath("session.ini")).load();
        QVERIFY(saved.error.isEmpty());
        QCOMPARE(saved.state.paths, QStringList({third, media, second}));
        QCOMPARE(saved.state.currentIndex, 2);
        player.seekAbsolute(2.0); // Drain the backend after the edit as well.
        QTRY_VERIFY(qAbs(player.state().position - 2.0) < 0.08);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(!queue.hasNext());
        QVERIFY(queue.hasPrevious());
    }

    void removingCurrentStopsAndSelectsNeighbour() {
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        queue.addFiles({media, media, media});
        QTRY_COMPARE(loaded.count(), 1);
        queue.playAt(1);
        QTRY_COMPARE(loaded.count(), 2);
        queue.removeAt(1);
        QTRY_VERIFY(!player.state().active);
        QCOMPARE(queue.model()->currentIndex(), 1);
        QCOMPARE(queue.model()->rowCount(), 2);
        QCOMPARE(loaded.count(), 2);
        QVERIFY(finished.isEmpty());
        queue.playOrPause();
        QTRY_COMPARE(loaded.count(), 3);
        queue.removeAt(1);
        QTRY_VERIFY(!player.state().active);
        QCOMPARE(queue.model()->currentIndex(), 0);
        queue.removeAt(0);
        QCOMPARE(queue.model()->currentIndex(), -1);
        QVERIFY(!queue.hasNext() && !queue.hasPrevious());
    }

    void failureHistoryFollowsEditedEntries_data() {
        QTest::addColumn<bool>("moving");
        QTest::newRow("native-model-move") << true;
        QTest::newRow("controller-removal") << false;
    }

    void failureHistoryFollowsEditedEntries() {
        QFETCH(bool, moving);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto first = temporary.filePath("bad-a.mkv");
        const auto second = temporary.filePath("bad-b.mkv");
        for (const auto &path : {first, second}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write("invalid media"), 13);
        }
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy errors(&queue, &PlaylistController::queueError);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        QStringList attempted;
        queue.setRepeatMode(PlaylistController::RepeatMode::All);
        queue.restoreFiles(moving ? QStringList{first, second}
                                  : QStringList{QString::fromUtf8(TEST_MEDIA_PATH), first, second},
                           moving ? 0 : 1);
        connect(&queue, &PlaylistController::queueError, this, [&] {
            attempted.append(queue.model()->currentPath());
            if (attempted.size() == 1) {
                if (moving)
                    QVERIFY(queue.model()->moveRow({}, 0, {}, 2));
                else
                    queue.removeAt(0);
            }
        });
        queue.playAt(moving ? 0 : 1);
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(errors.count(), 2);
        QCOMPARE(attempted, QStringList({first, second}));
        QVERIFY(!player.state().active);
    }

    void removalCancelsAutomaticAdvanceEvenFromErrorCallback() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto broken = temporary.filePath("broken.mkv");
        QFile file(broken);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("invalid media"), 13);
        file.close();
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&queue, &PlaylistController::queueError);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        connect(&queue, &PlaylistController::queueError, this,
                [&] { queue.removeAt(queue.model()->currentIndex()); });
        queue.addFiles({broken, QString::fromUtf8(TEST_MEDIA_PATH)});
        QTRY_COMPARE(errors.count(), 1);
        QCOMPARE(queue.model()->currentIndex(), 0);
        QCOMPARE(queue.model()->rowCount(), 1);
        // Reproduce an EOF already queued at removal; it must not restart playback.
        QVERIFY(QMetaObject::invokeMethod(&player, "playbackFinished", Qt::DirectConnection));
        QVERIFY(finished.isEmpty());
        QVERIFY(loaded.isEmpty());
        queue.playOrPause();
        QTRY_COMPARE(loaded.count(), 1);
        QCOMPARE(queue.model()->currentIndex(), 0);
    }

    void invalidAndIdleEditsDoNotStartPlayback() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.restoreFiles({media, "b.mkv", "c.mkv"}, 0);
        queue.moveItem(-1, 0);
        queue.moveItem(0, 3);
        queue.moveItem(0, 0);
        queue.removeAt(-1);
        queue.removeAt(3);
        QCOMPARE(queue.model()->rowCount(), 3);
        queue.moveItem(0, 2);
        QCOMPARE(queue.model()->currentIndex(), 2);
        queue.removeAt(2);
        QCOMPARE(queue.model()->currentIndex(), 1);
        QVERIFY(!player.state().active);
        QVERIFY(loaded.isEmpty());
    }

    void missingFileCallbackCanCancelTheScan_data() {
        QTest::addColumn<bool>("clear");
        QTest::addColumn<bool>("fromEof");
        QTest::newRow("remove-current") << false << false;
        QTest::newRow("clear-queue") << true << false;
        QTest::newRow("eof-remove-current") << false << true;
        QTest::newRow("eof-clear-queue") << true << true;
    }

    void missingFileCallbackCanCancelTheScan() {
        QFETCH(bool, clear);
        QFETCH(bool, fromEof);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy finished(&queue, &PlaylistController::queueFinished);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.restoreFiles(fromEof ? QStringList{media, temporary.filePath("missing.mkv"), media}
                                   : QStringList{temporary.filePath("missing.mkv"), media, media},
                           0);
        if (fromEof) {
            queue.playAt(0);
            QTRY_COMPARE(loaded.count(), 1);
            loaded.clear();
        }
        const auto connection = connect(&queue, &PlaylistController::queueError, this, [&] {
            if (clear)
                queue.clear();
            else
                queue.removeAt(queue.model()->currentIndex());
        });
        if (fromEof)
            QVERIFY(QMetaObject::invokeMethod(&player, "playbackFinished", Qt::DirectConnection));
        else
            queue.playAt(0);
        QTRY_VERIFY(!player.state().active);
        QCOMPARE(queue.model()->currentIndex(), clear ? -1 : 0);
        QVERIFY(finished.isEmpty());
        QVERIFY(loaded.isEmpty());
        disconnect(connection);
        if (!clear && !fromEof) {
            queue.playOrPause();
            QTRY_COMPARE(loaded.count(), 1);
            QCOMPARE(queue.model()->currentIndex(), 0);
        }
    }

    void missingFileCallbackEditsUseLiveRows_data() {
        QTest::addColumn<bool>("moving");
        QTest::newRow("remove-failed-entry") << false;
        QTest::newRow("move-failed-entry") << true;
    }

    void missingFileCallbackEditsUseLiveRows() {
        QFETCH(bool, moving);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        queue.restoreFiles({media, temporary.filePath("missing.mkv"), media}, 0);
        connect(&queue, &PlaylistController::queueError, this, [&] {
            if (moving)
                queue.moveItem(1, 2);
            else
                queue.removeAt(1);
        });
        queue.playAt(1);
        QTRY_COMPARE(loaded.count(), 1);
        QCOMPARE(queue.model()->currentIndex(), 1);
        QCOMPARE(queue.model()->rowCount(), moving ? 3 : 2);
    }
    void batchRemovalDeduplicatesAndKeepsCurrentEntry() {
        PlaylistModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.addFiles({"a.mkv", "b.mkv", "a.mkv", "c.mkv", "d.mkv"});
        model.setCurrentIndex(2);
        QPersistentModelIndex current(model.index(2));
        QSignalSpy changed(&model, &PlaylistModel::currentIndexChanged);
        QCOMPARE(model.removeRowsAt({0, 4, 0, -1, 7}), 2);
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.data(model.index(0)).toString(), QStringLiteral("b.mkv"));
        QCOMPARE(model.data(model.index(1)).toString(), QStringLiteral("a.mkv"));
        QCOMPARE(model.data(model.index(2)).toString(), QStringLiteral("c.mkv"));
        QCOMPARE(model.currentIndex(), 1);
        QCOMPARE(current.row(), 1);
        QVERIFY(model.data(model.index(1), PlaylistModel::CurrentRole).toBool());
        QCOMPARE(changed.count(), 1);
        QCOMPARE(model.removeRowsAt({1, 1, 1}), 1);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.currentIndex(), 1);
        QCOMPARE(changed.count(), 2);
    }

    void batchRemovalSelectsSurvivorsAndClears() {
        PlaylistModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.addFiles({"a.mkv", "b.mkv", "c.mkv", "d.mkv"});
        QSignalSpy changed(&model, &PlaylistModel::currentIndexChanged);
        model.setCurrentIndex(1);
        changed.clear();
        QCOMPARE(model.removeRowsAt({1}), 1);
        QCOMPARE(model.currentIndex(), 1);
        QCOMPARE(model.data(model.index(1)).toString(), QStringLiteral("c.mkv"));
        QCOMPARE(changed.count(), 1);

        model.setCurrentIndex(2);
        changed.clear();
        QCOMPARE(model.removeRowsAt({2}), 1);
        QCOMPARE(model.currentIndex(), 1);
        QCOMPARE(model.data(model.index(1)).toString(), QStringLiteral("c.mkv"));
        QCOMPARE(changed.count(), 1);

        changed.clear();
        QCOMPARE(model.removeRowsAt({0, 1}), 2);
        QCOMPARE(model.currentIndex(), -1);
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(changed.count(), 1);
    }

    void invalidRowsAreIgnoredAndRemoveAtMatchesBulk() {
        PlaylistModel bulk;
        PlaylistModel single;
        for (auto *model : {&bulk, &single}) {
            model->addFiles({"a.mkv", "b.mkv", "c.mkv", "d.mkv"});
            model->setCurrentIndex(2);
        }
        QSignalSpy bulkChanged(&bulk, &PlaylistModel::currentIndexChanged);
        QSignalSpy singleChanged(&single, &PlaylistModel::currentIndexChanged);
        QCOMPARE(bulk.removeRowsAt({-2, 4, 99, 1, 1}), 1);
        QVERIFY(single.removeAt(1));
        QCOMPARE(bulk.paths(), single.paths());
        QCOMPARE(bulk.currentIndex(), single.currentIndex());
        QCOMPARE(bulk.currentIndex(), 1);
        QCOMPARE(bulkChanged.count(), 1);
        QCOMPARE(singleChanged.count(), 1);
        QVERIFY(!single.removeAt(-1));
        QVERIFY(!single.removeAt(3));
        QCOMPARE(single.removeRowsAt({}), 0);
        QCOMPARE(single.rowCount(), 3);
    }

    void titleSortIsNaturalCaseInsensitiveAndStable() {
        PlaylistModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.addFiles({"Episode10.mkv", "episode2.mkv", "EPISODE1.mkv", "episode2.mkv"});
        QPersistentModelIndex lowerDuplicate(model.index(3));
        QPersistentModelIndex ten(model.index(0));
        QVERIFY(model.sortEntries(PlaylistModel::SortKey::Title, Qt::AscendingOrder));
        QCOMPARE(model.data(model.index(0)).toString(), QStringLiteral("EPISODE1.mkv"));
        QCOMPARE(model.data(model.index(1)).toString(), QStringLiteral("episode2.mkv"));
        QCOMPARE(model.data(model.index(2)).toString(), QStringLiteral("episode2.mkv"));
        QCOMPARE(model.data(model.index(3)).toString(), QStringLiteral("Episode10.mkv"));
        QCOMPARE(lowerDuplicate.row(), 2);
        QCOMPARE(ten.row(), 3);

        PlaylistModel chinese;
        chinese.addFiles({QStringLiteral("第 10 集.mkv"), QStringLiteral("第 2 集.mkv")});
        QVERIFY(chinese.sortEntries(PlaylistModel::SortKey::Title, Qt::AscendingOrder));
        QCOMPARE(chinese.data(chinese.index(0)).toString(), QStringLiteral("第 2 集.mkv"));
        QCOMPARE(chinese.data(chinese.index(1)).toString(), QStringLiteral("第 10 集.mkv"));
        QVERIFY(!chinese.sortEntries(PlaylistModel::SortKey::Title, Qt::AscendingOrder));
    }

    void pathSortIsNaturalAndRemapsPersistentIndexes() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto file = [&directory](const char *name) {
            return directory.filePath(QString::fromLatin1(name));
        };
        PlaylistModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.addFiles({file("episode10.mkv"), file("Episode2.mkv"), file("episode1.mkv")});
        model.setCurrentIndex(0);
        QPersistentModelIndex ten(model.index(0));
        QPersistentModelIndex two(model.index(1));
        QSignalSpy changed(&model, &PlaylistModel::currentIndexChanged);
        QSignalSpy data(&model, &PlaylistModel::dataChanged);
        QSignalSpy layouts(&model, &QAbstractItemModel::layoutAboutToBeChanged);
        QVERIFY(model.sortEntries(PlaylistModel::SortKey::Path, Qt::AscendingOrder));
        QCOMPARE(model.paths(),
                 QStringList({file("episode1.mkv"), file("Episode2.mkv"), file("episode10.mkv")}));
        QCOMPARE(ten.row(), 2);
        QCOMPARE(two.row(), 1);
        QCOMPARE(model.currentIndex(), 2);
        QCOMPARE(model.pathAt(2), file("episode10.mkv"));
        QCOMPARE(changed.count(), 1);
        QVERIFY(!data.isEmpty());
        QCOMPARE(layouts.count(), 1);

        changed.clear();
        data.clear();
        layouts.clear();
        QVERIFY(!model.sortEntries(PlaylistModel::SortKey::Path, Qt::AscendingOrder));
        QCOMPARE(changed.count(), 0);
        QVERIFY(data.isEmpty());
        QCOMPARE(layouts.count(), 0);

        QVERIFY(model.sortEntries(PlaylistModel::SortKey::Path, Qt::DescendingOrder));
        QCOMPARE(model.paths(),
                 QStringList({file("episode10.mkv"), file("Episode2.mkv"), file("episode1.mkv")}));
        QCOMPARE(model.currentIndex(), 0);
        QCOMPARE(ten.row(), 0);
    }

    void sortKeepsCurrentEntryAndOnlySignalsWhenItsRowMoves() {
        PlaylistModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.addFiles({"c.mkv", "a.mkv", "b.mkv"});
        model.setCurrentIndex(0);
        const auto currentPath = model.pathAt(0);
        QPersistentModelIndex current(model.index(0));
        QSignalSpy changed(&model, &PlaylistModel::currentIndexChanged);
        QSignalSpy data(&model, &PlaylistModel::dataChanged);
        QVERIFY(model.sortEntries(PlaylistModel::SortKey::Title, Qt::AscendingOrder));
        QCOMPARE(model.currentIndex(), 2);
        QCOMPARE(model.pathAt(2), currentPath);
        QCOMPARE(current.row(), 2);
        QCOMPARE(changed.count(), 1);
        QVERIFY(!data.isEmpty());

        PlaylistModel stable;
        stable.addFiles({"a.mkv", "c.mkv", "b.mkv"});
        stable.setCurrentIndex(0);
        QSignalSpy stableChanged(&stable, &PlaylistModel::currentIndexChanged);
        QSignalSpy stableData(&stable, &PlaylistModel::dataChanged);
        QVERIFY(stable.sortEntries(PlaylistModel::SortKey::Title, Qt::AscendingOrder));
        QCOMPARE(stable.currentIndex(), 0);
        QCOMPARE(stableChanged.count(), 0);
        QVERIFY(!stableData.isEmpty());
    }
};

QTEST_GUILESS_MAIN(PlaylistEditTest)
#include "playlist_edit_test.moc"
