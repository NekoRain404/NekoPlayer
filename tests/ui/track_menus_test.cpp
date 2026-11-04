#include "playback/media/mediatrack.h"
#include "playback/mpv/mpvcontroller.h"
#include "ui/menus/trackmenus.h"

#include <QAction>
#include <QMenu>
#include <QtTest>

class TrackMenusTest final : public QObject {
    Q_OBJECT
  private slots:
    void secondarySubtitleSubmenuSelectsAndClears() {
        MpvController player(true, true);
        QWidget window;
        QMenu delayMenu;
        QMenu displayMenu;
        TrackMenus menus(player, &window, &delayMenu, &displayMenu);
        player.openFile(QString::fromUtf8(TEST_LANGUAGE_MEDIA_PATH));
        QTRY_VERIFY(player.state().active);
        QTRY_VERIFY(player.tracks().size() == 4);

        auto *menu = window.findChild<QMenu *>(QStringLiteral("secondarySubtitleTracksMenu"));
        QVERIFY(menu);
        QTRY_COMPARE(menu->actions().size(), 3);
        qint64 secondaryId = 0;
        for (const auto &track : player.tracks()) {
            if (track.type == MediaTrack::Type::Subtitle && track.id > secondaryId)
                secondaryId = track.id;
        }
        QVERIFY(secondaryId > 0);
        QAction *chosen = nullptr;
        for (auto *action : menu->actions()) {
            if (action->data().toLongLong() == secondaryId)
                chosen = action;
        }
        QVERIFY(chosen && chosen->isCheckable());
        chosen->trigger();
        QTRY_COMPARE(player.secondarySubtitleTrack(), secondaryId);
        QVERIFY(chosen->isChecked());

        auto *off = window.findChild<QAction *>(QStringLiteral("secondarySubtitleTrack0Action"));
        QVERIFY(off && off->isCheckable() && !off->isChecked());
        off->trigger();
        QTRY_COMPARE(player.secondarySubtitleTrack(), qint64{0});
        QVERIFY(off->isChecked());
        QVERIFY(!chosen->isChecked());
    }
};

QTEST_MAIN(TrackMenusTest)
#include "track_menus_test.moc"
