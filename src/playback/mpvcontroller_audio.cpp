#include "mpvcontroller_p.h"

const QList<AudioDevice> &MpvController::audioDevices() const { return impl_->audioDevices; }

QString MpvController::selectedAudioDevice() const { return impl_->selectedAudioDevice; }

void MpvController::setAudioDevice(const QString &name) { impl_->requestAudioDevice(name, true); }

void MpvController::applyAudioDevicePreference(const QString &name) {
    if (impl_->audioDevices.isEmpty())
        return;
    const auto available =
        std::any_of(impl_->audioDevices.cbegin(), impl_->audioDevices.cend(),
                    [&name](const AudioDevice &device) { return device.name == name; });
    impl_->requestAudioDevice(available ? name : QStringLiteral("auto"), false);
}

void MpvController::Impl::requestAudioDevice(const QString &name, bool persistPreference) {
    const auto available =
        std::any_of(audioDevices.cbegin(), audioDevices.cend(),
                    [&name](const AudioDevice &device) { return device.name == name; });
    if (!available) {
        report(MPV_ERROR_INVALID_PARAMETER);
        return;
    }
    if (name == requestedAudioDevice) {
        persistAudioDeviceRequest = persistAudioDeviceRequest || persistPreference;
        return;
    }
    if (name == selectedAudioDevice) {
        if (persistPreference)
            emit owner->audioDevicePreferenceChanged(name);
        return;
    }

    auto encoded = name.toUtf8();
    char *value = encoded.data();
    const auto id = ++request;
    const auto status =
        mpv_set_property_async(client.get(), id, "audio-device", MPV_FORMAT_STRING, &value);
    if (status >= 0) {
        audioDeviceRequest = id;
        requestedAudioDevice = name;
        persistAudioDeviceRequest = persistPreference;
    }
    report(status);
}

void MpvController::Impl::confirmAudioDevice(const mpv_event &event) {
    if (!audioDeviceRequest || event.reply_userdata != audioDeviceRequest)
        return;
    audioDeviceRequest = 0;
    const auto name = std::exchange(requestedAudioDevice, {});
    const bool persistPreference = std::exchange(persistAudioDeviceRequest, false);
    if (event.error >= 0 && persistPreference)
        emit owner->audioDevicePreferenceChanged(name);
    report(event.error);
}
