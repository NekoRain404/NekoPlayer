#include "playback/media/mediatrack.h"
#include "playback/mpv/mpvcontroller.h"

#include <QSignalSpy>
#include <QtTest>
#include <mpv/client.h>

namespace {
QList<MediaTrack> tracksOf(const MpvController &player, MediaTrack::Type type) {
    QList<MediaTrack> result;
    for (const auto &track : player.tracks()) {
        if (track.type == type)
            result.append(track);
    }
    return result;
}

qint64 backendSecondary(const MpvController &player) {
    if (!player.handle())
        return -1;
    char *text = nullptr;
    if (mpv_get_property(player.handle(), "secondary-sid", MPV_FORMAT_STRING, &text) < 0)
        return -1;
    const QString value = QString::fromUtf8(text ? text : "");
    mpv_free(text);
    if (value == QLatin1String("no") || value.isEmpty())
        return 0;
    bool ok = false;
    const auto id = value.toLongLong(&ok);
    return ok ? id : -1;
}
bool backendSecondaryVisible(const MpvController &player) {
    int flag = -1;
    if (!player.handle() ||
        mpv_get_property(player.handle(), "secondary-sub-visibility", MPV_FORMAT_FLAG, &flag) < 0)
        return false;
    return flag == 1;
}
} // namespace

class SecondarySubtitlesTest final : public QObject {
    Q_OBJECT
  private slots:
    void selectsAndClearsSecondarySubtitle() {
        MpvController player(true, true);
        player.openFile(QString::fromUtf8(TEST_LANGUAGE_MEDIA_PATH));
        QTRY_VERIFY(player.state().active);
        QTRY_VERIFY(tracksOf(player, MediaTrack::Type::Subtitle).size() == 2);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);

        const auto subtitles = tracksOf(player, MediaTrack::Type::Subtitle);
        player.selectSubtitleTrack(subtitles.at(0).id);
        QTRY_VERIFY(tracksOf(player, MediaTrack::Type::Subtitle).at(0).selected);
        QSignalSpy changed(&player, &MpvController::secondarySubtitleChanged);
        player.selectSecondarySubtitleTrack(subtitles.at(1).id);
        QTRY_COMPARE(player.secondarySubtitleTrack(), subtitles.at(1).id);
        QTRY_COMPARE(backendSecondary(player), subtitles.at(1).id);
        QCOMPARE(changed.count(), 1);
        // The primary selection is independent of the secondary one.
        QVERIFY(tracksOf(player, MediaTrack::Type::Subtitle).at(0).selected);

        // Hiding subtitles also hides the secondary layer.
        player.setSubtitleVisible(false);
        QTRY_VERIFY(!backendSecondaryVisible(player));
        player.setSubtitleVisible(true);
        QTRY_VERIFY(backendSecondaryVisible(player));

