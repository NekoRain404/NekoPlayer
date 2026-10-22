#include "input/localfiledrop.h"
#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>

class FileDropTest final : public QObject {
    Q_OBJECT
  private slots:
    void appendsMediaInOrderWithDuplicates() {
        MpvController player(true, true);
        PlaylistController queue(player);
        LocalFileDrop drop(player);
        connect(&drop, &LocalFileDrop::mediaFilesDropped, &queue, &PlaylistController::addFiles);
        QSignalSpy errors(&drop, &LocalFileDrop::errorOccurred);
        const auto path = QString::fromUtf8(TEST_TRACK_MEDIA_PATH);
        const auto other = QStringLiteral("/tmp/猫 #2.unknown");
        QVERIFY(drop.handleUrls(
            {QUrl::fromLocalFile(path), QUrl::fromLocalFile(other), QUrl::fromLocalFile(path)}));
        QTRY_VERIFY(player.state().active);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QCOMPARE(queue.model()->rowCount(), 3);
        QCOMPARE(queue.model()->paths(), QStringList({path, other, path}));
        QVERIFY(errors.isEmpty());
    }

    void loadsSubtitleWithoutReplacingQueueOrLeavingPause() {
        MpvController player(true, true);
        PlaylistController queue(player);
        LocalFileDrop drop(player);
        connect(&drop, &LocalFileDrop::mediaFilesDropped, &queue, &PlaylistController::addFiles);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        const auto media = QString::fromUtf8(TEST_TRACK_MEDIA_PATH);
        queue.addFiles({media, media});
        QTRY_COMPARE(player.tracks().size(), 3);
        QTRY_VERIFY(player.state().active);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.seekAbsolute(1.0);
        QTRY_VERIFY(player.state().position >= 0.9);
        const auto position = player.state().position;
        const auto subtitle = QString::fromUtf8(TEST_EXTERNAL_SUBTITLE_PATH);
        QVERIFY(drop.handleUrls({QUrl::fromLocalFile(subtitle)}));
        QTRY_COMPARE(player.tracks().size(), 4);
        QTRY_VERIFY(std::any_of(
            player.tracks().cbegin(), player.tracks().cend(), [](const MediaTrack &track) {
                return track.external && track.selected && track.type == MediaTrack::Type::Subtitle;
            }));
        QCOMPARE(queue.model()->paths(), QStringList({media, media}));
        QCOMPARE(queue.model()->currentIndex(), 0);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(player.state().paused);
        QVERIFY(std::abs(player.state().position - position) < 0.08);
        QVERIFY(errors.isEmpty());
    }

    void rejectsWholeInvalidDrop_data() {
        QTest::addColumn<QList<QUrl>>("urls");
        const auto media = QUrl::fromLocalFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        const auto sub = QUrl::fromLocalFile(QString::fromUtf8(TEST_EXTERNAL_SUBTITLE_PATH));
        QTest::newRow("empty") << QList<QUrl>{};
        QTest::newRow("remote-after-media")
            << QList<QUrl>{media, QUrl("https://example.com/a.mkv")};
        QTest::newRow("remote-file-host") << QList<QUrl>{media, QUrl("file://server/share/a.mkv")};
        QTest::newRow("localhost-authority") << QList<QUrl>{QUrl("file://localhost/tmp/a.srt")};
        QTest::newRow("invalid-url") << QList<QUrl>{QUrl("http://[")};
        QTest::newRow("empty-local-path") << QList<QUrl>{QUrl("file:")};
        QTest::newRow("media-then-subtitle") << QList<QUrl>{media, sub};
        QTest::newRow("subtitle-then-media") << QList<QUrl>{sub, media};
        QTest::newRow("multiple-subtitles") << QList<QUrl>{sub, sub};
    }

    void rejectsWholeInvalidDrop() {
        QFETCH(QList<QUrl>, urls);
        MpvController player(true, true);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_COMPARE(player.tracks().size(), 3);
        QTRY_VERIFY(player.state().active);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        LocalFileDrop drop(player);
        QSignalSpy media(&drop, &LocalFileDrop::mediaFilesDropped);
        QSignalSpy errors(&drop, &LocalFileDrop::errorOccurred);
        QVERIFY(!drop.handleUrls(urls));
        QCOMPARE(errors.count(), 1);
        QVERIFY(!errors.constFirst().constFirst().toString().isEmpty());
        QVERIFY(media.isEmpty());
        // A subsequent observed command drains earlier subtitle commands too.
        player.setSubtitleDelay(0.5);
        QTRY_COMPARE(player.state().subtitleDelay, 0.5);
        QCOMPARE(player.tracks().size(), 3);
        QVERIFY(player.state().active && player.state().paused);
    }

    void recognizesSubtitleExtensionsWhileIdle_data() {
        QTest::addColumn<QString>("extension");
        for (const auto *extension : {"srt", "ASS", "SsA", "sub", "vtt", "sup", "idx", "mks"})
            QTest::newRow(extension) << QString::fromLatin1(extension);
    }

    void recognizesSubtitleExtensionsWhileIdle() {
        QFETCH(QString, extension);
        MpvController player(true, true);
        LocalFileDrop drop(player);
        QSignalSpy media(&drop, &LocalFileDrop::mediaFilesDropped);
        QSignalSpy errors(&drop, &LocalFileDrop::errorOccurred);
        const auto path = QStringLiteral("/tmp/字幕.%1").arg(extension);
        QVERIFY(!drop.handleUrls({QUrl::fromLocalFile(path)}));
        QCOMPARE(errors.count(), 1);
        QVERIFY(media.isEmpty());
        QVERIFY(player.tracks().isEmpty());
    }

    void loadsUppercaseSubtitleAndReportsMissingFileAsBackendError() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(QStringLiteral("字幕 #1.SRT"));
        QVERIFY(QFile::copy(QString::fromUtf8(TEST_EXTERNAL_SUBTITLE_PATH), path));
        MpvController player(true, true);
        LocalFileDrop drop(player);
        QSignalSpy media(&drop, &LocalFileDrop::mediaFilesDropped);
        QSignalSpy inputErrors(&drop, &LocalFileDrop::errorOccurred);
        QSignalSpy backendErrors(&player, &MpvController::errorOccurred);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_COMPARE(player.tracks().size(), 3);
        QTRY_VERIFY(player.state().active);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QVERIFY(drop.handleUrls({QUrl::fromLocalFile(path)}));
        QTRY_COMPARE(player.tracks().size(), 4);
        QVERIFY(drop.handleUrls({QUrl::fromLocalFile(directory.filePath("missing.srt"))}));
        QTRY_COMPARE(backendErrors.count(), 1);
        QVERIFY(media.isEmpty());
        QVERIFY(inputErrors.isEmpty());
        QCOMPARE(player.tracks().size(), 4);
        QVERIFY(player.state().active && player.state().paused);
    }
};

QTEST_GUILESS_MAIN(FileDropTest)
#include "file_drop_test.moc"
