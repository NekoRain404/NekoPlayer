#include "audiodevicemenu.h"
#include "playback/mpvcontroller.h"

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QWidget>

AudioDeviceMenu::AudioDeviceMenu(MpvController &controller, QWidget *parent)
    : QObject(parent), controller_(controller), menu_(new QMenu(tr("输出设备"), parent)),
      group_(new QActionGroup(this)) {
    menu_->setObjectName(QStringLiteral("audioOutputDevicesMenu"));
    group_->setExclusive(true);
    connect(group_, &QActionGroup::triggered, this,
            [this](QAction *action) { controller_.setAudioDevice(action->data().toString()); });
    connect(&controller_, &MpvController::audioDevicesChanged, this, &AudioDeviceMenu::rebuild);
    connect(&controller_, &MpvController::stateChanged, this, &AudioDeviceMenu::updateSelection);
    rebuild();
}

QMenu *AudioDeviceMenu::menu() const { return menu_; }

void AudioDeviceMenu::rebuild() {
    menu_->clear();
    for (const auto &device : controller_.audioDevices()) {
        const auto label = device.description.isEmpty() ? device.name : device.description;
        auto *action = menu_->addAction(label);
        action->setObjectName(QStringLiteral("audioOutputDevice_%1").arg(device.name));
        action->setData(device.name);
        action->setCheckable(true);
        group_->addAction(action);
    }
    menu_->setEnabled(!controller_.audioDevices().isEmpty());
    updateSelection();
}

void AudioDeviceMenu::updateSelection() {
    const auto selected = controller_.selectedAudioDevice();
    for (auto *action : group_->actions())
        action->setChecked(action->data().toString() == selected);
}
