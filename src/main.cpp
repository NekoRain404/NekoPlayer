#include "cli/commandlinearguments.h"
#include "playback/backenddiagnostic.h"
#include "playback/mpvcontroller.h"
#include "playback/videowidget.h"
#include "ui/appearancepreferences.h"
#include "ui/languagemanager.h"
#include "ui/mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <cstdlib>
#include <iostream>
#include <memory>

int main(int argc, char *argv[]) {
    const bool checkBackendOnly =
        CommandLineArguments::hasOptionBeforeSeparator(argc, argv, "--check-backend");
    std::unique_ptr<QCoreApplication> app;
    if (checkBackendOnly)
        app = std::make_unique<QCoreApplication>(argc, argv);
    else
        app = std::make_unique<QApplication>(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("NekoPlayer"));
    QCoreApplication::setOrganizationName(QStringLiteral("NekoPlayer"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

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
        const auto appearance = useConfig ? AppearancePreferences::loadSettings(settings)
                                          : AppearancePreferences::Settings{};
        AppearancePreferences::applySettings(*gui, appearance);
    }
    const auto sessionPath =
        useConfig ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
                        .filePath(QStringLiteral("session.ini"))
                  : QString{};
    MainWindow window(parser.isSet(QStringLiteral("silent-audio")), sessionPath);
    if (!paths.isEmpty())
        window.openFiles(paths);
    window.show();
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
