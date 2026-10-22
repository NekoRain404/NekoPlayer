#include "videomenu.h"
#include "playback/mpvcontroller.h"
#include "videocolordialog.h"

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QWidget>

VideoMenu::VideoMenu(MpvController &controller, QWidget *window)
    : QObject(window), controller_(controller), menu_(new QMenu(window)),
      aspects_(new QActionGroup(this)) {
    menu_->setObjectName(QStringLiteral("videoMenu"));
    const auto make = [this, window](const QString &text, const char *name,
                                     QKeySequence shortcut = {}) {
        auto *action = new QAction(text, this);
        action->setObjectName(QString::fromLatin1(name));
        action->setShortcut(shortcut);
        window->addAction(action);
        return action;
    };
    auto *aspectMenu = menu_->addMenu(tr("宽高比"));
    const auto addAspect = [this, aspectMenu, &make](const QString &text, const char *name,
                                                     VideoAspect aspect) {
        auto *action = make(text, name);
        action->setCheckable(true);
        action->setData(static_cast<int>(aspect));
        aspects_->addAction(action);
        aspectMenu->addAction(action);
        connect(action, &QAction::triggered, this, [this, aspect] {
            controller_.setVideoAspect(aspect);
            // QAction checks itself even if the controller rejects submission.
            update();
        });
    };
    addAspect(tr("原始比例"), "videoAspectSourceAction", VideoAspect::Source);
    addAspect(QStringLiteral("4:3"), "videoAspect4x3Action", VideoAspect::Ratio4x3);
    addAspect(QStringLiteral("16:9"), "videoAspect16x9Action", VideoAspect::Ratio16x9);
    addAspect(QStringLiteral("16:10"), "videoAspect16x10Action", VideoAspect::Ratio16x10);
    addAspect(QStringLiteral("21:9"), "videoAspect21x9Action", VideoAspect::Ratio21x9);
    addAspect(tr("拉伸填满"), "videoAspectStretchAction", VideoAspect::Stretch);
    menu_->addSeparator();
    auto *clockwise = make(tr("向右旋转 90°"), "videoRotateClockwiseAction",
                           QKeySequence(Qt::ALT | Qt::Key_Right));
    auto *counterClockwise = make(tr("向左旋转 90°"), "videoRotateCounterClockwiseAction",
                                  QKeySequence(Qt::ALT | Qt::Key_Left));
    resetRotation_ = make(tr("重置旋转"), "videoRotationResetAction");
    menu_->addActions({clockwise, counterClockwise, resetRotation_});
    connect(clockwise, &QAction::triggered, &controller_, &MpvController::rotateVideoClockwise);
    connect(counterClockwise, &QAction::triggered, &controller_,
            &MpvController::rotateVideoCounterClockwise);
    connect(resetRotation_, &QAction::triggered, this, [this] { controller_.setVideoRotation(0); });
    menu_->addSeparator();
    auto *colorAdjustment = make(tr("色彩调整…"), "videoColorAdjustmentAction");
    menu_->addAction(colorAdjustment);
    connect(colorAdjustment, &QAction::triggered, this, [this, window] {
        VideoColorDialog dialog(controller_, window);
        dialog.exec();
    });
    connect(&controller_, &MpvController::videoPresentationChanged, this, [this] {
        update();
        emit feedback(tr("画面比例：%1 · 旋转：%2°")
                          .arg(aspects_->checkedAction()->text())
                          .arg(controller_.videoPresentation().rotation));
    });
    // Menu actions also live on the window for shortcuts, so disable them all
    // independently if backend startup failed.
    for (auto *action : {clockwise, counterClockwise})
        action->setEnabled(controller_.isReady());
    aspects_->setEnabled(controller_.isReady());
    colorAdjustment->setEnabled(controller_.isReady());
    menu_->setEnabled(controller_.isReady());
    update();
}

QMenu *VideoMenu::menu() const { return menu_; }

void VideoMenu::update() {
    const auto &presentation = controller_.videoPresentation();
    for (auto *action : aspects_->actions())
        action->setChecked(action->data().toInt() == static_cast<int>(presentation.aspect));
    resetRotation_->setEnabled(controller_.isReady() && presentation.rotation != 0);
    menu_->setTitle(tr("视频(&I) · %1 · %2°")
                        .arg(aspects_->checkedAction()->text())
                        .arg(presentation.rotation));
}
