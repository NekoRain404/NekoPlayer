#include "playbackoverlaytext.h"

#include "playback/mediainformation.h"
#include "playback/mediasource.h"
#include "playback/mpvcontroller.h"
#include "playback/playbacktime.h"

#include <QCoreApplication>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace {
QString tr(const char *text) { return QCoreApplication::translate("PlaybackOverlay", text); }

QString displayText(const QString &value) {
    auto result = value.left(160);
    for (auto &character : result) {
        const auto category = character.category();
        if (category == QChar::Other_Control || category == QChar::Other_Format ||
            category == QChar::Separator_Line || category == QChar::Separator_Paragraph)
            character = QLatin1Char(' ');
    }
    result = result.trimmed();
    if (result.isEmpty())
        return tr("未知");
    if (value.size() > 160)
        result = result.left(159) + QChar(0x2026);
    return result;
}
} // namespace

SeekProgressPresentation formatSeekProgress(const PlaybackState &state) {
    if (!state.active || !state.seekable || !std::isfinite(state.duration) || state.duration <= 0 ||
        !std::isfinite(state.position))
        return {};
    const auto ratio = std::clamp(state.position / state.duration, 0.0, 1.0);
    return {true, static_cast<int>(std::lround(ratio * 10000)),
            QStringLiteral("%1 / %2  (%3%)")
                .arg(formatPlaybackTime(state.position), formatPlaybackTime(state.duration))
                .arg(ratio * 100, 0, 'f', 1)};
}

QString formatPlaybackOverlayInformation(const MediaInformation &info, const PlaybackState &state) {
    if (!state.active)
        return tr("未播放媒体");

    const auto unknown = tr("未知");
    const auto hasVideo = !info.videoCodec.isEmpty() || info.videoWidth > 0 || info.videoHeight > 0;
    const auto dimensions =
        info.videoWidth > 0 && info.videoHeight > 0
            ? QStringLiteral("%1 × %2").arg(info.videoWidth).arg(info.videoHeight)
            : unknown;
    const auto frameRate = std::isfinite(info.containerFrameRate) && info.containerFrameRate > 0
                               ? QStringLiteral("%1 fps").arg(info.containerFrameRate, 0, 'f', 3)
                               : unknown;
    const auto speed = std::isfinite(state.playbackSpeed) && state.playbackSpeed > 0
                           ? QStringLiteral("%1×").arg(state.playbackSpeed, 0, 'f', 2)
                           : unknown;
    const auto decoder = state.hardwareDecoder == QLatin1String("no")
                             ? tr("软件解码")
                             : displayText(state.hardwareDecoder);
    const auto sampleRate =
        info.audioSampleRate > 0 ? QStringLiteral("%1 Hz").arg(info.audioSampleRate) : unknown;
    QStringList lines;
    lines << tr("媒体: %1").arg(displayText(MediaSource::displayName(state.path)))
          << tr("时间: %1 / %2")
                 .arg(formatPlaybackTime(state.position), formatPlaybackTime(state.duration))
          << tr("状态: %1 · 速度 %2").arg(state.paused ? tr("已暂停") : tr("播放中"), speed)
          << (hasVideo ? tr("视频: %1 · %2 · %3")
                             .arg(displayText(info.videoCodec), dimensions,
                                  displayText(info.videoPixelFormat))
                       : tr("视频: 无视频"))
          << tr("容器帧率（估计）: %1").arg(hasVideo ? frameRate : tr("无视频"))
          << tr("实际视频解码: %1").arg(hasVideo ? decoder : tr("无视频"))
          << tr("音频: %1 · %2 · %3")
                 .arg(displayText(info.audioCodec), sampleRate, displayText(info.audioChannels));
    return lines.join(QLatin1Char('\n'));
}
