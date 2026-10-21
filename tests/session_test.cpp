#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"
#include "session/sessionmanager.h"
#include "session/sessionstore.h"

#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class SessionTest final : public QObject {
    Q_OBJECT
  private slots:
    void restoredSessionRemainsIdleAndHonorsLaterChanges() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        SessionState saved;
        saved.paths = {temporary.filePath(QStringLiteral("missing.mkv")), media, media};
        saved.currentIndex = 1;
        saved.audio = {23.0, true};
        QVERIFY(SessionStore(file).save(saved).isEmpty());
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, file);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QVERIFY(session.restore().error.isEmpty());
        QCOMPARE(queue.model()->rowCount(), 3);
        QCOMPARE(queue.model()->currentIndex(), 1);
        // Saving before the first mpv observation must preserve loaded preferences.
        QVERIFY(session.save({}).isEmpty());
        QCOMPARE(SessionStore(file).load().state.audio.volume, 23.0);
        QVERIFY(SessionStore(file).load().state.audio.muted);
        QTRY_COMPARE(player.state().volume, 23.0);
        QTRY_VERIFY(player.state().muted);
        QVERIFY(!player.state().active);
        QVERIFY(loaded.isEmpty());
        queue.playOrPause();
        QTRY_COMPARE(loaded.count(), 1);
        QCOMPARE(player.state().path, media);
        player.setVolume(61.0);
        player.setMuted(false);
        session.rememberMedia(media);
        QCOMPARE(session.mediaDirectory(), QFileInfo(media).absolutePath());
        QVERIFY(session.save({}).isEmpty());
        const auto changed = SessionStore(file).load();
        QCOMPARE(changed.state.audio.volume, 61.0);
        QVERIFY(!changed.state.audio.muted);
        QCOMPARE(changed.state.window.mediaDirectory, QFileInfo(media).absolutePath());
        queue.clear();
        QVERIFY(session.save({}).isEmpty());
        MpvController restarted(true, true);
        PlaylistController newQueue(restarted);
        SessionManager newSession(restarted, newQueue, file);
        QVERIFY(newSession.restore().error.isEmpty());
        QCOMPARE(newQueue.model()->rowCount(), 0);
        QTRY_COMPARE(restarted.state().volume, 61.0);
        QVERIFY(!restarted.state().muted);
    }

    void restartPreservesQueueAndPreferences() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("nested/settings.ini"));
        const auto media = temporary.filePath(QStringLiteral("猫 sample [1].mkv"));
        const auto missing = temporary.filePath(QStringLiteral("missing.mkv"));
        SessionState saved;
        saved.paths = {media, missing, media};
        saved.currentIndex = 2;
        saved.audio = {37.5, true};
        saved.window = {QByteArray::fromHex("00ff0012"), true, temporary.path()};
        QVERIFY(SessionStore(file).save(saved).isEmpty());
        const auto loaded = SessionStore(file).load();
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.state.paths, QStringList({media, missing, media}));
        QCOMPARE(loaded.state.currentIndex, 2);
        QCOMPARE(loaded.state.audio.volume, 37.5);
        QVERIFY(loaded.state.audio.muted);
        QCOMPARE(loaded.state.window.geometry, QByteArray::fromHex("00ff0012"));
        QVERIFY(loaded.state.window.playlistVisible);
        QCOMPARE(loaded.state.window.mediaDirectory, temporary.path());
    }

    void emptyQueueReplacesPreviousSession() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        SessionStore store(temporary.filePath(QStringLiteral("settings.ini")));
        SessionState saved;
        saved.paths = {temporary.filePath(QStringLiteral("old.mkv"))};
        saved.currentIndex = 0;
        QVERIFY(store.save(saved).isEmpty());
        QCOMPARE(store.load().state.paths.size(), 1);
        QVERIFY(store.save({}).isEmpty());
        const auto loaded = store.load();
        QVERIFY(loaded.error.isEmpty());
        QVERIFY(loaded.state.paths.isEmpty());
        QCOMPARE(loaded.state.currentIndex, -1);
    }

    void unknownSchemaIsNotOverwritten() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        {
            QSettings settings(file, QSettings::IniFormat);
            settings.setValue(QStringLiteral("schemaVersion"), 99);
            settings.setValue(QStringLiteral("future/value"), QStringLiteral("keep me"));
            settings.sync();
        }
        QFile before(file);
        QVERIFY(before.open(QIODevice::ReadOnly));
        const auto original = before.readAll();
        before.close();
        SessionStore store(file);
        QVERIFY(!store.load().error.isEmpty());
        QVERIFY(!store.save({}).isEmpty());
        QVERIFY(!SessionStore(file).save({}).isEmpty());
        QVERIFY(before.open(QIODevice::ReadOnly));
        QCOMPARE(before.readAll(), original);
    }

    void malformedConfigurationIsPreserved() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        const QByteArray original("[General]\nschemaVersion=1\n[broken group\nvalue=keep\n");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(original), original.size());
        file.close();
        SessionStore store(path);
        QVERIFY(!store.load().error.isEmpty());
        QVERIFY(!store.save({}).isEmpty());
        QVERIFY(!SessionStore(path).save({}).isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), original);
    }

    void pendingFilesSurviveClosingBeforeRendererReady() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, path);
        QVERIFY(session.restore().error.isEmpty());
        QVERIFY(session.save({}, {media, media}).isEmpty());
        const auto loaded = SessionStore(path).load();
        QCOMPARE(loaded.state.paths, QStringList({media, media}));
        QCOMPARE(loaded.state.currentIndex, 0);
        QVERIFY(!player.state().active);
    }

    void invalidValuesCannotEscapeDomainLimits() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        const auto media = temporary.filePath(QStringLiteral("missing.mkv"));
        {
            QSettings settings(file, QSettings::IniFormat);
            settings.setValue(QStringLiteral("schemaVersion"), 1);
            settings.setValue(QStringLiteral("audio/volume"), QStringLiteral("nan"));
            settings.setValue(QStringLiteral("audio/muted"), QStringLiteral("garbage"));
            settings.setValue(QStringLiteral("playlist/paths"),
                              QStringList{QString{}, QStringLiteral("relative.mkv"), media});
            settings.setValue(QStringLiteral("playlist/currentIndex"), 2);
            settings.setValue(QStringLiteral("window/mediaDirectory"), media);
            settings.sync();
        }
        const auto loaded = SessionStore(file).load();
        QVERIFY(loaded.error.isEmpty());
        QCOMPARE(loaded.state.audio.volume, 100.0);
        QVERIFY(!loaded.state.audio.muted);
        QCOMPARE(loaded.state.paths, QStringList{media});
        QCOMPARE(loaded.state.currentIndex, 0);
        QVERIFY(loaded.state.window.mediaDirectory.isEmpty());
    }

    void inaccessibleDestinationReportsFailure() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto blocker = temporary.filePath(QStringLiteral("regular-file"));
        QFile file(blocker);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        SessionStore store(blocker + QStringLiteral("/settings.ini"));
        QVERIFY(!store.save({}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(SessionTest)
#include "session_test.moc"
