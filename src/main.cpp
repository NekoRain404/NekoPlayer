#include "cli/commandlinearguments.h"
#include "desktop/singleinstance.h"
#include "playback/mpv/backenddiagnostic.h"
#include "playback/mpv/mpvcontroller.h"
#include "playback/render/videowidget.h"
#include "ui/theme/appearancepreferences.h"
#include "ui/theme/appicon.h"
#include "ui/theme/languagemanager.h"
#include "ui/window/mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>

#ifdef Q_OS_WIN
#include <consoleapi.h>
#include <shobjidl.h>
#include <windows.h>
#endif

int main(int argc, char *argv[]) {
#ifdef Q_OS_WIN
    // The release build uses the GUI subsystem so double-clicking the exe does
    // not flash a console. Reattach stdout/stderr when a terminal launched us so
    // --help/--check-backend remain usable.
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE *stream = nullptr;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONOUT$", "w", stderr);
    }
#endif
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
#ifdef Q_OS_WIN
    SetCurrentProcessExplicitAppUserModelID(L"NekoRain.NekoPlayer");
#endif

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
    const bool guiRun = qobject_cast<QApplication *>(app.get()) != nullptr;
    SingleInstance singleInstance;
    if (useConfig && guiRun &&
        singleInstance.start(paths) == SingleInstance::StartResult::Forwarded)
        return EXIT_SUCCESS;
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
    if (useConfig && guiRun) {
        // Connections that arrive after listen() queue in the event loop until
        // exec(), so connecting before show() keeps them from being lost.
        QObject::connect(&singleInstance, &SingleInstance::argumentsReceived, &window,
                         &MainWindow::openFiles);
        QObject::connect(&singleInstance, &SingleInstance::argumentsReceived, &window,
                         [&window](const QStringList &) {
                             if (window.isMinimized())
                                 window.showNormal();
                             window.raise();
                             window.activateWindow();
                         });
    }
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
