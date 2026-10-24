#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"
#include "session/playbackhistorystore.h"
#include "ui/appearanceicons.h"
#include "ui/appearancepreferences.h"
#include "ui/playlistpane.h"
#include "ui/welcomepanel.h"

#include <QAction>
#include <QBoxLayout>
#include <QImage>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QTranslator>
#include <QtTest>

class UiSurfaceTest final : public QObject {
    Q_OBJECT
  private slots:
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

        const auto seekBack =
            AppearanceIcons::icon(AppearanceIcons::Symbol::SeekBackward, palette).pixmap(24, 24);
        const auto seekForward =
            AppearanceIcons::icon(AppearanceIcons::Symbol::SeekForward, palette).pixmap(24, 24);
        QVERIFY(seekBack.toImage() != seekForward.toImage());

        const auto disabled = play.pixmap(QSize(24, 24), QIcon::Disabled, QIcon::Off);
        QVERIFY(!disabled.isNull());
        QCOMPARE(disabled.size(), QSize(24, 24));
    }

    void welcomePanelPaintsItsThemeSurface() {
        AppearancePreferences::applyTheme(*qApp, AppearancePreferences::Theme::Dark);
        QAction open(QStringLiteral("打开文件…"));
        QAction network(QStringLiteral("打开网络地址…"));
        QWidget video;
        WelcomePanel panel(&open, &network, &video);
        video.resize(800, 600);
        panel.setGeometry(200, 180, 400, 240);
        video.show();
        panel.show();
        QCoreApplication::processEvents();

        QImage image(panel.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        panel.render(&painter, {}, {}, QWidget::DrawWindowBackground);
        painter.end();

        QCOMPARE(image.pixelColor(10, panel.height() / 2), panel.palette().color(QPalette::Window));
        AppearancePreferences::applyTheme(*qApp, AppearancePreferences::Theme::System);
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
        panel.setRecentSources(
            {QStringLiteral("/media/one.mkv"), QStringLiteral("/media/two.mkv")});
        const QList<QSize> sizes{{400, 215}, {400, 230}, {400, 240}, {400, 250},
                                 {400, 259}, {400, 260}, {400, 261}, {400, 270},
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
