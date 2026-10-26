#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"
#include "playback/videowidget.h"
#include "session/playbackhistorystore.h"
#include "ui/actionfeedbackoverlay.h"
#include "ui/appearanceicons.h"
#include "ui/appearancepreferences.h"
#include "ui/appearancestylesheet.h"
#include "ui/appearancetokens.h"
#include "ui/fullscreencontrols.h"
#include "ui/playlistpane.h"
#include "ui/transportbar.h"
#include "ui/welcomepanel.h"

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QDialog>
#include <QGraphicsOpacityEffect>
#include <QImage>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QSignalSpy>
#include <QTranslator>
#include <QWheelEvent>
#include <QtTest>

class UiSurfaceTest final : public QObject {
    Q_OBJECT
  private slots:
    void stylesheetUsesTheUnifiedSurfaceContract() {
        const auto tokens = appearanceTokens(AppearancePreferences::Theme::Dark,
                                             AppearancePreferences::Accent::Rose);
        const auto style =
            appearanceStyleSheet(tokens, true, glassOpacityDefault, glassBlurDefault);
        QVERIFY(style.contains(tokens.accentSurface.name(QColor::HexRgb)));
        QVERIFY(style.contains(tokens.focus.name(QColor::HexRgb)));
        QVERIFY(style.contains(tokens.disabledText.name(QColor::HexRgb)));
        QVERIFY(style.contains(QStringLiteral("QPushButton:focus")));
        QVERIFY(style.contains(QStringLiteral("QPushButton:disabled")));
        QVERIFY(style.contains(QStringLiteral("QMenu::item:disabled")));
        QVERIFY(style.contains(QStringLiteral("QToolButton:pressed")));
        QVERIFY(style.contains(QStringLiteral("QToolTip{color:")));
    }

    void glassChangesPanelsButNotTransportMaterial() {
        QAction action;
        TransportBar transport(&action, &action, &action, &action, &action, &action, &action);
        QMenu menu;
        QDialog dialog;
        QList<QColor> opaqueColors;
        for (const bool glass : {false, true}) {
            AppearancePreferences::applySettings(
                *qApp,
                {AppearancePreferences::Theme::Dark, AppearancePreferences::Accent::Ocean, glass});
            int index = 0;
            for (auto *surface : {static_cast<QWidget *>(&transport), static_cast<QWidget *>(&menu),
                                  static_cast<QWidget *>(&dialog)}) {
                surface->resize(400, 120);
                surface->ensurePolished();
                QImage image(surface->size(), QImage::Format_ARGB32_Premultiplied);
                image.fill(Qt::transparent);
                QPainter painter(&image);
                surface->render(&painter, {}, {}, QWidget::DrawWindowBackground);
                painter.end();
                const auto fill = image.pixelColor(5, 5);
                if (!glass)
                    opaqueColors.append(fill);
                else if (index == 0)
                    QCOMPARE(fill, opaqueColors.at(index));
                else
                    QVERIFY(fill != opaqueColors.at(index));
                ++index;
                QVERIFY(!surface->testAttribute(Qt::WA_TranslucentBackground));
                QVERIFY(!surface->graphicsEffect());
            }
        }
        AppearancePreferences::applyTheme(*qApp, AppearancePreferences::Theme::System);
    }

    void glassPreferenceKeepsFullscreenTransportFadeIndependent() {
        QWidget host;
        QVBoxLayout layout(&host);
        QWidget video(&host);
        QAction action;
        TransportBar transport(&action, &action, &action, &action, &action, &action, &action,
                               &host);
        layout.addWidget(&video);
        layout.addWidget(&transport);
        FullscreenControls controls(transport, video, layout);
        auto *opacity = qobject_cast<QGraphicsOpacityEffect *>(transport.graphicsEffect());
        auto *animation = controls.findChild<QPropertyAnimation *>();
        QVERIFY(opacity && animation);
        QCOMPARE(animation->duration(),
                 appearanceTokens(AppearancePreferences::Theme::Dark).motionDurationMs);
        host.show();
        for (const bool glass : {false, true}) {
            AppearancePreferences::applySettings(
                *qApp,
                {AppearancePreferences::Theme::Dark, AppearancePreferences::Accent::Ocean, glass});
            controls.enter();
            QCOMPARE(opacity->opacity(), 0.0);
            QVERIFY(opacity->isEnabled());
            controls.reveal();
            QCOMPARE(animation->endValue().toReal(), 1.0);
            animation->setCurrentTime(animation->duration());
            QCOMPARE(opacity->opacity(), 1.0);
            AppearancePreferences::applySettings(
                *qApp,
                {AppearancePreferences::Theme::Light, AppearancePreferences::Accent::Rose, !glass});
            QCOMPARE(transport.graphicsEffect(), opacity);
            QCOMPARE(opacity->opacity(), 1.0);
            controls.fadeOut();
            QCOMPARE(animation->endValue().toReal(), 0.0);
            animation->setCurrentTime(animation->duration());
            QVERIFY(transport.isHidden());
            QVERIFY(transport.testAttribute(Qt::WA_TransparentForMouseEvents));
            controls.leave();
            QVERIFY(!opacity->isEnabled());
            QCOMPARE(opacity->opacity(), 1.0);
            QCOMPARE(transport.parentWidget(), &host);
        }
        AppearancePreferences::applyTheme(*qApp, AppearancePreferences::Theme::System);
    }

