#include "playback/playlist/playlistmodel.h"

#include <QAbstractItemModelTester>
#include <QtTest>

class PlaylistBlockMoveTest final : public QObject {
    Q_OBJECT
  private slots:
    void movingContiguousBlocksKeepsOrderAndCurrent() {
        PlaylistModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.addFiles({"a.mkv", "b.mkv", "c.mkv", "d.mkv", "e.mkv"});
        const auto names = [&model] {
            QStringList result;
            for (int row = 0; row < model.rowCount(); ++row)
                result.append(model.data(model.index(row)).toString());
            return result;
        };
        model.setCurrentIndex(1);
        QVERIFY(model.moveBlock(1, 2, -1));
        QCOMPARE(names(), QStringList({"b.mkv", "c.mkv", "a.mkv", "d.mkv", "e.mkv"}));
        QCOMPARE(model.currentIndex(), 0);
        QVERIFY(model.moveBlock(0, 2, 1));
        QCOMPARE(names(), QStringList({"a.mkv", "b.mkv", "c.mkv", "d.mkv", "e.mkv"}));
        QCOMPARE(model.currentIndex(), 1);
        QVERIFY(!model.moveBlock(0, 2, -1));
        QVERIFY(!model.moveBlock(3, 2, 1));
        QVERIFY(!model.moveBlock(0, 2, 2));
    }
};

QTEST_GUILESS_MAIN(PlaylistBlockMoveTest)
#include "playlist_block_move_test.moc"
