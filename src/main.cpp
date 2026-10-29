#include "cli/commandlinearguments.h"
#include "playback/mpv/backenddiagnostic.h"
#include "playback/mpv/mpvcontroller.h"
#include "playback/render/videowidget.h"
#include "ui/theme/appearancepreferences.h"
#include "ui/theme/appicon.h"
#include "ui/theme/languagemanager.h"
#include "ui/window/mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QTimer>
#include <clocale>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace {
QByteArray encodeSingleInstanceArguments(const QStringList &arguments) {
    QJsonArray array;
    for (const auto &argument : arguments)
        array.append(argument);
    return QJsonDocument(array).toJson(QJsonDocument::Compact) + '\n';
}

std::optional<QStringList> decodeSingleInstanceArguments(const QByteArray &payload) {
    const auto newline = payload.indexOf('\n');
    const auto document = QJsonDocument::fromJson(newline < 0 ? payload : payload.left(newline));
    if (!document.isArray())
        return std::nullopt;
    QStringList arguments;
    for (const auto &value : document.array()) {
        if (!value.isString())
            return std::nullopt;
        arguments.append(value.toString());
    }
    return arguments;
}

#ifdef Q_OS_UNIX
// QtNetwork is not linked into the application, so the handoff uses a
// Unix-domain socket with the same per-configuration name. The protocol and
// fallback behaviour match QLocalServer: a failed probe or listen leaves normal
// startup untouched.
QString singleInstanceSocketPath() {
    const auto configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const auto digest = QCryptographicHash::hash(configPath.toUtf8(), QCryptographicHash::Sha256);
    const auto name = QStringLiteral("nekoplayer-") + QString::fromLatin1(digest.toHex().left(16));
    // The runtime directory is per-user; do not fall back to a shared temp
    // directory where another user could pre-create the socket path.
    const auto directory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (directory.isEmpty())
        return {};
    return QDir(directory).filePath(name);
}

bool setCloseOnExec(int fd) {
    const int flags = ::fcntl(fd, F_GETFD, 0);
    return flags >= 0 && ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == 0;
}

bool setNonBlocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    return flags >= 0 && ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

int connectToRunningInstance(const QString &path) {
    sockaddr_un address{};
    const auto nativePath = QFile::encodeName(path);
    if (nativePath.isEmpty() || nativePath.size() >= static_cast<int>(sizeof(address.sun_path)))
        return -1;
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    if (!setCloseOnExec(fd) || !setNonBlocking(fd)) {
        ::close(fd);
        return -1;
    }
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, nativePath.constData(), static_cast<size_t>(nativePath.size()));
    if (::connect(fd, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0) {
        if (errno != EINPROGRESS) {
            ::close(fd);
            return -1;
        }
        pollfd descriptor{fd, POLLOUT, 0};
        int ready = 0;
        do {
            ready = ::poll(&descriptor, 1, 300);
        } while (ready < 0 && errno == EINTR);
        int error = 0;
        socklen_t length = sizeof(error);
        if (ready <= 0 || ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) != 0 ||
            error != 0) {
            ::close(fd);
            return -1;
        }
    }
    return fd;
}

bool writeAll(int fd, const QByteArray &payload) {
    qsizetype written = 0;
    while (written < payload.size()) {
        const auto count = ::write(fd, payload.constData() + written,
                                   static_cast<size_t>(payload.size() - written));
        if (count == 0)
            return false;
        if (count < 0) {
            if (errno == EINTR)
                continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                return false;
            pollfd descriptor{fd, POLLOUT, 0};
            if (::poll(&descriptor, 1, 300) <= 0)
                return false;
            continue;
        }
        written += count;
    }
    return true;
}

enum class InstanceProbe { NotRunning, Forwarded, Failed };

InstanceProbe forwardToRunningInstance(const QString &path, const QStringList &arguments) {
    const int fd = connectToRunningInstance(path);
    if (fd < 0)
        return InstanceProbe::NotRunning;
    const bool sent = writeAll(fd, encodeSingleInstanceArguments(arguments));
    // Closing delivers the queued one-line payload and the peer's EOF.
    ::close(fd);
    return sent ? InstanceProbe::Forwarded : InstanceProbe::Failed;
}