    void sharedIconsRenderAtDevicePixelRatio() {
        QPalette palette;
        palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#70a0ff")));
        const auto play = AppearanceIcons::icon(AppearanceIcons::Symbol::Play, palette);
        QVERIFY(!play.isNull());
        const auto native = play.pixmap(QSize(24, 24));
        QVERIFY(!native.isNull());
        QCOMPARE(native.size(), QSize(24, 24));
        const auto highDpi = play.pixmap(QSize(24, 24), 2.0);
        QVERIFY(!highDpi.isNull());
        QCOMPARE(highDpi.size(), QSize(48, 48));
        QCOMPARE(highDpi.devicePixelRatio(), 2.0);

        const auto media = AppearanceIcons::icon(AppearanceIcons::Symbol::MediaFile, palette);
        const auto mediaImage = media.pixmap(QSize(24, 24)).toImage();
        QVERIFY(!mediaImage.isNull());
        QVERIFY(mediaImage.pixelColor(10, 6).alpha() == 0);

        const auto network = AppearanceIcons::icon(AppearanceIcons::Symbol::Network, palette);
        const auto networkImage = network.pixmap(QSize(24, 24)).toImage();
        QVERIFY(networkImage.pixelColor(12, 7).alpha() == 0);

        const auto settingsImage = AppearanceIcons::icon(AppearanceIcons::Symbol::Settings, palette)
                                       .pixmap(QSize(24, 24))
                                       .toImage();
        QVERIFY(!settingsImage.isNull());
        const auto playImage = native.toImage();
        QVERIFY(settingsImage != playImage);
        QVERIFY(settingsImage.pixelColor(5, 12).alpha() > 0);
        QCOMPARE(playImage.pixelColor(5, 12).alpha(), 0);

        const auto seekBack =
            AppearanceIcons::icon(AppearanceIcons::Symbol::SeekBackward, palette).pixmap(24, 24);
        const auto seekForward =
            AppearanceIcons::icon(AppearanceIcons::Symbol::SeekForward, palette).pixmap(24, 24);
        QVERIFY(seekBack.toImage() != seekForward.toImage());

        const auto disabled = play.pixmap(QSize(24, 24), QIcon::Disabled, QIcon::Off);
        QVERIFY(!disabled.isNull());
        QCOMPARE(disabled.size(), QSize(24, 24));
    }

