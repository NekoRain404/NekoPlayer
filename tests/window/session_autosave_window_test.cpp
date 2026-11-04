#include "playback/mpv/mpvcontroller.h"
#include "session/sessionstore.h"
#include "ui/window/mainwindow.h"

#include <QTemporaryDir>
#include <QtTest>

class SessionAutosaveWindowTest final : public QObject {
    Q_OBJECT
  private slots:
    void writesQueueAndPositionWithoutClosing() {
        qputenv("NEKOPLAYER_AUTOSAVE_MS", "200");
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto sessionPath = directory.filePath(QStringLiteral("session.ini"));
        MainWindow window(true, sessionPath);
        window.show();
        window.openFiles({QString::fromUtf8(TEST_MEDIA_PATH)});
        QTRY_VERIFY(window.controller()->state().active && window.controller()->state().seekable);
        QTest::qWait(1200);
        const auto loaded = SessionStore(sessionPath).load();
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.state.paths, QStringList{QString::fromUtf8(TEST_MEDIA_PATH)});
        QCOMPARE(loaded.state.currentIndex, 0);
        QVERIFY(loaded.state.resumePosition > 0);
        window.close();
    }
};

QTEST_MAIN(SessionAutosaveWindowTest)
#include "session_autosave_window_test.moc"
