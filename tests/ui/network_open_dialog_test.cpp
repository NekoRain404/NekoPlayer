#include "ui/dialogs/networkopendialog.h"

#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QtTest>

class NetworkOpenDialogTest final : public QObject {
    Q_OBJECT
  private slots:
    void parsesUrlAndAdvancedOptions() {
        NetworkOpenDialog dialog;
        auto *address = dialog.findChild<QLineEdit *>(QStringLiteral("networkAddress"));
        auto *proxy = dialog.findChild<QLineEdit *>(QStringLiteral("networkProxy"));
        auto *headers = dialog.findChild<QPlainTextEdit *>(QStringLiteral("networkHeaders"));
        auto *cookies = dialog.findChild<QLineEdit *>(QStringLiteral("networkCookies"));
        auto *open = dialog.findChild<QPushButton *>(QStringLiteral("openNetworkButton"));
        QVERIFY(address && proxy && headers && cookies);
        QVERIFY(open && !open->isEnabled());

        address->setText(QStringLiteral("https://example.test/video.mkv"));
        proxy->setText(QStringLiteral("http://127.0.0.1:7890"));
        headers->setPlainText(QStringLiteral("X-Test: 1\n\nAuthorization: Bearer token"));
        cookies->setText(QStringLiteral("/tmp/cookies.txt"));
        QCOMPARE(dialog.mediaUrl(), QStringLiteral("https://example.test/video.mkv"));
        QVERIFY(open->isEnabled());
        const auto options = dialog.networkOptions();
        QCOMPARE(options.proxy, QStringLiteral("http://127.0.0.1:7890"));
        QCOMPARE(options.headers,
                 QStringList({QStringLiteral("X-Test: 1"),
                              QStringLiteral("Authorization: Bearer token")}));
        QCOMPARE(options.cookiesFile, QStringLiteral("/tmp/cookies.txt"));
    }

    void rejectsNonNetworkAddresses() {
        NetworkOpenDialog dialog;
        auto *address = dialog.findChild<QLineEdit *>(QStringLiteral("networkAddress"));
        QVERIFY(address);
        address->setText(QStringLiteral("/local/file.mkv"));
        QVERIFY(dialog.mediaUrl().isEmpty());
        address->setText(QStringLiteral("file:///local/file.mkv"));
        QVERIFY(dialog.mediaUrl().isEmpty());
    }
};

QTEST_MAIN(NetworkOpenDialogTest)
#include "network_open_dialog_test.moc"
