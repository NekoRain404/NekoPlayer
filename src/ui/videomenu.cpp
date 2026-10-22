#include "videomenu.h"
#include "playback/mpvcontroller.h"
#include "screenshotmenu.h"
#include "videocolordialog.h"
#include "videoframingmenu.h"

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QWidget>

VideoMenu::VideoMenu(MpvController &controller, QWidget *window)
    : QObject(window), controller_(controller), menu_(new QMenu(window)), aspectMenu_(nullptr),
      hdrToneMappingMenu_(nullptr), aspects_(new QActionGroup(this)),
      hdrToneMappings_(new QActionGroup(this)) {
    menu_->setObjectName(QStringLiteral("videoMenu"));
    aspects_->setExclusive(true);
    hdrToneMappings_->setExclusive(true);
    const auto make = [this, window](const QString &text, const char *name,
                                     QKeySequence shortcut = {}) {
        auto *action = new QAction(text, this);
        action->setObjectName(QString::fromLatin1(name));
        action->setShortcut(shortcut);
        window->addAction(action);
        return action;
    };
    aspectMenu_ = menu_->addMenu(tr("宽高比"));
    aspectMenu_->setObjectName(QStringLiteral("videoAspectMenu"));
    const auto addAspect = [this, &make](const QString &text, const char *name,
                                         VideoAspect aspect) {
        auto *action = make(text, name);
        action->setCheckable(true);
        action->setData(static_cast<int>(aspect));
        aspects_->addAction(action);
        aspectMenu_->addAction(action);
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
    auto *framing = new VideoFramingMenu(controller_, window);
    menu_->addMenu(framing->menu());
    connect(framing, &VideoFramingMenu::feedback, this, &VideoMenu::feedback);
    menu_->addSeparator();
    auto *colorAdjustment = make(tr("色彩调整…"), "videoColorAdjustmentAction");
    menu_->addAction(colorAdjustment);
    connect(colorAdjustment, &QAction::triggered, this, [this, window] {
        VideoColorDialog dialog(controller_, window);
        dialog.exec();
    });
    menu_->addSeparator();
    hdrToneMappingMenu_ = menu_->addMenu(tr("HDR 色调映射"));
    hdrToneMappingMenu_->setObjectName(QStringLiteral("hdrToneMappingMenu"));
    const auto addToneMapping = [this, &make](const QString &text, const char *name,
                                              HdrToneMapping mapping) {
        auto *action = make(text, name);
        action->setCheckable(true);
        action->setData(static_cast<int>(mapping));
        hdrToneMappings_->addAction(action);
        hdrToneMappingMenu_->addAction(action);
        connect(action, &QAction::triggered, this, [this, mapping] {
            controller_.setHdrToneMapping(mapping);
            update();
        });
    };
    addToneMapping(tr("自动"), "hdrToneMappingAutoAction", HdrToneMapping::Auto);
    addToneMapping(tr("BT.2390"), "hdrToneMappingBt2390Action", HdrToneMapping::Bt2390);
    addToneMapping(tr("剪裁"), "hdrToneMappingClipAction", HdrToneMapping::Clip);
    menu_->addSeparator();
    auto *screenshots = new ScreenshotMenu(controller_, window);
    menu_->addMenu(screenshots->menu());
    connect(screenshots, &ScreenshotMenu::feedback, this, &VideoMenu::feedback);
    connect(&controller_, &MpvController::videoPresentationChanged, this, [this] {
        update();
        emit feedback(tr("画面比例：%1 · 旋转：%2°")
                          .arg(aspects_->checkedAction()->text())
                          .arg(controller_.videoPresentation().rotation));
    });
    connect(&controller_, &MpvController::hdrToneMappingChanged, this, &VideoMenu::update);
    // Menu actions also live on the window for shortcuts, so disable them all
    // independently if backend startup failed.
    for (auto *action : {clockwise, counterClockwise})
        action->setEnabled(controller_.isReady());
    aspects_->setEnabled(controller_.isReady());
    colorAdjustment->setEnabled(controller_.isReady());
    hdrToneMappings_->setEnabled(controller_.isReady());
    menu_->setEnabled(controller_.isReady());
    update();
}

QMenu *VideoMenu::menu() const { return menu_; }

void VideoMenu::retranslate() {
    const auto setActionText = [this](const char *name, const QString &text) {
        if (auto *action = findChild<QAction *>(QString::fromLatin1(name)))
            action->setText(text);
    };
    aspectMenu_->setTitle(tr("宽高比"));
    setActionText("videoAspectSourceAction", tr("原始比例"));
    setActionText("videoAspectStretchAction", tr("拉伸填满"));
    setActionText("videoRotateClockwiseAction", tr("向右旋转 90°"));
    setActionText("videoRotateCounterClockwiseAction", tr("向左旋转 90°"));
    setActionText("videoRotationResetAction", tr("重置旋转"));
    setActionText("videoColorAdjustmentAction", tr("色彩调整…"));
    hdrToneMappingMenu_->setTitle(tr("HDR 色调映射"));
    setActionText("hdrToneMappingAutoAction", tr("自动"));
    setActionText("hdrToneMappingClipAction", tr("剪裁"));
    update();
}

void VideoMenu::update() {
    const auto &presentation = controller_.videoPresentation();
    for (auto *action : aspects_->actions())
        action->setChecked(action->data().toInt() == static_cast<int>(presentation.aspect));
    for (auto *action : hdrToneMappings_->actions())
        action->setChecked(action->data().toInt() ==
                           static_cast<int>(controller_.hdrToneMapping()));
    resetRotation_->setEnabled(controller_.isReady() && presentation.rotation != 0);
    menu_->setTitle(tr("视频(&I) · %1 · %2°")
                        .arg(aspects_->checkedAction()->text())
                        .arg(presentation.rotation));
}
