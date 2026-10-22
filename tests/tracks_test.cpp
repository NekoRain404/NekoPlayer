#include "playback/mediatrack.h"
#include "playback/mpvcontroller.h"

#include <QSignalSpy>
#include <QtTest>
#include <algorithm>

namespace {
qint64 selectedTrack(const MpvController &player, MediaTrack::Type type) {
    for (const auto &track : player.tracks()) {
        if (track.type == type && track.selected)
            return track.id;
    }
    return 0;
}
} // namespace

class TracksTest final : public QObject {
    Q_OBJECT
  private slots:
    void readsTrackMetadata() {
        MpvController player(true, true);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active);
        player.setPaused(true);
        QTRY_COMPARE(player.tracks().size(), 3);
        const auto &audio = player.tracks().at(0);
        QCOMPARE(audio.type, MediaTrack::Type::Audio);
        QCOMPARE(audio.id, 1);
        QCOMPARE(audio.language, QStringLiteral("jpn"));
        QCOMPARE(audio.title, QStringLiteral("Original"));
        QVERIFY(!audio.codec.isEmpty());
        QVERIFY(!audio.external);
        const auto &subtitle = player.tracks().at(2);
        QCOMPARE(subtitle.type, MediaTrack::Type::Subtitle);
        QCOMPARE(subtitle.id, 1);
        QCOMPARE(subtitle.language, QStringLiteral("zho"));
        QCOMPARE(subtitle.title, QStringLiteral("简体中文"));
    }

    void switchesAudioAndSubtitlesWithoutReloading() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.selectAudioTrack(2);
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Audio), 2);
        player.selectSubtitleTrack(1);
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), 1);
        player.selectSubtitleTrack(0);
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), 0);
        player.selectAudioTrack(0);
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Audio), 0);
        player.selectAudioTrack(1);
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Audio), 1);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(player.state().paused);
    }

    void loadsExternalSubtitleWithUnicodeAndSpaces() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        player.setPaused(true);
        player.addSubtitle(QString::fromUtf8(TEST_EXTERNAL_SUBTITLE_PATH));
        QTRY_COMPARE(player.tracks().size(), 4);
        const auto external = std::find_if(player.tracks().cbegin(), player.tracks().cend(),
                                           [](const MediaTrack &track) { return track.external; });
        QVERIFY(external != player.tracks().cend());
        QCOMPARE(external->type, MediaTrack::Type::Subtitle);
        const auto externalId = external->id;
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), externalId);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(errors.isEmpty());
    }

    void rejectsWrongTrackTypeAndMissingSubtitle() {
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        player.setPaused(true);
        player.selectSubtitleTrack(2); // Audio ID 2 must not be accepted as a subtitle.
        QCOMPARE(errors.count(), 1);
        player.addSubtitle(QStringLiteral("/not/a/subtitle.srt"));
        QTRY_COMPARE(errors.count(), 2);
        QVERIFY(player.state().active);
        QCOMPARE(player.tracks().size(), 3);
    }

    void clearsTracksOnStopAndReplacement() {
        MpvController player(true, true);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        player.stop();
        QTRY_VERIFY(!player.state().active);
        QTRY_VERIFY(player.tracks().isEmpty());
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_COMPARE(player.tracks().size(), 3);
        player.openFile(QStringLiteral("/not/a/media.mkv"));
        QTRY_VERIFY(!player.state().active);
        QTRY_VERIFY(player.tracks().isEmpty());
    }

    void restoresTracksAfterRendererRecovery_data() {
        QTest::addColumn<bool>("external");
        QTest::addColumn<bool>("disabled");
        QTest::newRow("internal-subtitle") << false << false;
        QTest::newRow("external-subtitle") << true << false;
        QTest::newRow("disabled-external-subtitle-and-audio") << true << true;
    }

    void restoresSecondStreamOfExternalSubtitleWithoutDuplicates() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        player.setPaused(true);
        player.addSubtitle(QString::fromUtf8(TEST_MULTI_SUBTITLE_PATH));
        QTRY_COMPARE(player.tracks().size(), 5);
        player.selectSubtitleTrack(3);
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), 3);
        for (int round = 0; round < 2; ++round) {
            player.prepareForRendererReset();
            player.resumeAfterRendererReset();
            QTRY_COMPARE(loaded.count(), round + 2);
            QTRY_COMPARE(player.tracks().size(), 5);
            QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), 3);
            QVERIFY(player.state().paused);
            QVERIFY(errors.isEmpty());
        }
    }

    void repeatedResetWhileRestoringKeepsIntendedTracks() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        player.setPaused(true);
        player.addSubtitle(QString::fromUtf8(TEST_MULTI_SUBTITLE_PATH));
        QTRY_COMPARE(player.tracks().size(), 5);
        player.selectSubtitleTrack(3);
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), 3);
        int reloads = 0;
        connect(&player, &MpvController::fileLoaded, &player, [&] {
            if (++reloads == 1) {
                player.prepareForRendererReset();
                player.resumeAfterRendererReset();
            }
        });
        player.prepareForRendererReset();
        player.resumeAfterRendererReset();
        QTRY_COMPARE(loaded.count(), 3);
        QTRY_COMPARE(player.tracks().size(), 5);
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), 3);
        QVERIFY(player.state().paused);
        QVERIFY(errors.isEmpty());
    }

    void restoresTracksAfterRendererRecovery() {
        QFETCH(bool, external);
        QFETCH(bool, disabled);
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        player.setPaused(true);
        player.selectAudioTrack(2);
        player.selectSubtitleTrack(1);
        QTRY_VERIFY(player.state().paused);
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Audio), 2);
        QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), 1);
        if (external) {
            player.addSubtitle(QString::fromUtf8(TEST_EXTERNAL_SUBTITLE_PATH));
            QTRY_COMPARE(player.tracks().size(), 4);
            QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), 2);
        }
        if (disabled) {
            player.selectAudioTrack(0);
            player.selectSubtitleTrack(0);
            QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Audio), 0);
            QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Subtitle), 0);
        }
        for (int round = 0; round < 2; ++round) {
            player.prepareForRendererReset();
            player.resumeAfterRendererReset();
            QTRY_COMPARE(loaded.count(), round + 2);
            QTRY_COMPARE(player.tracks().size(), external ? 4 : 3);
            QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Audio), disabled ? 0 : 2);
            QTRY_COMPARE(selectedTrack(player, MediaTrack::Type::Subtitle),
                         disabled ? 0 : (external ? 2 : 1));
            QVERIFY(player.state().paused);
            QVERIFY(errors.isEmpty());
        }
    }
};

QTEST_GUILESS_MAIN(TracksTest)
#include "tracks_test.moc"
