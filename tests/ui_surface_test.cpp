#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "ui/playlistpane.h"
#include "ui/welcomepanel.h"

#include <QAction>
#include <QBoxLayout>
#include <QLabel>
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
        auto *hint = panel.findChild<QLabel *>(QStringLiteral("welcomeDropHint"));
        QVERIFY(openButton && networkButton && title && hint);
        QCOMPARE(openButton->text(), open.text());
        QCOMPARE(networkButton->text(), network.text());
        QVERIFY(!title->text().isEmpty());
        QVERIFY(!hint->text().isEmpty());
        QVERIFY(openButton->accessibleName() == open.text());
        QVERIFY(networkButton->accessibleName() == network.text());
        QSignalSpy openSpy(&open, &QAction::triggered);
        openButton->click();
        QCOMPARE(openSpy.size(), 1);
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

    void welcomePanelRetranslatesAtRuntime() {
        QAction open(QStringLiteral("打开文件…"));
        QAction network(QStringLiteral("打开网络地址…"));
        WelcomePanel panel(&open, &network);
        QTranslator translator;
        QVERIFY(translator.load(QStringLiteral(":/i18n/NekoPlayer_en.qm")));
        qApp->installTranslator(&translator);
        QEvent languageChange(QEvent::LanguageChange);
        QCoreApplication::sendEvent(&panel, &languageChange);
        QCOMPARE(panel.findChild<QLabel *>(QStringLiteral("welcomeTitle"))->text(),
                 QStringLiteral("Start Playing"));
        qApp->removeTranslator(&translator);
    }
};

QTEST_MAIN(UiSurfaceTest)
#include "ui_surface_test.moc"
