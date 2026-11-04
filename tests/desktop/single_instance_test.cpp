#include "desktop/singleinstance.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

class SingleInstanceTest final : public QObject {
    Q_OBJECT
  private slots:
    void initTestCase();
    void firstProcessOwnsEndpoint();
    void secondProcessForwardsArguments();
    void malformedAndOversizedPayloadsAreDropped();
    void destroyingOwnerReleasesEndpoint();
    void cleanupTestCase();
};

void SingleInstanceTest::initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("NekoPlayerTest"));
    QCoreApplication::setApplicationName(
        QStringLiteral("SingleInstance-%1").arg(QCoreApplication::applicationPid()));
    // Remove an endpoint left behind by an interrupted previous run.
    SingleInstance instance;
    if (!instance.endpointName().isEmpty())
        QLocalServer::removeServer(instance.endpointName());
}

void SingleInstanceTest::firstProcessOwnsEndpoint() {
    SingleInstance instance;
    const auto endpoint = instance.endpointName();
    QVERIFY(!endpoint.isEmpty());
    QVERIFY(QFileInfo(endpoint).fileName().startsWith(QStringLiteral("nekoplayer-")));
#ifdef Q_OS_UNIX
    QVERIFY(QDir::isAbsolutePath(endpoint));
#else
    // QLocalSocket rejects path separators in named-pipe names.
    QVERIFY(!endpoint.contains(QLatin1Char('/')));
    QVERIFY(!endpoint.contains(QLatin1Char('\\')));
#endif
    QCOMPARE(instance.start({}), SingleInstance::StartResult::Owner);
}

void SingleInstanceTest::secondProcessForwardsArguments() {
    SingleInstance owner;
    QCOMPARE(owner.start({}), SingleInstance::StartResult::Owner);
    QSignalSpy received(&owner, &SingleInstance::argumentsReceived);

    const QStringList arguments{QStringLiteral("C:/media/one file.mp4"),
                                QStringLiteral("--start-paused"),
                                QString::fromUtf8("日本語 with spaces.mkv"), QString()};
    SingleInstance second;
    QCOMPARE(second.start(arguments), SingleInstance::StartResult::Forwarded);
    QTRY_COMPARE_WITH_TIMEOUT(received.count(), 1, 2000);
    QCOMPARE(received.at(0).at(0).toStringList(), arguments);
}

void SingleInstanceTest::malformedAndOversizedPayloadsAreDropped() {
    SingleInstance owner;
    QCOMPARE(owner.start({}), SingleInstance::StartResult::Owner);
    QSignalSpy received(&owner, &SingleInstance::argumentsReceived);

    const auto send = [&owner](const QByteArray &payload) {
        auto *socket = new QLocalSocket;
        socket->connectToServer(owner.endpointName());
        if (!socket->waitForConnected(1000)) {
            delete socket;
            return static_cast<QLocalSocket *>(nullptr);
        }
        socket->write(payload);
        socket->flush();
        return socket;
    };

    auto *malformed = send(QByteArrayLiteral("not a json array\n"));
    QVERIFY(malformed != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(malformed->state(), QLocalSocket::UnconnectedState, 3000);
    QCOMPARE(received.count(), 0);
    delete malformed;

    auto *oversized = send({});
    QVERIFY(oversized != nullptr);
    const QByteArray chunk(256 * 1024, 'x');
    for (int index = 0; index < 6; ++index)
        oversized->write(chunk);
    oversized->flush();
    QTRY_COMPARE_WITH_TIMEOUT(oversized->state(), QLocalSocket::UnconnectedState, 3000);
    QCOMPARE(received.count(), 0);
    delete oversized;

    // The owner survived both payloads and still forwards real arguments.
    SingleInstance second;
    const QStringList arguments{QStringLiteral("still-alive.mp4")};
    QCOMPARE(second.start(arguments), SingleInstance::StartResult::Forwarded);
    QTRY_COMPARE_WITH_TIMEOUT(received.count(), 1, 2000);
    QCOMPARE(received.at(0).at(0).toStringList(), arguments);
}

void SingleInstanceTest::destroyingOwnerReleasesEndpoint() {
    QString endpoint;
    {
        SingleInstance owner;
        QCOMPARE(owner.start({}), SingleInstance::StartResult::Owner);
        endpoint = owner.endpointName();
    }
#ifdef Q_OS_UNIX
    QVERIFY(!QFile::exists(endpoint));
#endif
    SingleInstance replacement;
    QCOMPARE(replacement.endpointName(), endpoint);
    QCOMPARE(replacement.start({}), SingleInstance::StartResult::Owner);
}

void SingleInstanceTest::cleanupTestCase() {
    SingleInstance instance;
    if (!instance.endpointName().isEmpty())
        QLocalServer::removeServer(instance.endpointName());
}

QTEST_GUILESS_MAIN(SingleInstanceTest)
#include "single_instance_test.moc"
