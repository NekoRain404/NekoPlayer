#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"
#include "playback/videowidget.h"
#include "session/sessionstore.h"
#include "ui/languagemanager.h"
#include "ui/mainwindow.h"
#include "ui/playbacksettingsdialog.h"
#include "ui/timeseekdialog.h"
#include <QAction>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QVariant>
#include <QtTest>

namespace {
QString mediaPath() { return QString::fromUtf8(TEST_MEDIA_PATH); }
bool hasColoredVideo(const QImage &image) {
    if (image.isNull())
        return false;
    int colored = 0;
    for (int y = image.height() / 4; y < image.height() * 3 / 4; y += 8) {
        for (int x = image.width() / 4; x < image.width() * 3 / 4; x += 8) {
            const auto color = image.pixelColor(x, y);
            if (qMax(color.red(), qMax(color.green(), color.blue())) -
                    qMin(color.red(), qMin(color.green(), color.blue())) >
                80)
                ++colored;
        }
    }
    return colored > 40;
}
QAction *action(MainWindow &window, const char *name) {
    return window.findChild<QAction *>(QString::fromLatin1(name));
}
qint64 selectedTrack(MainWindow &window, MediaTrack::Type type) {
    for (const auto &track : window.controller()->tracks()) {
        if (track.type == type && track.selected)
            return track.id;
    }
    return 0;
}
} // namespace

class WindowTest final : public QObject {
    Q_OBJECT
  private slots:
    void playbackSettingsApplyAndCancelPreserveIntent() {
        MpvController player(true, true);
        {
            PlaybackSettingsDialog dialog(player);
            auto *choice = dialog.findChild<QComboBox *>(QStringLiteral("hardwareDecodingChoice"));
            auto *buttons =
                dialog.findChild<QDialogButtonBox *>(QStringLiteral("playbackSettingsButtons"));
            auto *actual = dialog.findChild<QLabel *>(QStringLiteral("actualDecoder"));
            QVERIFY(choice && buttons && actual);
            QVERIFY(choice->currentData().toBool());
            QCOMPARE(actual->text(), QStringLiteral("未播放"));
            QVERIFY(!buttons->button(QDialogButtonBox::Apply)->isEnabled());
            choice->setCurrentIndex(1);
            QVERIFY(buttons->button(QDialogButtonBox::Apply)->isEnabled());
            QVERIFY(player.hardwareDecodingEnabled());
            buttons->button(QDialogButtonBox::Cancel)->click();
            QVERIFY(player.hardwareDecodingEnabled());
        }
        {
            PlaybackSettingsDialog dialog(player);
            auto *choice = dialog.findChild<QComboBox *>(QStringLiteral("hardwareDecodingChoice"));
            auto *buttons =
                dialog.findChild<QDialogButtonBox *>(QStringLiteral("playbackSettingsButtons"));
            QVERIFY(choice && buttons);
            choice->setCurrentIndex(1);
            buttons->button(QDialogButtonBox::Apply)->click();
            QVERIFY(!player.hardwareDecodingEnabled());
            QVERIFY(!buttons->button(QDialogButtonBox::Apply)->isEnabled());
            choice->setCurrentIndex(0);
            buttons->button(QDialogButtonBox::Cancel)->click();
            QVERIFY(!player.hardwareDecodingEnabled());
        }
        {
            PlaybackSettingsDialog dialog(player);
            auto *choice = dialog.findChild<QComboBox *>(QStringLiteral("hardwareDecodingChoice"));
            auto *buttons =
                dialog.findChild<QDialogButtonBox *>(QStringLiteral("playbackSettingsButtons"));
            QVERIFY(choice && buttons);
            QVERIFY(!choice->currentData().toBool());
            choice->setCurrentIndex(0);
            buttons->button(QDialogButtonBox::Ok)->click();
            QVERIFY(player.hardwareDecodingEnabled());
            QCOMPARE(dialog.result(), QDialog::Accepted);
        }
        MainWindow window(true);
        auto *settings = action(window, "playbackSettingsAction");
        QVERIFY(settings);
        QCOMPARE(settings->shortcut(), QKeySequence(Qt::Key_F5));
        int containers = 0;
        for (auto *menu : window.findChildren<QMenu *>()) {
            if (menu->actions().contains(settings))
                ++containers;
        }
        QCOMPARE(containers, 2); // Menu bar and context menu share the same action.
    }

