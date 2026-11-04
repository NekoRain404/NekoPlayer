#include "playback/mpv/mpvcontroller.h"

#include <QSignalSpy>
#include <QTemporaryFile>
#include <QtTest>

class NetworkOptionsTest final : public QObject {
    Q_OBJECT
  private slots:
    void optionsReachMpvAndAreNormalized() {
        MpvController player(true, true);
        QVERIFY(player.isReady());
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        QTemporaryFile cookies;
        QVERIFY(cookies.open());
        NetworkOptions options;
        options.proxy = QStringLiteral("http://127.0.0.1:7890");
        options.headers = {QStringLiteral(" Authorization: Bearer test "), QString(),
                           QStringLiteral("X-Test: 1")};
        options.cookiesFile = cookies.fileName();
        player.setNetworkOptions(options);
        QCOMPARE(player.networkOptions().headers,
                 QStringList(
                     {QStringLiteral("Authorization: Bearer test"), QStringLiteral("X-Test: 1")}));
        QVERIFY2(errors.isEmpty(),
                 qPrintable(errors.isEmpty() ? QString() : errors.first().first().toString()));

        char *proxy = mpv_get_property_string(player.handle(), "http-proxy");
        QCOMPARE(QString::fromUtf8(proxy), options.proxy);
        mpv_free(proxy);
    }

    void emptyOptionsCanBeClearedLater() {
        MpvController player(true, true);
        QVERIFY(player.isReady());
        NetworkOptions options;
        options.proxy = QStringLiteral("http://127.0.0.1:7890");
        options.headers = {QStringLiteral("X-Test: 1")};
        player.setNetworkOptions(options);
        player.setNetworkOptions({});
        QVERIFY(player.networkOptions().proxy.isEmpty());
        QVERIFY(player.networkOptions().headers.isEmpty());
    }
};

QTEST_GUILESS_MAIN(NetworkOptionsTest)
#include "network_options_test.moc"
