#include "playbackoverlaytext.h"

#include "playback/mediainformation.h"
#include "playback/mediasource.h"
#include "playback/mediatrack.h"
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

QString byteSize(qint64 bytes) {
    if (bytes < 0)
        return tr("未知");
    if (bytes < 1024)
        return tr("%1 字节").arg(bytes);
    constexpr qint64 kib = 1024;
    constexpr qint64 mib = kib * 1024;
    constexpr qint64 gib = mib * 1024;
    if (bytes >= gib)
        return QStringLiteral("%1 GiB (%2 字节)").arg(bytes / double(gib), 0, 'f', 2).arg(bytes);
    if (bytes >= mib)
        return QStringLiteral("%1 MiB (%2 字节)").arg(bytes / double(mib), 0, 'f', 2).arg(bytes);
    return QStringLiteral("%1 KiB (%2 字节)").arg(bytes / double(kib), 0, 'f', 2).arg(bytes);
}

QString selectedTrack(const QList<MediaTrack> &tracks, MediaTrack::Type type) {
    for (const auto &track : tracks) {
        if (track.type != type || !track.selected)
            continue;
        QStringList details{tr("轨道 %1").arg(track.id)};
        if (!track.language.isEmpty())
            details << displayText(track.language);
        if (!track.title.isEmpty())
            details << displayText(track.title);
        if (!track.codec.isEmpty())
            details << displayText(track.codec);
        if (track.external)
            details << tr("外部");
        return details.join(QStringLiteral(" · "));
    }
    return type == MediaTrack::Type::Audio ? tr("无音频轨道") : tr("未选择字幕");
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

QString formatPlaybackOverlayInformation(const MediaInformation &info, const PlaybackState &state,
                                         const QList<MediaTrack> &tracks) {
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
    const auto dynamicRange =
        info.isHdr() ? QStringLiteral("HDR · %1").arg(info.videoTransfer.toUpper()) : tr("SDR");
    const auto sampleRate =
        info.audioSampleRate > 0 ? QStringLiteral("%1 Hz").arg(info.audioSampleRate) : unknown;
    const auto progress = formatSeekProgress(state);
    const auto remaining = state.seekable && std::isfinite(state.duration) &&
                                   std::isfinite(state.position) && state.duration > 0
                               ? formatPlaybackTime(std::max(0.0, state.duration - state.position))
                               : unknown;
    const auto volume = std::isfinite(state.volume)
                            ? QStringLiteral("%1%%2")
                                  .arg(std::clamp(state.volume, 0.0, 100.0), 0, 'f', 1)
                                  .arg(state.muted ? tr("（静音）") : QString())
                            : unknown;
    const auto signedDelay = [](double seconds) {
        if (!std::isfinite(seconds))
            return tr("未知");
        return QStringLiteral("%1%2 ms")
            .arg(seconds > 0 ? QLatin1Char('+') : QLatin1Char(' '))
            .arg(qRound64(seconds * 1000));
    };
    QStringList lines;
    lines
        << tr("媒体: %1").arg(displayText(MediaSource::displayName(state.path)))
        << tr("时间: %1 / %2")
               .arg(formatPlaybackTime(state.position), formatPlaybackTime(state.duration))
        << tr("状态: %1 · 速度 %2").arg(state.paused ? tr("已暂停") : tr("播放中"), speed)
        << (hasVideo ? tr("视频: %1 · %2 · %3")
                           .arg(displayText(info.videoCodec), dimensions,
                                displayText(info.videoPixelFormat))
                     : tr("视频: 无视频"))
        << tr("容器帧率（估计）: %1").arg(hasVideo ? frameRate : tr("无视频"))
        << tr("实际视频解码: %1").arg(hasVideo ? decoder : tr("无视频"))
        << tr("视频色彩: %1 · 原色: %2")
               .arg(hasVideo ? dynamicRange : tr("无视频"),
                    hasVideo ? displayText(info.videoPrimaries) : tr("无视频"))
        << tr("容器: %1 · 文件大小: %2").arg(displayText(info.fileFormat), byteSize(info.fileSize))
        << tr("进度: %1 · 剩余: %2")
               .arg(progress.available ? progress.text : tr("不可定位"), remaining)
        << tr("音量: %1 · 音频延迟: %2 · 字幕延迟: %3")
               .arg(volume, signedDelay(state.audioDelay), signedDelay(state.subtitleDelay))
        << tr("音频: %1 · %2 · %3 · %4")
               .arg(displayText(info.audioCodec), sampleRate, displayText(info.audioChannels),
                    displayText(info.audioSampleFormat))
        << tr("当前音轨: %1").arg(selectedTrack(tracks, MediaTrack::Type::Audio))
        << tr("当前字幕: %1").arg(selectedTrack(tracks, MediaTrack::Type::Subtitle));
    return lines.join(QLatin1Char('\n'));
}