struct SingleInstanceServer {
    int fd = -1;
    QString path;
    ~SingleInstanceServer() {
        if (fd >= 0)
            ::close(fd);
        if (!path.isEmpty()) {
            const auto nativePath = QFile::encodeName(path);
            ::unlink(nativePath.constData());
        }
    }
};

void startSingleInstanceServer(SingleInstanceServer &server) {
    const auto path = singleInstanceSocketPath();
    if (path.isEmpty())
        return;
    sockaddr_un address{};
    const auto nativePath = QFile::encodeName(path);
    if (nativePath.isEmpty() || nativePath.size() >= static_cast<int>(sizeof(address.sun_path)))
        return;
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return;
    if (!setCloseOnExec(fd) || !setNonBlocking(fd)) {
        ::close(fd);
        return;
    }
    // The probe only fails when no live server answered, so any existing path
    // is stale.
    ::unlink(nativePath.constData());
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, nativePath.constData(), static_cast<size_t>(nativePath.size()));
    if (::bind(fd, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0) {
        ::close(fd);
        return;
    }
    if (::listen(fd, 16) != 0) {
        ::close(fd);
        ::unlink(nativePath.constData());
        return;
    }
    ::chmod(nativePath.constData(), 0600);
    server.fd = fd;
    server.path = path;
}

void serveSingleInstance(SingleInstanceServer &server, QApplication &application,
                         MainWindow &window) {
    auto *notifier = new QSocketNotifier(server.fd, QSocketNotifier::Read, &application);
    QObject::connect(
        notifier, &QSocketNotifier::activated, notifier, [&server, &application, &window] {
            while (true) {
                const int connection = ::accept(server.fd, nullptr, nullptr);
                if (connection < 0)
                    return;
                if (!setNonBlocking(connection)) {
                    ::close(connection);
                    continue;
                }
                setCloseOnExec(connection);
                auto payload = std::make_shared<QByteArray>();
                auto *reader = new QSocketNotifier(connection, QSocketNotifier::Read, &application);
                QObject::connect(reader, &QSocketNotifier::activated, reader,
                                 [connection, payload, reader, &window] {
                                     bool peerClosed = false;
                                     char buffer[4096];
                                     while (true) {
                                         const auto count =
                                             ::read(connection, buffer, sizeof(buffer));
                                         if (count > 0) {
                                             payload->append(buffer, static_cast<int>(count));
                                             continue;
                                         }
                                         if (count == 0) {
                                             peerClosed = true;
                                             break;
                                         }
                                         if (errno == EINTR)
                                             continue;
                                         if (errno == EAGAIN || errno == EWOULDBLOCK)
                                             break;
                                         peerClosed = true;
                                         break;
                                     }
                                     if (!peerClosed && !payload->contains('\n'))
                                         return;
                                     reader->setEnabled(false);
                                     reader->deleteLater();
                                     ::close(connection);
                                     if (payload->isEmpty())
                                         return;
                                     const auto arguments = decodeSingleInstanceArguments(*payload);
                                     if (!arguments)
                                         return;
                                     window.openFiles(*arguments);
                                     if (window.isMinimized())
                                         window.showNormal();
                                     window.raise();
                                     window.activateWindow();
                                 });
            }
        });
}
#endif
} // namespace