    void videoSurfaceStartsClickTimerOnReleaseAndIgnoresEmptyWheel() {
        MpvController player(true, true);
        VideoWidget video(player);
        QSignalSpy wheel(&video, &VideoWidget::wheelScrolled);
        QSignalSpy single(&video, &VideoWidget::singleClicked);
        const QPointF local(20, 20);
        const auto global = video.mapToGlobal(local.toPoint());
        const auto sendWheel = [&video, local, global](int deltaY) {
            QWheelEvent event(local, global, QPoint(), QPoint(0, deltaY), Qt::NoButton,
                              Qt::NoModifier, Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(&video, &event);
        };
        sendWheel(0);
        QCOMPARE(wheel.count(), 0);
        sendWheel(120);
        QCOMPARE(wheel.count(), 1);
        QCOMPARE(wheel.constFirst().constFirst().toInt(), 120);
        const auto sendButton = [&video, local, global](QEvent::Type type,
                                                        Qt::MouseButtons buttons) {
            QMouseEvent event(type, local, global, Qt::LeftButton, buttons, Qt::NoModifier);
            QCoreApplication::sendEvent(&video, &event);
        };
        sendButton(QEvent::MouseButtonPress, Qt::LeftButton);
        QTest::qWait(QApplication::doubleClickInterval() + 50);
        QCOMPARE(single.count(), 0);
        sendButton(QEvent::MouseButtonRelease, Qt::NoButton);
        QTRY_COMPARE(single.count(), 1);
        sendButton(QEvent::MouseButtonDblClick, Qt::LeftButton);
        sendButton(QEvent::MouseButtonRelease, Qt::NoButton);
        QTest::qWait(QApplication::doubleClickInterval() + 50);
        QCOMPARE(single.count(), 1);
    }

    void welcomePanelExposesPrimaryAndSecondaryActions() {
        QAction open(QStringLiteral("打开文件…"));
        QAction network(QStringLiteral("打开网络地址…"));
        WelcomePanel panel(&open, &network);

        auto *openButton = panel.findChild<QPushButton *>(QStringLiteral("welcomeOpenButton"));
        auto *networkButton =
            panel.findChild<QPushButton *>(QStringLiteral("welcomeNetworkButton"));
        auto *title = panel.findChild<QLabel *>(QStringLiteral("welcomeTitle"));
        auto *brand = panel.findChild<QLabel *>(QStringLiteral("welcomeBrand"));
        auto *hint = panel.findChild<QLabel *>(QStringLiteral("welcomeDropHint"));
        QVERIFY(openButton && networkButton && title && brand && hint);
        QCOMPARE(openButton->text(), open.text());
        QCOMPARE(networkButton->text(), network.text());
        QVERIFY(!title->text().isEmpty());
        QCOMPARE(brand->text(), QStringLiteral("NEKOPLAYER"));
        QVERIFY(!hint->text().isEmpty());
        QVERIFY(openButton->accessibleName() == open.text());
        QVERIFY(networkButton->accessibleName() == network.text());
        QSignalSpy openSpy(&open, &QAction::triggered);
        openButton->click();
        QCOMPARE(openSpy.size(), 1);
    }

    void welcomePanelShowsRecentMediaAndRequestsPlayback() {
        QAction open(QStringLiteral("打开文件…"));
        QAction network(QStringLiteral("打开网络地址…"));
        WelcomePanel panel(&open, &network);
        QSignalSpy recentSpy(&panel, &WelcomePanel::recentSourceRequested);
        const QStringList sources{QStringLiteral("/media/Film One.mkv"),
                                  QStringLiteral("https://example.test/series/episode.mkv")};

        panel.setRecentSources(sources);

        auto *heading = panel.findChild<QLabel *>(QStringLiteral("welcomeRecentHeading"));
        auto *first = panel.findChild<QPushButton *>(QStringLiteral("welcomeRecentAction1"));
        auto *second = panel.findChild<QPushButton *>(QStringLiteral("welcomeRecentAction2"));
        QVERIFY(heading && first && second);
        QVERIFY(!heading->isHidden());
        QVERIFY(panel.findChild<QLabel *>(QStringLiteral("welcomeRecentTitle1"))
                    ->text()
                    .contains(QStringLiteral("Film One")));
        QVERIFY(panel.findChild<QLabel *>(QStringLiteral("welcomeRecentTitle2"))
                    ->text()
                    .contains(QStringLiteral("episode")));
        QVERIFY(!panel.findChild<QLabel *>(QStringLiteral("welcomeRecentMediaIcon1"))
                     ->pixmap()
                     .isNull());
        QVERIFY(!panel.findChild<QLabel *>(QStringLiteral("welcomeRecentMediaIcon2"))
                     ->pixmap()
                     .isNull());
        QCOMPARE(panel.findChild<QLabel *>(QStringLiteral("welcomeRecentLocation2"))->text(),
                 QStringLiteral("https://example.test/series/episode.mkv"));
        QVERIFY(second->accessibleName().contains(QStringLiteral("episode")));
        QVERIFY(!second->toolTip().contains(QStringLiteral("?")));
        QVERIFY(second->property("welcomeRecentAction").toBool());

        first->click();
        QCOMPARE(recentSpy.size(), 1);
        QCOMPARE(recentSpy.takeFirst().at(0).toString(), sources.constFirst());

        panel.setRecentSources({});
        QVERIFY(!panel.findChild<QPushButton *>(QStringLiteral("welcomeRecentAction1")));
    }

    void welcomeRecentRowsShowTitleAndSanitizedLocation() {
        QAction open(QStringLiteral("打开文件…"));
        QAction network(QStringLiteral("打开网络地址…"));
        WelcomePanel panel(&open, &network);
        const auto source =
            QStringLiteral("https://user:secret@example.test/video/Episode.mkv?token=private");
        panel.setRecentSources({source});

        auto *title = panel.findChild<QLabel *>(QStringLiteral("welcomeRecentTitle1"));
        auto *location = panel.findChild<QLabel *>(QStringLiteral("welcomeRecentLocation1"));
        QVERIFY(title && location);
        QCOMPARE(title->text(), QStringLiteral("Episode.mkv"));
        QCOMPARE(location->text(), QStringLiteral("https://example.test/video/Episode.mkv"));
        QVERIFY(!location->text().contains(QStringLiteral("secret")));
        QVERIFY(!location->text().contains(QStringLiteral("token")));
    }

    void welcomeRecentTextUsesTheSharedSpacingUnit() {
        QAction open(QStringLiteral("打开文件…"));
        QAction network(QStringLiteral("打开网络地址…"));
        WelcomePanel panel(&open, &network);
        panel.resize(640, 520);
        panel.setRecentSources({QStringLiteral("/media/Episode.mkv")});
        panel.show();
        QCoreApplication::processEvents();

        auto *button = panel.findChild<QPushButton *>(QStringLiteral("welcomeRecentAction1"));
        auto *title = panel.findChild<QLabel *>(QStringLiteral("welcomeRecentTitle1"));
        auto *location = panel.findChild<QLabel *>(QStringLiteral("welcomeRecentLocation1"));
        QVERIFY(button && title && location);
        const int titleBottom = title->mapTo(button, QPoint(0, title->height())).y();
        const int locationTop = location->mapTo(button, QPoint(0, 0)).y();
        QCOMPARE(locationTop - titleBottom,
                 appearanceTokens(AppearancePreferences::Theme::Dark).spaceUnit);
    }

    void welcomePanelShowsResumablePlaybackHistory() {
        QAction open(QStringLiteral("打开文件…"));
        QAction network(QStringLiteral("打开网络地址…"));
        WelcomePanel panel(&open, &network);
        QSignalSpy resumeSpy(&panel, &WelcomePanel::resumeSourceRequested);
        const QString path = QStringLiteral("/media/Film One.mkv");
        panel.setResumeEntries({{path, 45.0, 120.0, 100},
                                {QStringLiteral("/media/At End.mkv"), 119.5, 120.0, 99},
                                {QStringLiteral("/media/Unknown Duration.mkv"), 12.0, 0.0, 98},
                                {QStringLiteral("/media/Invalid Position.mkv"), 130.0, 120.0, 97},
                                {QStringLiteral("/media/Second.mkv"), 20.0, 80.0, 96},
                                {QStringLiteral("/media/Third.mkv"), 30.0, 90.0, 95},
                                {QStringLiteral("/media/Fourth.mkv"), 40.0, 100.0, 94}});

        auto *heading = panel.findChild<QLabel *>(QStringLiteral("welcomeContinueHeading"));
        auto *item = panel.findChild<QPushButton *>(QStringLiteral("welcomeContinueAction1"));
        auto *progress =
            panel.findChild<QProgressBar *>(QStringLiteral("welcomeContinueProgress1"));
        QVERIFY(heading && item && progress);
        QVERIFY(!heading->isHidden());
        QCOMPARE(panel.findChild<QLabel *>(QStringLiteral("welcomeContinueTitle1"))->text(),
                 QStringLiteral("Film One.mkv"));
        QCOMPARE(progress->value(), 375);
        QVERIFY(panel.findChild<QPushButton *>(QStringLiteral("welcomeContinueAction2")));
        QVERIFY(panel.findChild<QPushButton *>(QStringLiteral("welcomeContinueAction3")));
        QVERIFY(!panel.findChild<QPushButton *>(QStringLiteral("welcomeContinueAction4")));

        item->click();
        QCOMPARE(resumeSpy.size(), 1);
        QCOMPARE(resumeSpy.takeFirst().at(0).toString(), path);
    }

    void welcomePanelPrioritizesPlaybackActionsWhenShort() {
        QAction open(QStringLiteral("Open File…"));
        QAction network(QStringLiteral("Open Network URL…"));
        WelcomePanel panel(&open, &network);
        panel.setRecentSources({QStringLiteral("/media/one.mkv"), QStringLiteral("/media/two.mkv"),
                                QStringLiteral("/media/three.mkv")});
        panel.resize(360, 190);
        panel.show();
        QCoreApplication::processEvents();

        auto *openButton = panel.findChild<QPushButton *>(QStringLiteral("welcomeOpenButton"));
        auto *networkButton =
            panel.findChild<QPushButton *>(QStringLiteral("welcomeNetworkButton"));
        auto *heading = panel.findChild<QLabel *>(QStringLiteral("welcomeRecentHeading"));
        QVERIFY(openButton && networkButton && heading);
        QVERIFY(!openButton->isHidden());
        QVERIFY(!networkButton->isHidden());
        QVERIFY(heading->isHidden());
    }

    void welcomePanelKeepsDropHintBelowPrimaryActionsWhenResized() {
        QAction open(QStringLiteral("Open File…"));
        QAction network(QStringLiteral("Open Network URL…"));
        WelcomePanel panel(&open, &network);
        panel.setRecentSources({QStringLiteral("/media/one.mkv"), QStringLiteral("/media/two.mkv"),
                                QStringLiteral("/media/three.mkv")});
        panel.setResumeEntries({{QStringLiteral("/media/one.mkv"), 10.0, 100.0, 3},
                                {QStringLiteral("/media/two.mkv"), 20.0, 100.0, 2},
                                {QStringLiteral("/media/three.mkv"), 30.0, 100.0, 1}});
        const QList<QSize> sizes{{236, 215}, {236, 260}, {236, 320}, {236, 380}, {280, 215},
                                 {280, 260}, {280, 320}, {280, 380}, {360, 215}, {360, 260},
                                 {360, 320}, {360, 380}, {400, 215}, {400, 230}, {400, 240},
                                 {400, 250}, {400, 259}, {400, 260}, {400, 261}, {400, 270},
                                 {500, 300}, {500, 320}, {500, 360}, {500, 380}};
        for (const auto &size : sizes) {
            panel.resize(size);
            panel.show();
            QCoreApplication::processEvents();
            panel.layout()->activate();

            const auto *openButton =
                panel.findChild<QPushButton *>(QStringLiteral("welcomeOpenButton"));
            const auto *networkButton =
                panel.findChild<QPushButton *>(QStringLiteral("welcomeNetworkButton"));
            const auto *hint = panel.findChild<QLabel *>(QStringLiteral("welcomeDropHint"));
            QVERIFY(openButton && networkButton && hint);
            if (!hint->isHidden()) {
                const auto buttonsBottom =
                    std::max(openButton->geometry().bottom(), networkButton->geometry().bottom());
                QVERIFY2(hint->geometry().top() > buttonsBottom,
                         qPrintable(QStringLiteral("hint overlaps actions at %1x%2")
                                        .arg(size.width())
                                        .arg(size.height())));
            }
        }
    }

    void welcomePanelKeepsActionsReadableAtNarrowWidth() {
        QAction open(QStringLiteral("Open File…"));
        QAction network(QStringLiteral("Open Network URL…"));
        WelcomePanel panel(&open, &network);
        panel.resize(280, 220);
        panel.show();
        QCoreApplication::processEvents();

        auto *openButton = panel.findChild<QPushButton *>(QStringLiteral("welcomeOpenButton"));
        auto *networkButton =
            panel.findChild<QPushButton *>(QStringLiteral("welcomeNetworkButton"));
        QVERIFY(openButton && networkButton);
        panel.layout()->activate();
        QVERIFY(networkButton->geometry().top() > openButton->geometry().top());
        QVERIFY(openButton->width() >= openButton->sizeHint().width());
        QVERIFY(networkButton->width() >= networkButton->sizeHint().width());
    }

    void welcomePanelKeepsBrandVisibleOnlyWhenThereIsRoom() {
        QAction open(QStringLiteral("Open File…"));
        QAction network(QStringLiteral("Open Network URL…"));
        WelcomePanel panel(&open, &network);
        auto *brand = panel.findChild<QLabel *>(QStringLiteral("welcomeBrand"));
        QVERIFY(brand);

        panel.resize(360, 190);
        panel.show();
        QCoreApplication::processEvents();
        QVERIFY(!brand->isHidden());

        panel.resize(360, 150);
        QCoreApplication::processEvents();
        QVERIFY(brand->isHidden());
    }

    void playlistHeaderShowsLocalizedTitleAndItemCount() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QAction append, previous, next, clear;
        PlaylistPane pane(queue, &append, &previous, &next, &clear);

        auto *title = pane.findChild<QLabel *>(QStringLiteral("playlistTitle"));
        auto *count = pane.findChild<QLabel *>(QStringLiteral("playlistCount"));
        QVERIFY(title && count);
        QVERIFY(!title->text().isEmpty());
        QCOMPARE(count->text(), QStringLiteral("0 项"));

        queue.restoreFiles({QStringLiteral("first.mkv"), QStringLiteral("second.mkv")}, 0);
        QCOMPARE(count->text(), QStringLiteral("2 项"));
        queue.clear();
        QCOMPARE(count->text(), QStringLiteral("0 项"));
    }

