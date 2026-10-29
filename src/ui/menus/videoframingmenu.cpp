#include "ui/menus/videoframingmenu.h"
#include "playback/mpv/mpvcontroller.h"

#include <QAction>
#include <QActionGroup>
#include <QLocale>
#include <QMenu>
#include <QWidget>
#include <cmath>

namespace {
QString percentage(double value, bool signedValue = false) {
    if (!std::isfinite(value))
        return QStringLiteral("—");
    // Round before formatting so tiny negative offsets never display as -0%.
    const auto rounded = std::round(value * 10.0) / 10.0;
    const auto decimals = rounded == std::round(rounded) ? 0 : 1;
    const auto number = QLocale().toString(rounded == 0 ? 0.0 : rounded, 'f', decimals);
    return signedValue && rounded > 0 ? QStringLiteral("+") + number : number;
}
} // namespace

VideoFramingMenu::VideoFramingMenu(MpvController &controller, QWidget *window)
    : QObject(window), controller_(controller), menu_(new QMenu(window)),
      presets_(new QActionGroup(this)) {
    menu_->setObjectName(QStringLiteral("videoFramingMenu"));
    const auto make = [this, window](const QString &text, const char *name,
                                     QKeySequence shortcut = {}) {
        auto *action = new QAction(text, this);
        action->setObjectName(QString::fromLatin1(name));
        action->setShortcut(shortcut);
        window->addAction(action);
        menu_->addAction(action);
        return action;
    };
    zoomIn_ = make(tr("放大画面"), "videoZoomInAction", QKeySequence(Qt::ALT | Qt::Key_Equal));
    zoomOut_ = make(tr("缩小画面"), "videoZoomOutAction", QKeySequence(Qt::ALT | Qt::Key_Minus));
    connect(zoomIn_, &QAction::triggered, this, [this] { controller_.adjustVideoZoom(0.1); });
    connect(zoomOut_, &QAction::triggered, this, [this] { controller_.adjustVideoZoom(-0.1); });
    menu_->addSeparator();
    const struct {
        int percent;
        const char *name;
    } zoomPresets[] = {{25, "videoZoom25Action"},   {50, "videoZoom50Action"},
                       {100, "videoZoom100Action"}, {150, "videoZoom150Action"},
                       {200, "videoZoom200Action"}, {400, "videoZoom400Action"},
                       {800, "videoZoom800Action"}};
    for (const auto &preset : zoomPresets) {
        auto *action = make(tr("%1%").arg(percentage(preset.percent)), preset.name);
        const auto zoom = std::log2(preset.percent / 100.0);
        action->setCheckable(true);
        action->setAutoRepeat(false);
        action->setData(zoom);
        presets_->addAction(action);
        connect(action, &QAction::triggered, this, [this, zoom] {
            controller_.setVideoZoom(zoom);
            // Restore checks even if the backend rejects the request.
            update();
        });
    }
    menu_->addSeparator();
    panLeft_ = make(tr("画面左移"), "videoPanLeftAction",
                    QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_Left));
    panRight_ = make(tr("画面右移"), "videoPanRightAction",
                     QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_Right));
    panUp_ =
        make(tr("画面上移"), "videoPanUpAction", QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_Up));
    panDown_ = make(tr("画面下移"), "videoPanDownAction",
                    QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_Down));
    for (auto *action : {panLeft_, panRight_, panUp_, panDown_})
        action->setToolTip(tr("每次移动缩放后画面尺寸的 5%"));
    connect(panLeft_, &QAction::triggered, this, [this] { controller_.adjustVideoPanX(-0.05); });
    connect(panRight_, &QAction::triggered, this, [this] { controller_.adjustVideoPanX(0.05); });
    connect(panUp_, &QAction::triggered, this, [this] { controller_.adjustVideoPanY(-0.05); });
    connect(panDown_, &QAction::triggered, this, [this] { controller_.adjustVideoPanY(0.05); });
    menu_->addSeparator();
    reset_ =
        make(tr("重置缩放与位置"), "videoFramingResetAction", QKeySequence(Qt::ALT | Qt::Key_0));
    reset_->setAutoRepeat(false);
    connect(reset_, &QAction::triggered, &controller_, &MpvController::resetVideoFraming);
    connect(&controller_, &MpvController::videoFramingChanged, this, [this] {
        update();
        emit feedback(menu_->title());
    });
    connect(&controller_, &MpvController::videoPresentationChanged, this,
            &VideoFramingMenu::update);
    update();
}

QMenu *VideoFramingMenu::menu() const { return menu_; }

void VideoFramingMenu::update() {
    const auto &framing = controller_.videoFraming();
    const auto ready = controller_.isReady();
    const auto stretched = controller_.videoPresentation().aspect == VideoAspect::Stretch;
    const auto available = ready && !stretched;
    menu_->setEnabled(ready);
    presets_->setEnabled(available);
    zoomIn_->setEnabled(available && framing.zoom < 3.0);
    zoomOut_->setEnabled(available && framing.zoom > -2.0);
    panLeft_->setEnabled(available && framing.panX > -1.0);
    panRight_->setEnabled(available && framing.panX < 1.0);
    panUp_->setEnabled(available && framing.panY > -1.0);
    panDown_->setEnabled(available && framing.panY < 1.0);
    reset_->setEnabled(ready && (framing.zoom != 0 || framing.panX != 0 || framing.panY != 0));
    for (auto *action : presets_->actions())
        action->setChecked(std::abs(action->data().toDouble() - framing.zoom) < 0.000001);
    auto title =
        tr("画面缩放与平移 · %1% · 水平 %2% · 垂直 %3%")
            .arg(percentage(std::exp2(framing.zoom) * 100.0),
                 percentage(framing.panX * 100.0, true), percentage(framing.panY * 100.0, true));
    if (stretched)
        title += tr(" · 拉伸填满时不可用");
    menu_->setTitle(title);
    menu_->setToolTip(stretched ? tr("拉伸填满时不可用；切回其他宽高比后恢复缩放与位置。")
                                : tr("缩放与平移画面；平移百分比相对于缩放后的画面尺寸。"));
}
