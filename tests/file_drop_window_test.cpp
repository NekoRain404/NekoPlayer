#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"
#include "playback/videowidget.h"
#include "ui/mainwindow.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QtTest>

namespace {
bool dropUrls(MainWindow &window, const QList<QUrl> &urls) {
    QMimeData mime;
    mime.setUrls(urls);
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    QDragEnterEvent enter(QPointF(20, 20), Qt::CopyAction, &mime,
#else
    QDragEnterEvent enter(QPoint(20, 20), Qt::CopyAction, &mime,
#endif
                          Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&window, &enter);
    QDropEvent drop(QPointF(20, 20), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&window, &drop);
    return drop.isAccepted();
}
} // namespace

class FileDropWindowTest final : public QObject {
    Q_OBJECT
  private slots:
    void rejectsSubtitleWhileIdleAndRelaysError() {
        MainWindow window(true);
        QSignalSpy errors(&window, &MainWindow::userError);
        QVERIFY(!dropUrls(window, {QUrl::fromLocalFile(TEST_EXTERNAL_SUBTITLE_PATH)}));
        QCOMPARE(errors.count(), 1);
        QCOMPARE(window.playlist()->model()->rowCount(), 0);
    }

    void routesSubtitleToPlayingFileAndMediaToQueue() {
        MainWindow window(true);
        window.show();
        QTRY_VERIFY(window.videoWidget()->isReady());
        window.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(window.controller()->state().active &&
                    window.controller()->tracks().size() == 3);
        window.controller()->setPaused(true);
        QTRY_VERIFY(window.controller()->state().paused);
        QSignalSpy errors(&window, &MainWindow::userError);
        QVERIFY(dropUrls(window, {QUrl::fromLocalFile(TEST_EXTERNAL_SUBTITLE_PATH)}));
        QTRY_COMPARE(window.controller()->tracks().size(), 4);
        QCOMPARE(window.playlist()->model()->rowCount(), 1);
        QVERIFY(window.controller()->state().paused);
        QVERIFY(dropUrls(window, {QUrl::fromLocalFile(TEST_TRACK_MEDIA_PATH)}));
        QCOMPARE(window.playlist()->model()->rowCount(), 2);
        QVERIFY(!dropUrls(window, {QUrl::fromLocalFile(TEST_TRACK_MEDIA_PATH),
                                   QUrl::fromLocalFile(TEST_EXTERNAL_SUBTITLE_PATH)}));
        QCOMPARE(errors.count(), 1);
        QCOMPARE(window.playlist()->model()->rowCount(), 2);
    }
};

QTEST_MAIN(FileDropWindowTest)
#include "file_drop_window_test.moc"