int main(int argc, char *argv[]) {
    const bool checkBackendOnly =
        CommandLineArguments::hasOptionBeforeSeparator(argc, argv, "--check-backend");
    std::unique_ptr<QCoreApplication> app;
    if (checkBackendOnly)
        app = std::make_unique<QCoreApplication>(argc, argv);
    else
        app = std::make_unique<QApplication>(argc, argv);
    // Qt may adopt the desktop locale while constructing the application;
    // libmpv requires C numeric formatting, so pin it after Qt initialization.
    std::setlocale(LC_NUMERIC, "C");
    QCoreApplication::setApplicationName(QStringLiteral("NekoPlayer"));
    QCoreApplication::setOrganizationName(QStringLiteral("NekoPlayer"));
    QCoreApplication::setApplicationVersion(QStringLiteral(NEKOPLAYER_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("NekoPlayer — Qt 6 / libmpv 桌面播放器"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(
        {QStringLiteral("check-backend"), QStringLiteral("检查 libmpv 后退出（无需显示服务器）")});
    parser.addOption(
        {QStringLiteral("smoke-test"), QStringLiteral("验证窗口和渲染初始化后自动退出")});
    parser.addOption(
        {QStringLiteral("silent-audio"), QStringLiteral("使用空音频输出，供自动化检查使用")});
    parser.addOption(
        {QStringLiteral("no-config"), QStringLiteral("不读取或保存播放列表和偏好设置")});
    parser.addOption({QStringLiteral("fullscreen"), QStringLiteral("启动后进入全屏")});
    parser.addOption({QStringLiteral("start-paused"), QStringLiteral("启动后暂停在第一帧")});
    parser.addPositionalArgument(QStringLiteral("files"),
                                 QStringLiteral("一个或多个媒体文件或网络地址"),
                                 QStringLiteral("[files...]"));
    parser.process(*app);
    if (checkBackendOnly)
        return checkBackend();
    const auto paths = parser.positionalArguments();
    const bool useConfig =
        !parser.isSet(QStringLiteral("no-config")) && !parser.isSet(QStringLiteral("smoke-test"));
    app->setProperty("configurationEnabled", useConfig);
#ifdef Q_OS_UNIX
    const bool guiRun = qobject_cast<QApplication *>(app.get()) != nullptr;
    SingleInstanceServer singleInstanceServer;
    if (useConfig && guiRun) {
        const auto socketPath = singleInstanceSocketPath();
        if (!socketPath.isEmpty()) {
            const auto probe = forwardToRunningInstance(socketPath, paths);
            if (probe == InstanceProbe::Forwarded)
                return EXIT_SUCCESS;
            // A failed write leaves the live instance's socket alone; normal
            // startup continues without claiming it.
            if (probe == InstanceProbe::NotRunning)
                startSingleInstanceServer(singleInstanceServer);
        }
    }
#endif
    QSettings settings;
    const auto language =
        useConfig ? settings.value(QStringLiteral("appearance/language"), QStringLiteral("zh_CN"))
                        .toString()
                  : QStringLiteral("zh_CN");
    LanguageManager languageManager(*app);
    if (!languageManager.setLanguage(language))
        if (!languageManager.setLanguage(QStringLiteral("zh_CN")))
            return EXIT_FAILURE;
    app->setProperty("languageManager",
                     QVariant::fromValue(static_cast<QObject *>(&languageManager)));
    if (auto *gui = qobject_cast<QApplication *>(app.get())) {
        QGuiApplication::setDesktopFileName(QStringLiteral("nekoplayer"));
        QApplication::setWindowIcon(AppIcon::windowIcon());
        const auto appearance = useConfig ? AppearancePreferences::loadSettings(settings)
                                          : AppearancePreferences::Settings{};
        AppearancePreferences::applySettings(*gui, appearance);
    }
    const auto sessionPath =
        useConfig ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
                        .filePath(QStringLiteral("session.ini"))
                  : QString{};
    MainWindow window(parser.isSet(QStringLiteral("silent-audio")), sessionPath);
#ifdef Q_OS_UNIX
    if (auto *gui = qobject_cast<QApplication *>(app.get()); gui && singleInstanceServer.fd >= 0)
        serveSingleInstance(singleInstanceServer, *gui, window);
#endif
    if (!paths.isEmpty()) {
        if (parser.isSet(QStringLiteral("start-paused")))
            window.setStartPaused(true);
        window.openFiles(paths);
    }
    window.show();
    if (parser.isSet(QStringLiteral("fullscreen")))
        window.enterFullscreen();
    if (parser.isSet(QStringLiteral("smoke-test"))) {
        QObject::connect(&window, &MainWindow::userError, app.get(), [](const QString &message) {
            std::cerr << message.toStdString() << '\n';
            QCoreApplication::exit(EXIT_FAILURE);
        });
        QTimer::singleShot(1200, &window, [&window, &paths] {
            const bool ready = window.controller()->isReady() && window.videoWidget()->isReady();
            const bool loaded = paths.isEmpty() || window.controller()->state().active;
            QCoreApplication::exit(ready && loaded ? EXIT_SUCCESS : EXIT_FAILURE);
        });
    }
    return app->exec();
}
