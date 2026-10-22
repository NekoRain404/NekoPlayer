#include "screenshotmenu.h"
#include "playback/mpvcontroller.h"

#include <QAction>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QStandardPaths>
#include <QWidget>

ScreenshotMenu::ScreenshotMenu(MpvController &controller, QWidget *window)
    : QObject(window), controller_(controller), window_(window),
      menu_(new QMenu(tr("截图"), window)),
      withSubtitles_(new QAction(tr("截图另存为（含字幕）…"), this)),
      withoutSubtitles_(new QAction(tr("截图另存为（无字幕）…"), this)),
      directory_(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)) {
    menu_->setObjectName(QStringLiteral("screenshotMenu"));
    withSubtitles_->setObjectName(QStringLiteral("screenshotWithSubtitlesAction"));
    withoutSubtitles_->setObjectName(QStringLiteral("screenshotWithoutSubtitlesAction"));
    withSubtitles_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    withoutSubtitles_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_E));
    for (auto *action : {withSubtitles_, withoutSubtitles_}) {
        action->setAutoRepeat(false);
        window->addAction(action);
        menu_->addAction(action);
    }
    connect(withSubtitles_, &QAction::triggered, this, [this] { chooseDestination(true); });
    connect(withoutSubtitles_, &QAction::triggered, this, [this] { chooseDestination(false); });
    connect(&controller_, &MpvController::stateChanged, this, &ScreenshotMenu::updateActions);
    connect(&controller_, &MpvController::mediaInformationChanged, this,
            &ScreenshotMenu::updateActions);
    connect(&controller_, &MpvController::screenshotPendingChanged, this,
            &ScreenshotMenu::updateActions);
    connect(&controller_, &MpvController::screenshotFinished, this,
            [this](const QString &, bool success) {
                if (success)
                    emit feedback(tr("截图已保存"));
            });
    updateActions();
}

QMenu *ScreenshotMenu::menu() const { return menu_; }

void ScreenshotMenu::updateActions() {
    const bool enabled = controller_.canTakeScreenshot();
    withSubtitles_->setEnabled(enabled);
    withoutSubtitles_->setEnabled(enabled);
}

void ScreenshotMenu::chooseDestination(bool includeSubtitles) {
    if (!controller_.canTakeScreenshot())
        return;
    if (!QDir(directory_).exists())
        directory_ = QDir::homePath();
    const auto name =
        QStringLiteral("NekoPlayer-%1.png")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")));
    QFileDialog dialog(window_, tr("截图另存为"), QDir(directory_).filePath(name));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters({tr("PNG 图像 (*.png)"), tr("JPEG 图像 (*.jpg *.jpeg)")});
    dialog.setDefaultSuffix(QStringLiteral("png"));
    connect(&dialog, &QFileDialog::filterSelected, &dialog, [&dialog](const QString &filter) {
        dialog.setDefaultSuffix(filter.contains(QStringLiteral("*.png")) ? QStringLiteral("png")
                                                                         : QStringLiteral("jpg"));
    });
    // AcceptSave keeps Qt/native overwrite confirmation enabled, including the default suffix.
    // Recheck playback in the controller after this modal dialog's nested event loop.
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty())
        return;
    const auto path = QFileInfo(dialog.selectedFiles().constFirst()).absoluteFilePath();
    const auto extension = QFileInfo(path).suffix().toLower();
    if (extension != QLatin1String("png") && extension != QLatin1String("jpg") &&
        extension != QLatin1String("jpeg")) {
        emit feedback(tr("截图未保存：请使用 .png、.jpg 或 .jpeg 扩展名"));
        return;
    }
    if (!controller_.canTakeScreenshot()) {
        emit feedback(tr("截图未保存：当前没有可截图的视频，或上一截图尚未完成"));
        return;
    }
    if (controller_.takeScreenshot(path, includeSubtitles))
        directory_ = QFileInfo(path).absolutePath();
}
