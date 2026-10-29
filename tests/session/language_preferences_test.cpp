#include "playback/mpv/mpvcontroller.h"
#include "playback/playlist/playlistcontroller.h"
#include "session/sessionmanager.h"
#include "session/sessionstate.h"
#include "session/sessionstore.h"

#include <QByteArray>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

namespace {
QStringList numbered(int count) {
    QStringList values;
    values.reserve(count);
    for (int index = 1; index <= count; ++index)
        values.append(QStringLiteral("l%1").arg(index));
    return values;
}
} // namespace

class LanguagePreferencesTest final : public QObject {
    Q_OBJECT
  private slots:
    void defaultsAreEmptyAndAutoLoadDisabled() {
        const LanguagePreferences preferences;
        QVERIFY(preferences.audioLanguages.isEmpty());
        QVERIFY(preferences.subtitleLanguages.isEmpty());
        QVERIFY(!preferences.autoLoadExternalSubtitles);
        QVERIFY(preferences.normalized() == preferences);
    }

    void storeRoundTripPersistsAllThreeKeys() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        SessionState saved;
        saved.languages.audioLanguages = {QStringLiteral("jpn"), QStringLiteral("eng")};
        saved.languages.subtitleLanguages = {QStringLiteral("chi"), QStringLiteral("eng")};
        saved.languages.autoLoadExternalSubtitles = true;
        QVERIFY(SessionStore(path).save(saved).isEmpty());

        QSettings persisted(path, QSettings::IniFormat);
        QCOMPARE(persisted.value(QStringLiteral("schemaVersion")).toInt(), 1);
        QCOMPARE(persisted.value(QStringLiteral("tracks/preferredAudioLanguages")).toStringList(),
                 saved.languages.audioLanguages);
        QCOMPARE(
            persisted.value(QStringLiteral("tracks/preferredSubtitleLanguages")).toStringList(),
            saved.languages.subtitleLanguages);
        QVERIFY(persisted.value(QStringLiteral("subtitles/autoLoadExternal")).toBool());

