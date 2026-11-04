#include "desktop/singleinstance.h"

#include <QCryptographicHash>
#include <QDir>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>

#include <optional>

namespace {
constexpr qsizetype kMaxPayloadBytes = 1024 * 1024;
constexpr int kConnectTimeoutMs = 300;
constexpr int kWriteTimeoutMs = 300;

QByteArray encodeArguments(const QStringList &arguments) {
    QJsonArray array;
    for (const auto &argument : arguments)
        array.append(argument);
    return QJsonDocument(array).toJson(QJsonDocument::Compact) + '\n';
}

std::optional<QStringList> decodeArguments(const QByteArray &payload) {
    const auto newline = payload.indexOf('\n');
    const auto document = QJsonDocument::fromJson(newline < 0 ? payload : payload.left(newline));
    if (!document.isArray())
        return std::nullopt;
    const auto array = document.array();
    QStringList arguments;
    arguments.reserve(array.size());
    for (const auto &value : array) {
        if (!value.isString())
            return std::nullopt;
        arguments.append(value.toString());
    }
    return arguments;
}

QString resolveEndpointName() {
    const auto configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const auto digest = QCryptographicHash::hash(configPath.toUtf8(), QCryptographicHash::Sha256);
    const auto name = QStringLiteral("nekoplayer-") + QString::fromLatin1(digest.toHex().left(16));
#ifdef Q_OS_UNIX
    // The runtime directory is per-user; do not fall back to a shared temp
    // directory where another user could pre-create the socket path.
    const auto directory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (directory.isEmpty())
        return {};
    return QDir(directory).filePath(name);
#else
    // On Windows QLocalServer uses this name directly for the named pipe.
    return name;
#endif
}
} // namespace

class SingleInstance::Private {
  public:
    explicit Private(SingleInstance *owner) : owner_(owner) { endpoint_ = resolveEndpointName(); }

    ~Private() {
        buffers_.clear();
        if (server_ != nullptr) {
            server_->close();
            delete server_;
            server_ = nullptr;
        }
        if (!endpoint_.isEmpty())
            QLocalServer::removeServer(endpoint_);
    }

    [[nodiscard]] SingleInstance::StartResult start(const QStringList &arguments) {
        if (endpoint_.isEmpty())
            return SingleInstance::StartResult::Unavailable;
        if (server_ != nullptr && server_->isListening())
            return SingleInstance::StartResult::Owner;
        const auto payload = encodeArguments(arguments);
        if (payload.size() > kMaxPayloadBytes)
            return SingleInstance::StartResult::Unavailable;
        switch (forward(payload)) {
        case Probe::Forwarded:
            return SingleInstance::StartResult::Forwarded;
        case Probe::Failed:
            // A live owner exists but did not accept the payload. Leave its
            // endpoint alone and fall back to normal startup.
            return SingleInstance::StartResult::Unavailable;
        case Probe::NotRunning:
            break;
        }
        // The probe only fails when no live server answered, so any existing
        // endpoint is stale.
        QLocalServer::removeServer(endpoint_);
        server_ = new QLocalServer(owner_);
        server_->setSocketOptions(QLocalServer::UserAccessOption);
        QObject::connect(server_, &QLocalServer::newConnection, owner_,
                         [this] { acceptPending(); });
        if (!server_->listen(endpoint_)) {
            qWarning("single-instance listen failed: %s", qPrintable(server_->errorString()));
            delete server_;
            server_ = nullptr;
            QLocalServer::removeServer(endpoint_);
            return SingleInstance::StartResult::Unavailable;
        }
        return SingleInstance::StartResult::Owner;
    }

    [[nodiscard]] QString endpointName() const { return endpoint_; }

  private:
    enum class Probe { NotRunning, Forwarded, Failed };

    [[nodiscard]] Probe forward(const QByteArray &payload) {
        QLocalSocket socket;
        socket.connectToServer(endpoint_);
        if (!socket.waitForConnected(kConnectTimeoutMs)) {
            qWarning("single-instance connect failed: %s", qPrintable(socket.errorString()));
            return Probe::NotRunning;
        }
        if (socket.write(payload) != payload.size()) {
            qWarning("single-instance write failed: %s", qPrintable(socket.errorString()));
            socket.abort();
            return Probe::Failed;
        }
        socket.flush();
        if (socket.bytesToWrite() > 0 && !socket.waitForBytesWritten(kWriteTimeoutMs)) {
            qWarning("single-instance flush failed: %s", qPrintable(socket.errorString()));
            socket.abort();
            return Probe::Failed;
        }
        socket.disconnectFromServer();
        if (socket.state() != QLocalSocket::UnconnectedState)
            socket.waitForDisconnected(kWriteTimeoutMs);
        return Probe::Forwarded;
    }

    void acceptPending() {
        while (auto *socket = server_->nextPendingConnection()) {
            socket->setParent(owner_);
            buffers_.insert(socket, {});
            QObject::connect(socket, &QLocalSocket::readyRead, owner_,
                             [this, socket] { readSocket(socket); });
            QObject::connect(socket, &QLocalSocket::disconnected, owner_,
                             [this, socket] { finishSocket(socket); });
            if (socket->bytesAvailable() > 0)
                readSocket(socket);
        }
    }

    void readSocket(QLocalSocket *socket) {
        auto iterator = buffers_.find(socket);
        if (iterator == buffers_.end())
            return;
        QByteArray &buffer = iterator.value();
        while (socket->bytesAvailable() > 0) {
            const auto room = kMaxPayloadBytes + 1 - buffer.size();
            const auto chunk = socket->read(room);
            if (chunk.isEmpty())
                return;
            buffer.append(chunk);
            if (buffer.size() > kMaxPayloadBytes) {
                discard(socket);
                return;
            }
            if (buffer.contains('\n')) {
                deliver(socket);
                return;
            }
        }
    }

    void finishSocket(QLocalSocket *socket) {
        const auto iterator = buffers_.constFind(socket);
        if (iterator == buffers_.constEnd())
            return;
        // The peer closed without a trailing newline; the whole buffer is the
        // payload unless it exceeded the cap.
        if (iterator.value().size() <= kMaxPayloadBytes)
            deliver(socket);
        else
            discard(socket);
    }

    void deliver(QLocalSocket *socket) {
        auto iterator = buffers_.find(socket);
        if (iterator == buffers_.end())
            return;
        const auto payload = iterator.value();
        discard(socket);
        if (payload.isEmpty())
            return;
        const auto arguments = decodeArguments(payload);
        if (arguments)
            owner_->dispatch(*arguments);
    }

    void discard(QLocalSocket *socket) {
        buffers_.remove(socket);
        socket->abort();
        socket->deleteLater();
    }

    SingleInstance *owner_;
    QString endpoint_;
    QLocalServer *server_ = nullptr;
    QHash<QLocalSocket *, QByteArray> buffers_;
};

SingleInstance::SingleInstance(QObject *parent) : QObject(parent), d_(new Private(this)) {}

SingleInstance::~SingleInstance() { delete d_; }

SingleInstance::StartResult SingleInstance::start(const QStringList &arguments) {
    return d_->start(arguments);
}

QString SingleInstance::endpointName() const { return d_->endpointName(); }

void SingleInstance::dispatch(const QStringList &arguments) { emit argumentsReceived(arguments); }
