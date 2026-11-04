#include "playback/mpv/mpvcontroller_p.h"

#include <QByteArray>
#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr std::array equalizerBands{31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
constexpr std::array<std::array<int, 10>, 7> presetGains{{
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{5, 4, 3, 1, -1, -1, 0, 2, 3, 4}},
    {{-1, 0, 2, 4, 4, 2, 0, -1, -1, -1}},
    {{4, 3, 1, 2, -1, -1, 0, 1, 2, 3}},
    {{4, 3, 2, 1, 0, 0, 1, 2, 3, 4}},
    {{-3, -2, -1, 1, 3, 4, 4, 2, 0, -1}},
    {{6, 5, 4, 2, 1, 0, 0, 0, 0, 0}},
}};
QByteArray equalizerChain(const QList<double> &gains) {
    if (gains.size() != static_cast<qsizetype>(equalizerBands.size()))
        return {};
    QByteArray chain;
    for (size_t index = 0; index < equalizerBands.size(); ++index)
        chain += "equalizer=f=" + QByteArray::number(equalizerBands[index]) +
                 ":width_type=o:width=1:g=" +
                 QByteArray::number(
                     std::clamp(gains.at(static_cast<qsizetype>(index)), -12.0, 12.0), 'g', 17) +
                 ',';
    chain.chop(1);
    return chain;
}
QByteArray equalizerChain(EqualizerPreset preset) {
    if (preset == EqualizerPreset::Off)
        return {};
    const auto &gains = presetGains[static_cast<size_t>(preset) - 1];
    QList<double> values;
    for (const auto gain : gains)
        values.append(gain);
    return equalizerChain(values);
}
} // namespace

EqualizerPreset MpvController::equalizerPreset() const {
    return static_cast<EqualizerPreset>(property("equalizerPreset").toInt());
}

QList<double> MpvController::equalizerGains() const {
    if (impl_->customEqualizer)
        return impl_->customEqualizerGains;
    QList<double> gains;
    const auto preset = equalizerPreset();
    if (preset == EqualizerPreset::Off) {
        for (size_t index = 0; index < equalizerBands.size(); ++index)
            gains.append(0.0);
        return gains;
    }
    const auto &values = presetGains[static_cast<size_t>(preset) - 1];
    for (const auto gain : values)
        gains.append(gain);
    return gains;
}

bool MpvController::equalizerCustom() const { return impl_->customEqualizer; }

void MpvController::setEqualizerPreset(EqualizerPreset preset) {
    if (preset == equalizerPreset() && !impl_->customEqualizer)
        return;
    if (!impl_->command({"set", "af", equalizerChain(preset)}))
        return;
    impl_->customEqualizer = false;
    impl_->customEqualizerGains.clear();
    setProperty("equalizerPreset", static_cast<int>(preset));
    emit equalizerPresetChanged();
}

void MpvController::setEqualizerGains(const QList<double> &gains) {
    if (gains.size() != 10)
        return;
    QList<double> normalized;
    for (const auto gain : gains) {
        if (!std::isfinite(gain))
            return;
        normalized.append(std::clamp(gain, -12.0, 12.0));
    }
    if (impl_->customEqualizer && impl_->customEqualizerGains == normalized)
        return;
    if (!impl_->command({"set", "af", equalizerChain(normalized)}))
        return;
    impl_->customEqualizerGains = normalized;
    impl_->customEqualizer = true;
    emit equalizerPresetChanged();
}