        const auto loaded = SessionStore(path).load();
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.state.languages.audioLanguages, saved.languages.audioLanguages);
        QCOMPARE(loaded.state.languages.subtitleLanguages, saved.languages.subtitleLanguages);
        QVERIFY(loaded.state.languages.autoLoadExternalSubtitles);
    }

    void missingKeysUseDefaults() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.setValue(QStringLiteral("schemaVersion"), 1);
            settings.setValue(QStringLiteral("audio/volume"), 42.0);
            settings.sync();
        }
        const auto loaded = SessionStore(path).load();
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QVERIFY(loaded.state.languages.audioLanguages.isEmpty());
        QVERIFY(loaded.state.languages.subtitleLanguages.isEmpty());
        QVERIFY(!loaded.state.languages.autoLoadExternalSubtitles);
    }

    void malformedValuesFallBackIndependently() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        QSettings settings(path, QSettings::IniFormat);
        settings.setValue(QStringLiteral("schemaVersion"), 1);
        settings.setValue(QStringLiteral("tracks/preferredAudioLanguages"),
                          QByteArrayLiteral("not-a-list"));
        settings.setValue(QStringLiteral("tracks/preferredSubtitleLanguages"),
                          QStringList{QStringLiteral(" eng "), QStringLiteral("chi")});
        settings.setValue(QStringLiteral("subtitles/autoLoadExternal"), QStringLiteral("garbage"));
        settings.sync();

        const auto first = SessionStore(path).load();
        QVERIFY2(first.error.isEmpty(), qPrintable(first.error));
        QVERIFY(first.state.languages.audioLanguages.isEmpty());
        const QStringList subtitles{QStringLiteral("eng"), QStringLiteral("chi")};
        QCOMPARE(first.state.languages.subtitleLanguages, subtitles);
        QVERIFY(!first.state.languages.autoLoadExternalSubtitles);

        settings.setValue(QStringLiteral("tracks/preferredAudioLanguages"),
                          QStringList{QStringLiteral("jpn")});
        settings.setValue(QStringLiteral("tracks/preferredSubtitleLanguages"),
                          QByteArrayLiteral("not-a-list"));
        settings.setValue(QStringLiteral("subtitles/autoLoadExternal"), true);
        settings.sync();

        const auto second = SessionStore(path).load();
        QVERIFY2(second.error.isEmpty(), qPrintable(second.error));
        const QStringList audio{QStringLiteral("jpn")};
        QCOMPARE(second.state.languages.audioLanguages, audio);
        QVERIFY(second.state.languages.subtitleLanguages.isEmpty());
        QVERIFY(second.state.languages.autoLoadExternalSubtitles);
    }

    void normalizationTrimsDropsEmptyCapsEachListAndPreservesOrder() {
        LanguagePreferences preferences;
        preferences.audioLanguages = {QStringLiteral("  jpn  "), QString(), QStringLiteral("   "),
                                      QStringLiteral("eng")};
        preferences.subtitleLanguages = numbered(20);
        preferences.subtitleLanguages.insert(0, QStringLiteral(" "));
        preferences.subtitleLanguages.insert(1, QString());
        preferences.autoLoadExternalSubtitles = true;

        const auto normalized = preferences.normalized();
        const QStringList trimmedAudio{QStringLiteral("jpn"), QStringLiteral("eng")};
        QCOMPARE(normalized.audioLanguages, trimmedAudio);
        QCOMPARE(normalized.subtitleLanguages, numbered(16));
        QVERIFY(normalized.autoLoadExternalSubtitles);
    }

    void sessionManagerRestoresAndSavesNormalizedPreferences() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("settings.ini"));
        SessionState saved;
        saved.languages.audioLanguages = {QStringLiteral("jpn"), QStringLiteral("eng")};
        saved.languages.subtitleLanguages = {QStringLiteral("chi")};
        saved.languages.autoLoadExternalSubtitles = true;
        QVERIFY(SessionStore(path).save(saved).isEmpty());

        MpvController player(true, true);
        PlaylistController queue(player);
        SessionManager session(player, queue, path);
        QVERIFY(session.restore().error.isEmpty());
        QCOMPARE(session.languagePreferences().audioLanguages, saved.languages.audioLanguages);
        QCOMPARE(session.languagePreferences().subtitleLanguages,
                 saved.languages.subtitleLanguages);
        QVERIFY(session.languagePreferences().autoLoadExternalSubtitles);

        LanguagePreferences changed;
        changed.audioLanguages = {QStringLiteral("  fre  "), QString(), QStringLiteral("ger")};
        changed.subtitleLanguages = numbered(18);
        changed.autoLoadExternalSubtitles = false;
        session.setLanguagePreferences(changed);
        const QStringList trimmedAudio{QStringLiteral("fre"), QStringLiteral("ger")};
        QCOMPARE(session.languagePreferences().audioLanguages, trimmedAudio);
        QCOMPARE(session.languagePreferences().subtitleLanguages, numbered(16));
        QVERIFY(!session.languagePreferences().autoLoadExternalSubtitles);

        QVERIFY(session.save({}).isEmpty());
        const auto loaded = SessionStore(path).load();
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.state.languages.audioLanguages, trimmedAudio);
        QCOMPARE(loaded.state.languages.subtitleLanguages, numbered(16));
        QVERIFY(!loaded.state.languages.autoLoadExternalSubtitles);
    }

    void equalityCoversBothListsAndAutoLoad() {
        LanguagePreferences left;
        LanguagePreferences right;
        QVERIFY(left == right);
        right.audioLanguages = {QStringLiteral("eng")};
        QVERIFY(left != right);
        right = left;
        right.subtitleLanguages = {QStringLiteral("chi")};
        QVERIFY(left != right);
        right = left;
        right.autoLoadExternalSubtitles = true;
        QVERIFY(left != right);
        right = left;
        QVERIFY(left == right);
    }
};

QTEST_GUILESS_MAIN(LanguagePreferencesTest)
#include "language_preferences_test.moc"