    void settingsMenuRetranslatesWithoutChangingActionState() {
        const auto previousConfiguration = qApp->property("configurationEnabled");
        const auto previousLanguageManager = qApp->property("languageManager");
        qApp->setProperty("configurationEnabled", true);
        LanguageManager language(*qApp);
        qApp->setProperty("languageManager",
                          QVariant::fromValue(static_cast<QObject *>(&language)));

        MainWindow window(true);
        auto *themeMenuAction = action(window, "themeMenuAction");
        auto *playbackSettings = action(window, "playbackSettingsAction");
        auto *subtitleSettings = action(window, "subtitleSettingsAction");
        auto *about = action(window, "aboutAction");
        QVERIFY(themeMenuAction && themeMenuAction->menu() && playbackSettings &&
                subtitleSettings && about);
        const auto shortcut = playbackSettings->shortcut();
        const auto checkedTheme = themeMenuAction->menu()->actions().at(0)->isChecked();
        QCOMPARE(themeMenuAction->menu()->actions().size(), 3);

        QVERIFY(language.setLanguage(QStringLiteral("en")));
        QCoreApplication::processEvents();
        QCOMPARE(themeMenuAction->text(), QStringLiteral("Theme"));
        QCOMPARE(themeMenuAction->menu()->actions().at(0)->text(), QStringLiteral("Dark"));
        QCOMPARE(themeMenuAction->menu()->actions().at(1)->text(), QStringLiteral("Light"));
        QCOMPARE(themeMenuAction->menu()->actions().at(2)->text(), QStringLiteral("Follow System"));
        QCOMPARE(playbackSettings->text(), QStringLiteral("Playback Settings…"));
        QCOMPARE(about->text(), QStringLiteral("About NekoPlayer…"));
        QCOMPARE(playbackSettings->shortcut(), shortcut);
        QCOMPARE(themeMenuAction->menu()->actions().at(0)->isChecked(), checkedTheme);

        QVERIFY(language.setLanguage(QStringLiteral("zh_CN")));
        QCoreApplication::processEvents();
        QCOMPARE(themeMenuAction->text(), QStringLiteral("主题"));
        QCOMPARE(playbackSettings->text(), QStringLiteral("播放设置…"));
        QCOMPARE(about->text(), QStringLiteral("关于 NekoPlayer…"));
        qApp->setProperty("languageManager", previousLanguageManager);
        qApp->setProperty("configurationEnabled", previousConfiguration);
    }

