#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QtTest>

class PlaylistTest final : public QObject {
    Q_OBJECT
  private slots:
    void modelMaintainsOrderAndSelection() {
        PlaylistModel model;
        QCOMPARE(model.addFiles({QStringLiteral("a.mkv"), QStringLiteral("b.mkv")}), 2);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0)).toString(), QStringLiteral("a.mkv"));
        QVERIFY(model.setCurrentIndex(1));
        QCOMPARE(model.currentPath(), QDir::current().absoluteFilePath(QStringLiteral("b.mkv")));
        QVERIFY(model.removeAt(0));
        QCOMPARE(model.currentIndex(), 0);
        QCOMPARE(model.rowCount(), 1);
    }

    void controllerStartsFirstAndAdvancesAtEof() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        const auto first = QString::fromUtf8(TEST_MEDIA_PATH);
        const auto second =
            QDir::temp().absoluteFilePath(QStringLiteral("neko-playlist-second.mkv"));
        QFile::remove(second);
        QVERIFY(QFile::copy(first, second));
        queue.addFiles({first, second});
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 5000);
        QCOMPARE(queue.model()->currentIndex(), 0);
        QVERIFY(queue.hasNext());
        player.seekAbsolute(3.5);
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 2, 5000);
        QCOMPARE(queue.model()->currentIndex(), 1);
        QFile::remove(second);
    }

    void invalidFirstItemReportsAndSkipsToNextFile() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QSignalSpy errors(&queue, &PlaylistController::queueError);
        queue.addFiles({QStringLiteral("/not/a/file.mkv"), QString::fromUtf8(TEST_MEDIA_PATH)});
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 5000);
        QCOMPARE(queue.model()->currentIndex(), 1);
        QVERIFY(!errors.isEmpty());
    }
};

QTEST_GUILESS_MAIN(PlaylistTest)
#include "playlist_test.moc"
