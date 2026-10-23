#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"
#include "ui/playlistpane.h"
#include "ui/welcomepanel.h"

#include <QAction>
#include <QBoxLayout>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QSignalSpy>
#include <QTranslator>
#include <QtTest>

class UiSurfaceTest final : public QObject {
    Q_OBJECT
  private slots:
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
        QTranslator translator;
        QVERIFY(translator.load(QStringLiteral(":/i18n/NekoPlayer_en.qm")));
        qApp->installTranslator(&translator);
        QEvent languageChange(QEvent::LanguageChange);
        QCoreApplication::sendEvent(&panel, &languageChange);
        QCOMPARE(panel.findChild<QLabel *>(QStringLiteral("welcomeTitle"))->text(),
                 QStringLiteral("Start Playing"));
        QCOMPARE(panel.findChild<QLabel *>(QStringLiteral("welcomeRecentHeading"))->text(),
                 QStringLiteral("Recently Played"));
        QVERIFY(panel.findChild<QPushButton *>(QStringLiteral("welcomeRecentAction1"))
                    ->accessibleName()
                    .startsWith(QStringLiteral("Local media:")));
        qApp->removeTranslator(&translator);
    }
};

QTEST_MAIN(UiSurfaceTest)
#include "ui_surface_test.moc"