    void miniPlayerTogglesWindowPresentation() {
        MainWindow window(true);
        auto *mini = action(window, "miniPlayerAction");
        QVERIFY(mini);
        QCOMPARE(mini->shortcut(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
        QVERIFY(!mini->isChecked());

        const auto normalSize = window.size();
        mini->trigger();
        QVERIFY(mini->isChecked());
        QCOMPARE(mini->text(), QStringLiteral("退出小窗"));
        QVERIFY(window.menuBar()->isHidden());
        QVERIFY(window.statusBar()->isHidden());
        QVERIFY(window.windowFlags().testFlag(Qt::Tool));
        QVERIFY(window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QVERIFY(!window.findChild<QWidget *>(QStringLiteral("playlistPane"))->isVisible());

        mini->trigger();
        QVERIFY(!mini->isChecked());
        QCOMPARE(mini->text(), QStringLiteral("小窗播放"));
        QVERIFY(!window.menuBar()->isHidden());
        QVERIFY(!window.statusBar()->isHidden());
        QVERIFY(!window.windowFlags().testFlag(Qt::Tool));
        QVERIFY(!window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QCOMPARE(window.size(), normalSize);
    }

    void closingMiniPlayerPersistsNormalWindowGeometry() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto sessionPath = temporary.filePath(QStringLiteral("mini-session.ini"));
        MainWindow window(true, sessionPath);
        window.resize(920, 560);
        const auto normalGeometry = window.saveGeometry();
        auto *mini = action(window, "miniPlayerAction");
        QVERIFY(mini);
        mini->trigger();
        window.close();

        QCOMPARE(SessionStore(sessionPath).load().state.window.geometry, normalGeometry);
    }

    void playlistWidthTracksLiveResizeAndTransitions() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto sessionPath = temporary.filePath(QStringLiteral("playlist-width.ini"));
        SessionState saved;
        saved.window.playlistVisible = true;
        saved.window.playlistWidth = 400;
        QVERIFY(SessionStore(sessionPath).save(saved).isEmpty());
        {
            MainWindow window(true, sessionPath);
            auto *splitter = window.findChild<QSplitter *>(QStringLiteral("bodySplitter"));
            auto *pane = window.findChild<QWidget *>(QStringLiteral("playlistPane"));
            auto *showPlaylist = action(window, "showPlaylistAction");
            QVERIFY(splitter && pane && showPlaylist);
            window.show();
            QTRY_VERIFY(showPlaylist->isChecked());
            QTRY_COMPARE(pane->width(), 400);

            // A user drag updates the stored width through splitterMoved.
            splitter->setSizes({500, 600});
            emit splitter->splitterMoved(600, 1);
            QCOMPARE(pane->width(), 600);

            showPlaylist->trigger();
            QVERIFY(!pane->isVisible());
            showPlaylist->trigger();
            QVERIFY(pane->isVisible());
            QTRY_COMPARE(pane->width(), 600);

            action(window, "fullscreenAction")->trigger();
            QTRY_VERIFY(window.isFullScreen());
            action(window, "fullscreenAction")->trigger();
            QTRY_VERIFY(!window.isFullScreen());
            QTRY_COMPARE(pane->width(), 600);

            action(window, "miniPlayerAction")->trigger();
            QVERIFY(!pane->isVisible());
            action(window, "miniPlayerAction")->trigger();
            QVERIFY(pane->isVisible());
            QTRY_COMPARE(pane->width(), 600);

            // Hiding before close must retain the live width captured at hide time.
            showPlaylist->trigger();
            QVERIFY(!pane->isVisible());
            window.close();
        }
        QCOMPARE(SessionStore(sessionPath).load().state.window.playlistWidth, 600);
    }

    void chapterActionsFollowSelectionAndKeepPause() {
        MainWindow window(true);
        auto *jump = action(window, "seekTimeAction");
        auto *previous = action(window, "previousChapterAction");
        auto *next = action(window, "nextChapterAction");
        auto *menu = window.findChild<QMenu *>(QStringLiteral("chaptersMenu"));
        QVERIFY(jump && previous && next && menu);
        QVERIFY(!jump->isEnabled() && !previous->isEnabled() && !next->isEnabled());
        window.show();
        QTRY_VERIFY(window.videoWidget()->isReady());
        window.openFile(QString::fromUtf8(TEST_CHAPTER_MEDIA_PATH));
        QTRY_VERIFY(window.controller()->state().active && window.controller()->state().seekable);
        window.controller()->setPaused(true);
        QTRY_VERIFY(window.controller()->state().paused);
        QTRY_COMPARE(menu->actions().size(), 3);
        QVERIFY(jump->isEnabled());
        QTRY_COMPARE(window.controller()->state().currentChapter, 0);
        next->trigger();
        QVERIFY(previous->isEnabled());
        previous->trigger();
        QCOMPARE(window.controller()->chapterNavigationIndex(), 0);
        next->trigger();
        next->trigger();
        QVERIFY(!next->isEnabled() && previous->isEnabled());
        previous->trigger();
        QVERIFY(next->isEnabled());
        next->trigger();
        QTRY_COMPARE(window.controller()->state().currentChapter, 2);
        auto *last = action(window, "chapterAction2");
        QVERIFY(last);
        last->trigger();
        QTRY_COMPARE(window.controller()->state().currentChapter, 2);
        QVERIFY(last->isChecked() && previous->isEnabled() && !next->isEnabled());
        QVERIFY(window.controller()->state().paused);
        previous->trigger();
        QTRY_COMPARE(window.controller()->state().currentChapter, 1);
        QVERIFY(action(window, "chapterAction1")->isChecked());
        QVERIFY(next->isEnabled() && window.controller()->state().paused);
        window.controller()->stop();
        QTRY_VERIFY(!window.controller()->state().active);
        QVERIFY(!jump->isEnabled() && !menu->isEnabled());
    }

    void timeDialogRejectsMalformedAndOutOfRangeTargets() {
        TimeSeekDialog dialog(1.25, 4.0);
        auto *input = dialog.findChild<QLineEdit *>(QStringLiteral("timeSeekInput"));
        auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("timeSeekButtons"));
        QVERIFY(input && buttons);
        auto *accept = buttons->button(QDialogButtonBox::Ok);
        QVERIFY(accept->isEnabled());
        QCOMPARE(dialog.selectedSeconds().value(), 1.25);
        input->setText(QStringLiteral("4.001"));
        QVERIFY(!accept->isEnabled());
        input->setText(QStringLiteral("1:60"));
        QVERIFY(!accept->isEnabled());
        input->setText(QStringLiteral("00:00:02.500"));
        QVERIFY(accept->isEnabled());
        QCOMPARE(dialog.selectedSeconds().value(), 2.5);
    }

    void playlistModeActionsShareStateAndRefreshNavigation() {
        MainWindow window(true);
        auto *off = action(window, "repeatOffAction");
        auto *one = action(window, "repeatOneAction");
        auto *all = action(window, "repeatAllAction");
        auto *cycle = action(window, "cycleRepeatAction");
        auto *menu = window.findChild<QMenu *>(QStringLiteral("playlistModeMenu"));
        QVERIFY(off && one && all && cycle && menu);
        QVERIFY(off->isChecked());
        window.show();
        QTRY_VERIFY(window.videoWidget()->isReady());
        window.openFile(mediaPath());
        QTRY_VERIFY(window.controller()->state().active);
        window.controller()->setPaused(true);
        QTRY_VERIFY(window.controller()->state().paused);
        QVERIFY(!action(window, "nextAction")->isEnabled());
        all->trigger();
        QCOMPARE(window.playlist()->repeatMode(), PlaylistController::RepeatMode::All);
        QVERIFY(all->isChecked() && !off->isChecked() && !one->isChecked());
        QVERIFY(action(window, "nextAction")->isEnabled());
        QVERIFY(action(window, "previousAction")->isEnabled());
        QVERIFY(menu->title().contains(all->text()));
        cycle->trigger();
        QCOMPARE(window.playlist()->repeatMode(), PlaylistController::RepeatMode::Off);
        QVERIFY(off->isChecked());
        QVERIFY(!action(window, "nextAction")->isEnabled());
        window.playlist()->setRepeatMode(PlaylistController::RepeatMode::One);
        QVERIFY(one->isChecked());
        QVERIFY(window.controller()->state().paused);
    }

    void tuningActionsFollowBackendAndSurviveTrackRebuilds() {
        MainWindow window(true);
        auto *speed = window.findChild<QMenu *>(QStringLiteral("speedMenu"));
        auto *audioDelay = window.findChild<QMenu *>(QStringLiteral("audioDelayMenu"));
        auto *subtitleDelay = window.findChild<QMenu *>(QStringLiteral("subtitleDelayMenu"));
        QVERIFY(speed && audioDelay && subtitleDelay);
        auto *faster = action(window, "speedUpAction");
        auto *audioLater = action(window, "audioLaterAction");
        auto *subtitleEarlier = action(window, "subtitleEarlierAction");
        QVERIFY(faster && audioLater && subtitleEarlier);
        faster->trigger();
        faster->trigger();
        QTRY_VERIFY(qAbs(window.controller()->state().playbackSpeed - 1.2) < 0.0001);
        QTRY_VERIFY(speed->title().contains(QStringLiteral("1.20")));
        action(window, "speedResetAction")->trigger();
        QTRY_VERIFY(qAbs(window.controller()->state().playbackSpeed - (1.0)) < 0.000001);
        window.show();
        QTRY_VERIFY(window.videoWidget()->isReady());
        window.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(window.controller()->state().active);
        window.controller()->setPaused(true);
        QTRY_VERIFY(window.controller()->state().paused);
        audioLater->trigger();
        subtitleEarlier->trigger();
        QTRY_VERIFY(qAbs(window.controller()->state().audioDelay - (0.05)) < 0.000001);
        QTRY_VERIFY(qAbs(window.controller()->state().subtitleDelay - (-0.1)) < 0.000001);
        action(window, "subtitleTrack0Action")->trigger();
        QTRY_COMPARE(selectedTrack(window, MediaTrack::Type::Subtitle), 0);
        auto *subtitles = window.findChild<QMenu *>(QStringLiteral("subtitleTracksMenu"));
        QVERIFY(subtitles->actions().contains(subtitleDelay->menuAction()));
        QVERIFY(subtitleEarlier->isEnabled());
        QVERIFY(window.controller()->state().paused);
        action(window, "audioDelayResetAction")->trigger();
        action(window, "subtitleDelayResetAction")->trigger();
        QTRY_VERIFY(qAbs(window.controller()->state().audioDelay - (0.0)) < 0.000001);
        QTRY_VERIFY(qAbs(window.controller()->state().subtitleDelay - (0.0)) < 0.000001);
    }

    void sessionRestoresWithoutAutoplayAndSavesOnClose() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("settings.ini"));
        SessionState saved;
        saved.paths = {mediaPath(), mediaPath()};
        saved.currentIndex = 1;
        saved.audio = {34.0, true};
        saved.resumePosition = 1.24;
        saved.window.playlistVisible = true;
        QVERIFY(SessionStore(file).save(saved).isEmpty());
        {
            MainWindow window(true, file);
            QCOMPARE(window.playlist()->model()->rowCount(), 2);
            QCOMPARE(window.playlist()->model()->currentIndex(), 1);
            QVERIFY(!action(window, "pauseAction")->isEnabled());
            QCOMPARE(window.playlist()->pendingResumePosition(), 1.24);
            QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("上次位置")));
            QVERIFY(action(window, "showPlaylistAction")->isChecked());
            window.show();
            QTRY_VERIFY(window.videoWidget()->isReady());
            QTRY_VERIFY(action(window, "pauseAction")->isEnabled());
            QVERIFY(!window.controller()->state().active);
            window.controller()->setVolume(52);
            window.controller()->setMuted(false);
            window.resize(920, 540);
            const auto normalGeometry = window.saveGeometry();
            action(window, "fullscreenAction")->trigger();
            QTRY_VERIFY(window.isFullScreen());
            window.close();
            const auto loaded = SessionStore(file).load();
            QCOMPARE(loaded.state.window.geometry, normalGeometry);
            QCOMPARE(loaded.state.audio.volume, 52.0);
            QCOMPARE(loaded.state.resumePosition, 1.24);
            QVERIFY(!loaded.state.audio.muted);
        }
        MainWindow replaced(true, file);
        replaced.openFiles({mediaPath()}); // Also save files queued before renderer initialization.
        replaced.close();
        const auto loaded = SessionStore(file).load();
        QCOMPARE(loaded.state.paths, QStringList{mediaPath()});
        QCOMPARE(loaded.state.currentIndex, 0);
        QCOMPARE(loaded.state.resumePosition, 0.0);
        MainWindow cleared(true, file);
        cleared.appendFiles({mediaPath()});
        QVERIFY(action(cleared, "clearPlaylistAction")->isEnabled());
        action(cleared, "clearPlaylistAction")->trigger();
        cleared.close();
        QVERIFY(SessionStore(file).load().state.paths.isEmpty());
    }

    void trackMenuActionsFollowBackend() {
        MainWindow window(true);
        auto *audio = window.findChild<QMenu *>(QStringLiteral("audioTracksMenu"));
        auto *subtitles = window.findChild<QMenu *>(QStringLiteral("subtitleTracksMenu"));
        auto *load = action(window, "loadSubtitleAction");
        QVERIFY(audio && subtitles && load);
        QVERIFY(!audio->isEnabled());
        QVERIFY(!subtitles->isEnabled());
        QVERIFY(!load->isEnabled());
        QVERIFY(window.menuBar()->actions().contains(subtitles->menuAction()));
        auto *display = window.findChild<QMenu *>(QStringLiteral("subtitleDisplayMenu"));
        auto *settings = action(window, "subtitleSettingsAction");
        QVERIFY(display && settings && settings->isEnabled());
        QVERIFY(subtitles->actions().contains(display->menuAction()));
        QCOMPARE(settings->shortcut(), QKeySequence(Qt::Key_F6));
        int audioMenuUses = 0;
        const auto menus = window.findChildren<QMenu *>();
        for (auto *menu : menus) {
            if (menu->actions().contains(audio->menuAction()))
                ++audioMenuUses;
        }
        QCOMPARE(audioMenuUses, 2); // Menu bar and context menu share the same submenu.
        window.show();
        QTRY_VERIFY(window.videoWidget()->isReady());
        window.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(window.controller()->state().active);
        window.controller()->setPaused(true);
        QTRY_VERIFY(action(window, "audioTrack2Action"));
        QVERIFY(load->isEnabled());
        QVERIFY(audio->isEnabled());
        action(window, "audioTrack2Action")->trigger();
        QTRY_COMPARE(selectedTrack(window, MediaTrack::Type::Audio), 2);
        QTRY_VERIFY(action(window, "audioTrack2Action")->isChecked());
        action(window, "subtitleTrack1Action")->trigger();
        QTRY_COMPARE(selectedTrack(window, MediaTrack::Type::Subtitle), 1);
        QTRY_VERIFY(action(window, "subtitleTrack1Action")->isChecked());
        action(window, "subtitleTrack0Action")->trigger();
        QTRY_COMPARE(selectedTrack(window, MediaTrack::Type::Subtitle), 0);
        QTRY_VERIFY(action(window, "subtitleTrack0Action")->isChecked());
        window.controller()->stop();
        QTRY_VERIFY(!load->isEnabled());
        QTRY_VERIFY(!action(window, "audioTrack2Action"));
        QVERIFY(!audio->isEnabled());
        QVERIFY(!subtitles->isEnabled());
        QVERIFY(settings->isEnabled());
        QVERIFY(subtitles->actions().contains(display->menuAction()));
    }

    void rendersVideo() {
        MainWindow window(true);
        window.show();
        // Wayland compositors can suspend an unfocused window without exposing
        // it. Renderer readiness and actual framebuffer pixels are the contract.
        QTRY_VERIFY_WITH_TIMEOUT(window.videoWidget()->isReady(), 3000);
        window.openFile(mediaPath());
        QTRY_VERIFY(window.controller()->state().active);
        QTRY_VERIFY_WITH_TIMEOUT(hasColoredVideo(window.videoWidget()->grabFramebuffer()), 5000);
        const auto frame = window.videoWidget()->grabFramebuffer();
        const QSize expectedFramebufferSize(
            qRound(window.videoWidget()->width() * window.videoWidget()->devicePixelRatioF()),
            qRound(window.videoWidget()->height() * window.videoWidget()->devicePixelRatioF()));
        QCOMPARE(frame.size(), expectedFramebufferSize);
        auto *pause = action(window, "pauseAction");
        QVERIFY(pause);
        pause->trigger();
        QTRY_VERIFY(window.controller()->state().paused);
        auto *timeline = window.findChild<QSlider *>(QStringLiteral("timelineSlider"));
        QVERIFY(timeline && timeline->isEnabled());
        timeline->setValue(5000);
        QVERIFY(QMetaObject::invokeMethod(timeline, "sliderReleased", Qt::DirectConnection));
        QTRY_VERIFY(qAbs(window.controller()->state().position - 2.0) < 0.3);
        timeline->setSliderDown(true);
        timeline->setSliderPosition(7500);
        auto *time = window.findChild<QLabel *>(QStringLiteral("timeLabel"));
        QVERIFY(time);
        QTRY_VERIFY(time->text().startsWith(QStringLiteral("00:00:03")));
        timeline->setSliderDown(false);
        window.resize(800, 520);
        QTRY_VERIFY(hasColoredVideo(window.videoWidget()->grabFramebuffer()));
        const auto size = window.size();
        const auto verifyFrameGeometry = [&window] {
            QTRY_VERIFY_WITH_TIMEOUT(hasColoredVideo(window.videoWidget()->grabFramebuffer()),
                                     3000);
            const auto frame = window.videoWidget()->grabFramebuffer();
            const QSize expected(
                qRound(window.videoWidget()->width() * window.videoWidget()->devicePixelRatioF()),
                qRound(window.videoWidget()->height() * window.videoWidget()->devicePixelRatioF()));
            QCOMPARE(frame.size(), expected);
        };
        action(window, "fullscreenAction")->trigger();
        QTRY_VERIFY(window.isFullScreen());
        verifyFrameGeometry();
        // The compositor may suspend an unfocused test surface, so shortcut
        // dispatch cannot be exercised here. Test the shared QAction instead.
        action(window, "exitFullscreenAction")->trigger();
        QTRY_VERIFY(!window.isFullScreen());
        QTRY_COMPARE(window.size(), size);
        verifyFrameGeometry();
        QTest::mouseDClick(window.videoWidget(), Qt::LeftButton);
        QTRY_VERIFY(window.isFullScreen());
        verifyFrameGeometry();
        action(window, "fullscreenAction")->trigger();
        QTRY_VERIFY(!window.isFullScreen());
        verifyFrameGeometry();
        pause->trigger();
        QTRY_VERIFY(!window.controller()->state().paused);
        window.close();
    }

    void appendsLocalDropsAndRecoversFromInvalidInput() {
        MainWindow window(true);
        window.show();
        QTRY_VERIFY(window.videoWidget()->isReady());
        QSignalSpy errors(&window, &MainWindow::userError);
        const auto drop = [&window](const QList<QUrl> &urls) {
            QMimeData mime;
            mime.setUrls(urls);
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
            QDragEnterEvent enter(QPointF(50, 50), Qt::CopyAction, &mime,
#else
            QDragEnterEvent enter(QPoint(50, 50), Qt::CopyAction, &mime,
#endif
                                  Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(&window, &enter);
            QDropEvent event(QPointF(50, 50), Qt::CopyAction, &mime, Qt::LeftButton,
                             Qt::NoModifier);
            QApplication::sendEvent(&window, &event);
        };
        window.openFile(mediaPath());
        QTRY_VERIFY(window.controller()->state().active);
        drop({QUrl::fromLocalFile(mediaPath()), QUrl::fromLocalFile(mediaPath())});
        QCOMPARE(window.playlist()->model()->rowCount(), 3);
        QVERIFY(errors.isEmpty());
        QVERIFY(window.controller()->state().active);
        drop({QUrl(QStringLiteral("https://example.com/movie.mkv"))});
        QTRY_COMPARE(errors.count(), 1);
        action(window, "fullscreenAction")->trigger();
        QTRY_VERIFY(window.isFullScreen());
        window.openFile(QStringLiteral("/no-such-neko-file.mkv"));
        QTRY_COMPARE(errors.count(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(!window.statusBar()->currentMessage().isEmpty(), 3000);
        QTRY_VERIFY(!window.controller()->state().active);
        drop({QUrl::fromLocalFile(mediaPath())});
        QTRY_VERIFY(window.controller()->state().active);
    }

    void pausedAppendUpdatesNavigationImmediately() {
        MainWindow window(true);
        window.show();
        window.openFile(mediaPath());
        QTRY_VERIFY(window.controller()->state().active);
        window.controller()->setPaused(true);
        QTRY_VERIFY(window.controller()->state().paused);
        QVERIFY(!action(window, "nextAction")->isEnabled());
        window.appendFiles({mediaPath()});
        // No mpv property event is needed to publish a queue-only change.
        QVERIFY(action(window, "nextAction")->isEnabled());
        QVERIFY(window.controller()->state().paused);
        QCOMPARE(window.playlist()->model()->currentIndex(), 0);
        action(window, "clearPlaylistAction")->trigger();
        QVERIFY(!action(window, "clearPlaylistAction")->isEnabled());
        QVERIFY(!action(window, "nextAction")->isEnabled());
    }

    void commandLineMultipleFilesStartsPlaylist() {
        QProcess process;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("QT_QPA_PLATFORMTHEME"), QStringLiteral("generic"));
        process.setProcessEnvironment(environment);
        process.start(QString::fromUtf8(PLAYER_EXECUTABLE),
                      {QStringLiteral("--silent-audio"), QStringLiteral("--smoke-test"),
                       mediaPath(), mediaPath()});
        QVERIFY(process.waitForStarted());
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
    }

    void queuedOpenAndClose() {
        for (int i = 0; i < 2; ++i) {
            MainWindow window(true);
            window.openFile(mediaPath());
            window.show();
            QTRY_VERIFY(window.controller()->state().active);
            window.controller()->setPaused(true);
            QTRY_VERIFY(window.controller()->state().paused);
            window.hide();
            QTest::qWait(120);
            window.show();
            QTRY_VERIFY(hasColoredVideo(window.videoWidget()->grabFramebuffer()));
        }
    }

    void recreatesGraphicsContext_data() {
        QTest::addColumn<QString>("path");
        QTest::addColumn<bool>("paused");
        QTest::newRow("audio-paused") << mediaPath() << true;
        QTest::newRow("audio-playing") << mediaPath() << false;
        QTest::newRow("video-only") << QString::fromUtf8(TEST_VIDEO_ONLY_PATH) << true;
    }

    void recreatesGraphicsContext() {
        QFETCH(QString, path);
        QFETCH(bool, paused);
        MainWindow window(true);
        QSignalSpy errors(&window, &MainWindow::userError);
        window.show();
        window.openFile(path);
        QTRY_VERIFY(window.controller()->state().active && window.controller()->state().seekable);
        window.controller()->setPaused(true);
        QTRY_VERIFY(window.controller()->state().paused);
        window.controller()->seekAbsolute(1.5);
        QTRY_VERIFY(qAbs(window.controller()->state().position - 1.5) < 0.2);
        QTRY_VERIFY(hasColoredVideo(window.videoWidget()->grabFramebuffer()));
        if (!paused) {
            window.controller()->setPaused(false);
            QTRY_VERIFY(!window.controller()->state().paused);
        }
        QSignalSpy recreated(window.videoWidget(), &VideoWidget::ready);
        QSignalSpy loaded(window.controller(), &MpvController::fileLoaded);
        QWidget temporaryHost;
        auto *content = window.takeCentralWidget();
        content->setParent(&temporaryHost);
        temporaryHost.resize(900, 600);
        content->resize(900, 600);
        temporaryHost.show();
        content->show();
        QTRY_VERIFY(!recreated.isEmpty());
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(window.controller()->state().active);
        QTRY_COMPARE(window.controller()->state().paused, paused);
        QTRY_VERIFY(window.controller()->state().position >= 1.3);
        QTRY_VERIFY(hasColoredVideo(window.videoWidget()->grabFramebuffer()));
        QVERIFY2(errors.isEmpty(),
                 qPrintable(errors.isEmpty() ? QString() : errors.front().front().toString()));
        window.setCentralWidget(content);
        content->show();
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_VERIFY(window.videoWidget()->isReady());
        QTRY_VERIFY(window.controller()->state().active);
        QTRY_COMPARE(window.controller()->state().paused, paused);
        QTRY_VERIFY(window.controller()->state().position >= 1.3);
        QTRY_VERIFY(hasColoredVideo(window.videoWidget()->grabFramebuffer()));
        QVERIFY2(errors.isEmpty(),
                 qPrintable(errors.isEmpty() ? QString() : errors.front().front().toString()));
    }

    void reportsMissingGraphicsContext() {
        QProcess process;
        auto environment = QProcessEnvironment::systemEnvironment();
        // Use Qt's minimal platform so this test is independent of whether the
        // parent test process is running on native Wayland or X11/XWayland.
        // The minimal plugin cannot provide the OpenGL surface required by
        // QOpenGLWidget, which exercises the application's failure message.
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("minimal"));
        environment.insert(QStringLiteral("QT_QPA_PLATFORMTHEME"), QStringLiteral("generic"));
        environment.insert(QStringLiteral("QT_OPENGL"), QStringLiteral("software"));
        process.setProcessEnvironment(environment);
        process.start(
            QString::fromUtf8(PLAYER_EXECUTABLE),
            {QStringLiteral("--silent-audio"), QStringLiteral("--smoke-test"), mediaPath()});
        QVERIFY(process.waitForStarted());
        QVERIFY(process.waitForFinished(6000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 1);
        const auto output = QString::fromUtf8(process.readAllStandardError());
        QVERIFY2(output.contains(QStringLiteral("OpenGL 初始化失败")), qPrintable(output));
    }
};
QTEST_MAIN(WindowTest)
#include "window_test.moc"
