#include "playback/mediasource.h"
#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"
#include "session/sessionstore.h"

#include <QFile>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
#include <cmath>

namespace {
// Stream from disk so even a larger generated fixture cannot grow the socket queue
// without bound. Each connection owns its callbacks, socket, and file together.
class HttpConnection final : public QObject {
  public:
    HttpConnection(QTcpSocket *socket, const QString &mediaPath, const QByteArray &target,
                   QList<QByteArray> &requests, QObject *parent)
        : QObject(parent), socket_(socket), file_(mediaPath), target_(target), requests_(requests) {
        socket_->setParent(this);
        socket_->setReadBufferSize(MaxHeaderBytes + 1);
        connect(socket_, &QTcpSocket::readyRead, this, [this] { readRequest(); });
        connect(socket_, &QTcpSocket::bytesWritten, this, [this] { pump(); });
        connect(socket_, &QTcpSocket::disconnected, this, &QObject::deleteLater);
        readRequest();
    }

  private:
    static constexpr qint64 MaxHeaderBytes = 16 * 1024;
    static constexpr qint64 ChunkBytes = 64 * 1024;

    void finishWithoutBody(const QByteArray &status, const QByteArray &extra = {}) {
        responding_ = true;
        finished_ = true;
        socket_->write("HTTP/1.1 " + status + "\r\nContent-Length: 0\r\n" + extra +
                       "Connection: close\r\n\r\n");
        socket_->disconnectFromHost();
    }

    void readRequest() {
        if (responding_)
            return;
        request_ += socket_->read(MaxHeaderBytes + 1 - request_.size());
        if (request_.size() > MaxHeaderBytes) {
            finishWithoutBody("431 Request Header Fields Too Large");
            return;
        }
        const auto headerEnd = request_.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return;
        const auto lines = request_.left(headerEnd).split('\n');
        const auto firstLine = lines.constFirst().trimmed().split(' ');
        if (firstLine.size() != 3 || (firstLine.at(0) != "GET" && firstLine.at(0) != "HEAD")) {
            finishWithoutBody("400 Bad Request");
            return;
        }
        requests_.append(firstLine.at(1));
        if (firstLine.at(1) != target_) {
            finishWithoutBody("404 Not Found");
            return;
        }
        if (!file_.open(QIODevice::ReadOnly) || file_.size() <= 0) {
            finishWithoutBody("500 Internal Server Error");
            return;
        }

        const auto size = file_.size();
        qint64 start = 0;
        qint64 end = size - 1;
        bool partial = false;
        for (const auto &line : lines) {
            const auto header = line.trimmed();
            if (!header.toLower().startsWith("range:"))
                continue;
            const auto range = header.mid(header.indexOf(':') + 1).trimmed();
            const auto bounds = range.mid(6).split('-');
            bool validStart = false;
            bool validEnd = false;
            if (range.startsWith("bytes=") && bounds.size() == 2) {
                if (bounds.at(0).isEmpty()) {
                    const auto suffix = bounds.at(1).toLongLong(&validEnd);
                    validStart = validEnd && suffix > 0;
                    start = std::max(qint64(0), size - suffix);
                } else {
                    start = bounds.at(0).toLongLong(&validStart);
                    if (bounds.at(1).isEmpty())
                        validEnd = true;
                    else
                        end = std::min(size - 1, bounds.at(1).toLongLong(&validEnd));
                }
            }
            if (!validStart || !validEnd || start < 0 || start >= size || end < start) {
                finishWithoutBody("416 Range Not Satisfiable",
                                  "Content-Range: bytes */" + QByteArray::number(size) + "\r\n");
                return;
            }
            partial = true;
            break;
        }

        if (!file_.seek(start)) {
            finishWithoutBody("500 Internal Server Error");
            return;
        }
        const auto length = end - start + 1;
        auto response = QByteArray("HTTP/1.1 ") + (partial ? "206 Partial Content" : "200 OK") +
                        "\r\nContent-Type: video/x-matroska\r\nAccept-Ranges: bytes\r\n" +
                        "Content-Length: " + QByteArray::number(length) + "\r\n";
        if (partial)
            response += "Content-Range: bytes " + QByteArray::number(start) + '-' +
                        QByteArray::number(end) + '/' + QByteArray::number(size) + "\r\n";
        response += "Connection: close\r\n\r\n";
        responding_ = true;
        remaining_ = firstLine.at(0) == "HEAD" ? 0 : length;
        socket_->write(response);
        pump();
    }

    void pump() {
        if (!responding_ || finished_)
            return;
        while (remaining_ > 0 && socket_->bytesToWrite() < 2 * ChunkBytes) {
            const auto chunk = file_.read(std::min(ChunkBytes, remaining_));
            if (chunk.isEmpty() || socket_->write(chunk) != chunk.size()) {
                finished_ = true;
                socket_->abort();
                return;
            }
            remaining_ -= chunk.size();
        }
        if (remaining_ == 0) {
            finished_ = true;
            socket_->disconnectFromHost();
        }
    }

    QTcpSocket *socket_;
    QFile file_;
    QByteArray target_;
    QList<QByteArray> &requests_;
    QByteArray request_;
    qint64 remaining_ = 0;
    bool responding_ = false;
    bool finished_ = false;
};

class LocalHttpMedia final : public QObject {
  public:
    LocalHttpMedia() {
        connect(&server_, &QTcpServer::newConnection, this, [this] {
            while (server_.hasPendingConnections())
                new HttpConnection(server_.nextPendingConnection(),
                                   QString::fromUtf8(TEST_MEDIA_PATH), target_, requests_,
                                   &server_);
        });
    }