        player.selectSecondarySubtitleTrack(0);
        QTRY_COMPARE(player.secondarySubtitleTrack(), qint64{0});
        QTRY_COMPARE(backendSecondary(player), qint64{0});
        QCOMPARE(changed.count(), 2);
    }

    void rejectsNonSubtitleTrack() {
        MpvController player(true, true);
        player.openFile(QString::fromUtf8(TEST_LANGUAGE_MEDIA_PATH));
        QTRY_VERIFY(player.state().active);
        QTRY_VERIFY(tracksOf(player, MediaTrack::Type::Audio).size() == 2);
        const auto audio = tracksOf(player, MediaTrack::Type::Audio).at(0);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        // Task ids are not unique across track types; use an id no track owns.
        player.selectSecondarySubtitleTrack(9999);
        QTRY_COMPARE(errors.count(), 1);
        QCOMPARE(player.secondarySubtitleTrack(), qint64{0});
        QCOMPARE(backendSecondary(player), qint64{0});
    }

    void ignoresSelectionWhileIdle() {
        MpvController player(true, true);
        player.selectSecondarySubtitleTrack(1);
        QCOMPARE(player.secondarySubtitleTrack(), qint64{0});
    }

    void clearsOnStop() {
        MpvController player(true, true);
        player.openFile(QString::fromUtf8(TEST_LANGUAGE_MEDIA_PATH));
        QTRY_VERIFY(player.state().active);
        QTRY_VERIFY(tracksOf(player, MediaTrack::Type::Subtitle).size() == 2);
        player.setPaused(true);
        player.selectSecondarySubtitleTrack(tracksOf(player, MediaTrack::Type::Subtitle).at(1).id);
        QTRY_VERIFY(player.secondarySubtitleTrack() != 0);
        player.stop();
        QTRY_COMPARE(player.secondarySubtitleTrack(), qint64{0});
    }

    void survivesRendererReset() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(QString::fromUtf8(TEST_LANGUAGE_MEDIA_PATH));
        QTRY_VERIFY(player.state().active);
        QTRY_VERIFY(tracksOf(player, MediaTrack::Type::Subtitle).size() == 2);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        const auto secondary = tracksOf(player, MediaTrack::Type::Subtitle).at(1).id;
        player.selectSecondarySubtitleTrack(secondary);

        for (int round = 0; round < 2; ++round) {
            player.prepareForRendererReset();
            player.resumeAfterRendererReset();
            QTRY_COMPARE(loaded.count(), round + 2);
            QTRY_VERIFY(!player.trackRestorationPending());
            QTRY_COMPARE(player.secondarySubtitleTrack(), secondary);
            QTRY_COMPARE(backendSecondary(player), secondary);
            QVERIFY(player.state().paused);
        }
        QVERIFY(errors.isEmpty());
    }

    void usesExternalSubtitleAsSecondary() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.tracks().size() == 3);
        player.setPaused(true);
        player.addSubtitle(QString::fromUtf8(TEST_EXTERNAL_SUBTITLE_PATH));
        QTRY_COMPARE(player.tracks().size(), 4);
        auto externalId = qint64{0};
        for (const auto &track : player.tracks()) {
            if (track.external)
                externalId = track.id;
        }
        QVERIFY(externalId > 0);
        // mpv rejects using the primary track as its own secondary layer.
        player.selectSecondarySubtitleTrack(externalId);
        QTRY_COMPARE(errors.count(), 1);
        QCOMPARE(player.secondarySubtitleTrack(), qint64{0});
        auto internalId = qint64{0};
        for (const auto &track : player.tracks()) {
            if (track.type == MediaTrack::Type::Subtitle && !track.external)
                internalId = track.id;
        }
        QVERIFY(internalId > 0);
        player.selectSecondarySubtitleTrack(internalId);
        QTRY_COMPARE(player.secondarySubtitleTrack(), internalId);
        QTRY_COMPARE(backendSecondary(player), internalId);

        player.prepareForRendererReset();
        player.resumeAfterRendererReset();
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_VERIFY(!player.trackRestorationPending());
        QTRY_COMPARE(player.tracks().size(), 4);
        auto restoredId = qint64{0};
        for (const auto &track : player.tracks()) {
            if (track.external)
                restoredId = track.id;
        }
        QVERIFY(restoredId > 0);
        QTRY_COMPARE(player.secondarySubtitleTrack(),
                     restoredId == externalId ? internalId : restoredId);
        QTRY_COMPARE(backendSecondary(player), player.secondarySubtitleTrack());
        QCOMPARE(errors.count(), 1);
    }

    void dropsMissingSecondaryAfterReload() {
        MpvController player(true, true);
        player.openFile(QString::fromUtf8(TEST_LANGUAGE_MEDIA_PATH));
        QTRY_VERIFY(tracksOf(player, MediaTrack::Type::Subtitle).size() == 2);
        player.setPaused(true);
        player.selectSecondarySubtitleTrack(tracksOf(player, MediaTrack::Type::Subtitle).at(1).id);
        QTRY_VERIFY(player.secondarySubtitleTrack() != 0);
        // A different file without that track must not inherit the selection.
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active);
        QTRY_VERIFY(player.tracks().size() == 3);
        QCOMPARE(player.secondarySubtitleTrack(), qint64{0});
        QCOMPARE(backendSecondary(player), qint64{0});
    }
};

QTEST_MAIN(SecondarySubtitlesTest)
#include "secondary_subtitles_test.moc"
