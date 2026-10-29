#include "ui/window/windowtitle.h"

#include <QtTest>

class WindowTitleTest final : public QObject {
    Q_OBJECT
  private slots:
    void emptyPathUsesTheApplicationName() {
        QCOMPARE(windowTitleForPath({}), QStringLiteral("NekoPlayer"));
    }

    void localMediaShowsTheFileName() {
        QCOMPARE(windowTitleForPath(QStringLiteral("/media/Film One.mkv")),
                 QStringLiteral("Film One.mkv — NekoPlayer"));
    }

    void controlCharactersCannotSpoofTheTitle() {
        QCOMPARE(windowTitleForPath(QStringLiteral("/media/Bad\u202Ename.mkv")),
                 QStringLiteral("Bad name.mkv — NekoPlayer"));
        QCOMPARE(windowTitleForPath(QStringLiteral("/media/Line\nBreak\x07.mkv")),
                 QStringLiteral("Line Break .mkv — NekoPlayer"));
    }

    void networkMediaShowsTheRemoteFileName() {
        QCOMPARE(windowTitleForPath(
                     QStringLiteral("https://example.test/series/episode-01.mkv?token=secret")),
                 QStringLiteral("episode-01.mkv — NekoPlayer"));
    }
};

QTEST_GUILESS_MAIN(WindowTitleTest)
#include "window_title_test.moc"