    ~LocalHttpMedia() override {
        server_.close();
        for (auto *socket : server_.findChildren<QTcpSocket *>())
            socket->abort();
    }

    bool listen() { return server_.listen(QHostAddress::LocalHost, 0); }
    QString errorString() const { return server_.errorString(); }
    QString url() const { return origin() + QString::fromLatin1(target_); }
    QString missingUrl() const { return origin() + QStringLiteral("/missing.mkv?token=missing"); }
    const QList<QByteArray> &requests() const { return requests_; }
    const QByteArray &target() const { return target_; }

  private:
    QString origin() const {
        return QStringLiteral("http://127.0.0.1:%1").arg(server_.serverPort());
    }

    const QByteArray target_ =
        "/%E5%AA%92%E4%BD%93/%E7%8C%AB%20clip.mkv?sig=A%2fB%2BC%3D%41&part=1&part=2";
    QList<QByteArray> requests_;
    // Destroy connections before the request log referenced by their callbacks.
    QTcpServer server_;
};
} // namespace

class NetworkPlaybackTest final : public QObject {
    Q_OBJECT

  private slots:
    void playsEncodedUrlAndSupportsTransport() {
        LocalHttpMedia server;
        QVERIFY2(server.listen(), qPrintable(server.errorString()));
        MpvController player(true, true);
        QVERIFY2(player.isReady(), qPrintable(player.errorString()));
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&queue, &PlaylistController::queueError);
        const auto source = MediaSource::normalize(server.url());
        QCOMPARE(source, server.url());
        queue.addFiles({source});

        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 10000);
        QTRY_VERIFY(player.state().active && player.state().seekable);
        QCOMPARE(queue.model()->currentPath(), server.url());
        QCOMPARE(player.state().path, server.url());
        QTRY_VERIFY(player.state().duration > 3.5 && player.state().duration < 4.5);
        QVERIFY(server.requests().contains(server.target()));
        QVERIFY(errors.isEmpty());

        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QTest::qWait(100);
        const auto pausedAt = player.state().position;
        QTest::qWait(150);
        QVERIFY(std::abs(player.state().position - pausedAt) < 0.08);
        player.seekAbsolute(2.0);
        QTRY_VERIFY(std::abs(player.state().position - 2.0) < 0.25);
        QVERIFY(player.state().paused);
        player.setPaused(false);
        QTRY_VERIFY(!player.state().paused && player.state().position > 2.2);
        player.stop();
        QTRY_VERIFY(!player.state().active);
    }

    void httpFailureAdvancesToLocalFile() {
        LocalHttpMedia server;
        QVERIFY2(server.listen(), qPrintable(server.errorString()));
        MpvController player(true, true);
        QVERIFY2(player.isReady(), qPrintable(player.errorString()));
        PlaylistController queue(player);
        QSignalSpy failures(&player, &MpvController::mediaLoadFailed);
        QSignalSpy errors(&queue, &PlaylistController::queueError);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        connect(&player, &MpvController::fileLoaded, &player,
                [&player] { player.setPaused(true); });
        const auto local = QString::fromUtf8(TEST_MEDIA_PATH);
        queue.addFiles({MediaSource::normalize(server.missingUrl()), local});

        QCOMPARE(queue.model()->currentIndex(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(failures.count(), 1, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 5000);
        QTRY_VERIFY(player.state().active && player.state().paused);
        QCOMPARE(queue.model()->currentIndex(), 1);
        QCOMPARE(queue.model()->paths(), QStringList({server.missingUrl(), local}));
        QCOMPARE(player.state().path, local);
        QCOMPARE(errors.count(), 1);
        QVERIFY(server.requests().contains(QByteArray("/missing.mkv?token=missing")));
        player.stop();
        QTRY_VERIFY(!player.state().active);
    }

    void restoredUrlRemainsIdleUntilExplicitPlay() {
        LocalHttpMedia server;
        QVERIFY2(server.listen(), qPrintable(server.errorString()));
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        SessionStore store(temporary.filePath(QStringLiteral("session.ini")));
        SessionState saved;
        saved.paths = {MediaSource::normalize(server.url()), QString::fromUtf8(TEST_MEDIA_PATH)};
        saved.currentIndex = 0;
        saved.resumePosition = 1.25;
        QVERIFY(store.save(saved).isEmpty());
        const auto restored = store.load();
        QVERIFY2(restored.error.isEmpty(), qPrintable(restored.error));
        QCOMPARE(restored.state.paths, saved.paths);
        QCOMPARE(restored.state.currentIndex, 0);
        QCOMPARE(restored.state.resumePosition, 0.0);

        MpvController player(true, true);
        QVERIFY2(player.isReady(), qPrintable(player.errorString()));
        PlaylistController queue(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        connect(&player, &MpvController::fileLoaded, &player,
                [&player] { player.setPaused(true); });
        queue.restoreFiles(restored.state.paths, restored.state.currentIndex,
                           restored.state.resumePosition);
        QTest::qWait(150);
        QVERIFY(!player.state().active);
        QVERIFY(loaded.isEmpty());
        QVERIFY(server.requests().isEmpty());
        QCOMPARE(queue.model()->currentPath(), server.url());

        queue.playOrPause();
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 10000);
        QTRY_VERIFY(player.state().active && player.state().paused);
        QCOMPARE(player.state().path, server.url());
        QTRY_VERIFY(player.state().position < 0.25);
        QVERIFY(server.requests().contains(server.target()));
        player.stop();
        QTRY_VERIFY(!player.state().active);
    }
};

QTEST_GUILESS_MAIN(NetworkPlaybackTest)
#include "network_playback_test.moc"
