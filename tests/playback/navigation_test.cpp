#include "playback/mpv/mpvcontroller.h"

#include <QSignalSpy>
#include <QtTest>
#include <climits>
#include <cmath>

class NavigationTest final : public QObject {
    Q_OBJECT
  private slots:
    void rapidChapterRequestsPreserveUserIntent() {
        MpvController player(true, true);
        player.openFile(QString::fromUtf8(TEST_CHAPTER_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QTRY_COMPARE(player.state().currentChapter, 0);
        player.nextChapter();
        QCOMPARE(player.chapterNavigationIndex(), 1);
        player.nextChapter();
        QCOMPARE(player.chapterNavigationIndex(), 2);
        QTRY_COMPARE(player.state().currentChapter, 2);
        player.previousChapter();
        player.nextChapter();
        QTest::qWait(100);
        QCOMPARE(player.state().currentChapter, 2);
        player.selectChapter(0);
        player.nextChapter();
        QTRY_COMPARE(player.state().currentChapter, 1);
        player.seekAbsolute(0);
        QTRY_COMPARE(player.state().currentChapter, 0);
        player.nextChapter();
        QTRY_COMPARE(player.state().currentChapter, 1);
        QVERIFY(player.state().paused);
    }

    void identicalMediaReloadRefreshesChapterSnapshot() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        const auto path = QString::fromUtf8(TEST_CHAPTER_MEDIA_PATH);
        player.openFile(path);
        QTRY_VERIFY(player.state().active && player.state().seekable);
        QTRY_COMPARE(player.chapters().size(), 3);
        QTRY_COMPARE(player.state().currentChapter, 0);
        player.openFile(path);
        // Let the backend replace the file before Qt drains its event queue.
        QThread::msleep(300);
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_COMPARE(player.chapters().size(), 3);
        QTRY_COMPARE(player.state().currentChapter, 0);
    }

    void noOpChapterRequestsDoNotLeaveStaleIntent() {
        MpvController player(true, true);
        player.openFile(QString::fromUtf8(TEST_CHAPTER_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.selectChapter(2);
        QTRY_COMPARE(player.state().currentChapter, 2);
        player.selectChapter(2);
        QTest::qWait(100);
        const char *arguments[]{"set", "chapter", "0", nullptr};
        QVERIFY(mpv_command(player.handle(), arguments) >= 0);
        QTRY_COMPARE(player.state().currentChapter, 0);
        player.nextChapter();
        QTRY_COMPARE(player.state().currentChapter, 1);
        player.nextChapter();
        player.seekAbsolute(0);
        QTRY_COMPARE(player.state().currentChapter, 0);
        player.nextChapter();
        QTRY_COMPARE(player.state().currentChapter, 1);
        player.nextChapter();
        player.stop();
        QTRY_VERIFY(!player.state().active);
        player.openFile(QString::fromUtf8(TEST_CHAPTER_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        QTRY_COMPARE(player.state().currentChapter, 0);
        player.nextChapter();
        QTRY_COMPARE(player.state().currentChapter, 1);
    }

    void ordinarySeekSupersedesQueuedChapterSelection() {
        MpvController player(true, true);
        player.openFile(QString::fromUtf8(TEST_CHAPTER_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        for (int attempt = 0; attempt < 6; ++attempt) {
            player.selectChapter(1);
            QTRY_COMPARE(player.state().currentChapter, 1);
            player.selectChapter(2);
            player.seekAbsolute(0);
            QTRY_VERIFY(player.state().position < 0.08 && player.state().currentChapter == 0);
            int64_t chapter = -1;
            QVERIFY(mpv_get_property(player.handle(), "chapter", MPV_FORMAT_INT64, &chapter) >= 0);
            QCOMPARE(chapter, 0);
            QCOMPARE(player.chapterNavigationIndex(), 0);
        }
        QVERIFY(player.state().active && player.state().paused);
    }

    void sharedChapterStartConfirmsActualDestination() {
        MpvController player(true, true);
        player.openFile(QString::fromUtf8(TEST_OVERLAPPING_CHAPTER_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QTRY_COMPARE(player.chapters().size(), 4);
        player.selectChapter(1);
        QTRY_COMPARE(player.state().currentChapter, 2);
        QTRY_COMPARE(player.chapterNavigationIndex(), 2);
        player.nextChapter();
        QTRY_COMPARE(player.state().currentChapter, 3);
        QVERIFY(player.state().active && player.state().paused);
    }

    void chapterMetadataIsOwnedAndClearedOnReplacement() {
        MpvController player(true, true);
        QSignalSpy changes(&player, &MpvController::chaptersChanged);
        player.openFile(QString::fromUtf8(TEST_CHAPTER_MEDIA_PATH));
        QTRY_COMPARE(player.chapters().size(), 3);
        QCOMPARE(player.chapters()[0].title, QStringLiteral("序章 开始"));
        QCOMPARE(player.chapters()[1].title, QStringLiteral("第二章 & 中段"));
        QCOMPARE(player.chapters()[1].start, 1.25);
        QCOMPARE(player.chapters()[2].index, 2);
        QCOMPARE(player.chapters()[2].start, 2.5);
        const auto snapshot = player.chapters();
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        const auto metadataChanges = changes.count();
        player.selectChapter(2);
        QTRY_COMPARE(player.state().currentChapter, 2);
        QCOMPARE(changes.count(), metadataChanges);
        player.stop();
        QTRY_VERIFY(!player.state().active);
        QVERIFY(player.chapters().isEmpty());
        QCOMPARE(player.state().currentChapter, -1);
        QCOMPARE(snapshot[1].title, QStringLiteral("第二章 & 中段"));
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(player.state().active);
        QVERIFY(player.chapters().isEmpty());
        QCOMPARE(player.state().currentChapter, -1);
        player.openFile(QStringLiteral("/missing-neko-navigation.mkv"));
        QTRY_VERIFY(!player.state().active);
        QVERIFY(player.chapters().isEmpty());
    }

    void invalidChaptersCannotSeekOrFinishPlayback() {
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        QSignalSpy finished(&player, &MpvController::playbackFinished);
        player.selectChapter(0);
        player.nextChapter();
        player.previousChapter();
        player.openFile(QString::fromUtf8(TEST_CHAPTER_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.selectChapter(1);
        QTRY_COMPARE(player.state().currentChapter, 1);
        const auto position = player.state().position;
        player.selectChapter(-1);
        player.selectChapter(3);
        player.selectChapter(INT_MAX);
        QTest::qWait(100);
        QVERIFY(std::abs(player.state().position - position) < 0.08);
        QCOMPARE(player.state().currentChapter, 1);
        QVERIFY(player.state().paused && player.state().active);
        QVERIFY(errors.isEmpty() && finished.isEmpty());
    }

    void chapterSelectionSeeksWithoutReloadingOrLeavingPause() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        player.openFile(QString::fromUtf8(TEST_CHAPTER_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QVERIFY(QMetaObject::invokeMethod(&player, "selectChapter", Q_ARG(int, 1)));
        QTRY_VERIFY(std::abs(player.state().position - 1.25) < 0.08);
        QVERIFY(player.state().paused);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(QMetaObject::invokeMethod(&player, "nextChapter"));
        QTRY_VERIFY(std::abs(player.state().position - 2.5) < 0.08);
        QVERIFY(QMetaObject::invokeMethod(&player, "nextChapter"));
        QTest::qWait(100);
        QVERIFY(player.state().active && player.state().paused);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(QMetaObject::invokeMethod(&player, "previousChapter"));
        QTRY_VERIFY(std::abs(player.state().position - 1.25) < 0.08);
        QVERIFY(QMetaObject::invokeMethod(&player, "selectChapter", Q_ARG(int, 0)));
        QTRY_VERIFY(player.state().position < 0.08);
        QVERIFY(QMetaObject::invokeMethod(&player, "previousChapter"));
        QTest::qWait(100);
        QVERIFY(player.state().position < 0.08);
        QVERIFY(player.state().paused);
    }
};

QTEST_GUILESS_MAIN(NavigationTest)
#include "navigation_test.moc"
