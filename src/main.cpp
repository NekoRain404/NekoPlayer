#include "playback/backenddiagnostic.h"
#include "playback/mpvcontroller.h"
#include "playback/videowidget.h"
#include "ui/mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>

int main(int argc, char *argv[]) {
    bool checkBackendOnly = false;
    for (int i = 1; i < argc; ++i) {
        const auto argument = std::string_view(argv[i]);
        checkBackendOnly = checkBackendOnly || argument == "--check-backend";
    }
    std::unique_ptr<QCoreApplication> app;
    if (checkBackendOnly)
        app = std::make_unique<QCoreApplication>(argc, argv);
    else
        app = std::make_unique<QApplication>(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("NekoPlayer"));
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
    parser.addPositionalArgument(QStringLiteral("files"), QStringLiteral("一个或多个本地媒体文件"),
                                 QStringLiteral("[files...]"));
    parser.process(*app);
    if (checkBackendOnly)
        return checkBackend();
    const auto paths = parser.positionalArguments();
    MainWindow window(parser.isSet(QStringLiteral("silent-audio")));
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