    void playlistRowsKeepFilenameEndsAtNarrowWidth() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QAction append, previous, next, clear;
        PlaylistPane pane(queue, &append, &previous, &next, &clear);
        const auto path = QStringLiteral("/media/very-long-season-name/episode-01-final.mkv");
        queue.restoreFiles({path}, 0);

        auto *view = pane.view();
        QVERIFY(view);
        pane.resize(240, 280);
        pane.show();
        QCoreApplication::processEvents();
        QCOMPARE(view->textElideMode(), Qt::ElideMiddle);
        QCOMPARE(queue.model()->index(0).data(Qt::ToolTipRole).toString(), path);
    }

    void playlistSingleClickActivationDefersAndCoexistsWithDoubleClick() {
        MpvController player(true, true);
        PlaylistController queue(player);
        QAction append, previous, next, clear;
        PlaylistPane pane(queue, &append, &previous, &next, &clear);
        queue.restoreFiles({QStringLiteral("first.mkv"), QStringLiteral("second.mkv")}, -1);
        pane.resize(320, 320);
        pane.show();
        QCoreApplication::processEvents();

        auto *view = pane.view();
        const auto target = queue.model()->index(1);
        const auto position = view->visualRect(target).center();
        QCOMPARE(view->indexAt(position), target);
        QSignalSpy plays(&pane, &PlaylistPane::playRequested);

        // Opt-in: the default keeps single clicks inert.
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, position, 10);
        QTest::qWait(QApplication::doubleClickInterval() + 100);
        QVERIFY(plays.isEmpty());

        pane.setSingleClickActivation(true);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, position, 10);
        QCOMPARE(plays.count(), 0); // Deferred for the double-click interval.
        QTRY_COMPARE_WITH_TIMEOUT(plays.count(), 1, 2000);
        QCOMPARE(plays.at(0).at(0).toInt(), 1);

        // A double click plays once through activation without a late single-click replay.
        plays.clear();
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, position, 10);
        QTest::mouseDClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, position, 10);
        QTRY_COMPARE_WITH_TIMEOUT(plays.count(), 1, 1000);
        QTest::qWait(QApplication::doubleClickInterval() + 100);
        QCOMPARE(plays.count(), 1);

        pane.setSingleClickActivation(false);
        plays.clear();
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, position, 10);
        QTest::qWait(QApplication::doubleClickInterval() + 100);
        QVERIFY(plays.isEmpty());
    }

    void welcomePanelRetranslatesAtRuntime() {
        QAction open(QStringLiteral("打开文件…"));
        QAction network(QStringLiteral("打开网络地址…"));
        WelcomePanel panel(&open, &network);
        panel.setRecentSources({QStringLiteral("/media/Film One.mkv")});
        panel.setResumeEntries({{QStringLiteral("/media/Film One.mkv"), 45.0, 120.0, 100}});
        QTranslator translator;
        QVERIFY(translator.load(QStringLiteral(":/i18n/NekoPlayer_en.qm")));
        qApp->installTranslator(&translator);
        QEvent languageChange(QEvent::LanguageChange);
        QCoreApplication::sendEvent(&panel, &languageChange);
        QCOMPARE(panel.findChild<QLabel *>(QStringLiteral("welcomeTitle"))->text(),
                 QStringLiteral("Start Playing"));
        QCOMPARE(panel.findChild<QLabel *>(QStringLiteral("welcomeRecentHeading"))->text(),
                 QStringLiteral("Recently Played"));
        QCOMPARE(panel.findChild<QLabel *>(QStringLiteral("welcomeContinueHeading"))->text(),
                 QStringLiteral("Continue Watching"));
        QVERIFY(panel.findChild<QPushButton *>(QStringLiteral("welcomeRecentAction1"))
                    ->accessibleName()
                    .startsWith(QStringLiteral("Local media:")));
        qApp->removeTranslator(&translator);
    }
};

QTEST_MAIN(UiSurfaceTest)
#include "ui_surface_test.moc"
