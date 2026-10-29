#include "playback/mpv/mpvcontroller.h"
#include "playback/playlist/playlistcontroller.h"
#include "session/sessionmanager.h"
#include "session/sessionstore.h"

#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>

namespace {
QString hardwareDecodingOption(MpvController &player) {
    if (!player.handle())
        return {};
    char *value = mpv_get_property_string(player.handle(), "hwdec");
    const auto text = QString::fromUtf8(value);
    mpv_free(value);
    return text;
}
} // namespace

class DecodingTest final : public QObject {
    Q_OBJECT
  private slots:
    void switchingDecoderKeepsMediaAndPause() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.seekAbsolute(1.0);
        QTRY_VERIFY(std::abs(player.state().position - 1.0) < 0.08);
        player.addSubtitle(QString::fromUtf8(TEST_EXTERNAL_SUBTITLE_PATH));
        QTRY_COMPARE(player.tracks().size(), 2);
        QTRY_VERIFY(player.tracks().back().selected && player.tracks().back().external);
        const auto tracks = player.tracks();
        player.setHardwareDecodingEnabled(false);
        QTRY_COMPARE(hardwareDecodingOption(player), QStringLiteral("no"));
        QVERIFY(player.state().active && player.state().paused);
        QTRY_VERIFY(std::abs(player.state().position - 1.0) < 0.08);
        QCOMPARE(loaded.count(), 1);
        QTRY_COMPARE(player.tracks(), tracks);
        player.setHardwareDecodingEnabled(true);
        QTRY_COMPARE(hardwareDecodingOption(player), QStringLiteral("auto-safe"));
        QVERIFY(player.state().active && player.state().paused);
        QCOMPARE(loaded.count(), 1);
        QTRY_COMPARE(player.tracks(), tracks);
        QVERIFY(errors.isEmpty());
    }

    void restoredSoftwarePreferenceAppliesWhileIdle() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.setValue(QStringLiteral("schemaVersion"), 1);
            settings.setValue(QStringLiteral("video/hardwareDecoding"), false);
            settings.sync();
        }
        MpvController player(true, true);
        PlaylistController playlist(player);
        SessionManager session(player, playlist, path);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QVERIFY(session.restore().error.isEmpty());
        QTRY_COMPARE(hardwareDecodingOption(player), QStringLiteral("no"));
        QVERIFY(!player.state().active && loaded.isEmpty());
    }

    void immediateSaveKeepsLatestPreferenceAcrossRestart() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        {
            MpvController player(true, true);
            PlaylistController playlist(player);
            SessionManager session(player, playlist, path);
            QVERIFY(session.restore().error.isEmpty());
            // No Qt event draining between requests and saving: observations may lag.
            player.setHardwareDecodingEnabled(false);
            player.setHardwareDecodingEnabled(true);
            player.setHardwareDecodingEnabled(false);
            QVERIFY(session.save({}).isEmpty());
            QVERIFY(!SessionStore(path).load().state.hardwareDecodingEnabled);
            QTRY_COMPARE(hardwareDecodingOption(player), QStringLiteral("no"));
        }
        MpvController restarted(true, true);
        PlaylistController playlist(restarted);
        SessionManager session(restarted, playlist, path);
        QVERIFY(session.restore().error.isEmpty());
        restarted.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(restarted.state().active);
        QCOMPARE(hardwareDecodingOption(restarted), QStringLiteral("no"));
        restarted.setPaused(true);
        QTRY_VERIFY(restarted.state().paused);
        QSignalSpy loaded(&restarted, &MpvController::fileLoaded);
        restarted.prepareForRendererReset();
        restarted.resumeAfterRendererReset();
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(restarted.state().active && restarted.state().paused);
        QCOMPARE(hardwareDecodingOption(restarted), QStringLiteral("no"));
        restarted.stop();
        QTRY_VERIFY(!restarted.state().active);
        restarted.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(restarted.state().active);
        QCOMPARE(hardwareDecodingOption(restarted), QStringLiteral("no"));
    }

    void optionalPreferencePreservesOldConfiguration_data() {
        QTest::addColumn<QVariant>("value");
        QTest::addColumn<bool>("enabled");
        QTest::newRow("old-config") << QVariant{} << true;
        QTest::newRow("malformed") << QVariant(QStringLiteral("invalid")) << true;
        QTest::newRow("software") << QVariant(false) << false;
        QTest::newRow("automatic") << QVariant(true) << true;
        QTest::newRow("numeric-off") << QVariant(0) << false;
    }

    void optionalPreferencePreservesOldConfiguration() {
        QFETCH(QVariant, value);
        QFETCH(bool, enabled);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.setValue(QStringLiteral("schemaVersion"), 1);
            if (value.isValid())
                settings.setValue(QStringLiteral("video/hardwareDecoding"), value);
            settings.sync();
        }
        const auto loaded = SessionStore(path).load();
        QVERIFY(loaded.error.isEmpty());
        QCOMPARE(loaded.state.hardwareDecodingEnabled, enabled);
    }
};

QTEST_GUILESS_MAIN(DecodingTest)
#include "decoding_test.moc"
