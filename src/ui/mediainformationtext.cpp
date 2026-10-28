#include "mediainformationtext.h"

#include "playback/mediainformation.h"
#include "playback/mpvcontroller.h"

#include <QCoreApplication>
#include <QLocale>
#include <QStringList>
#include <QUrl>

QString formatMediaInformation(const MediaInformation &info, const PlaybackState &state) {
    const auto tr = [](const char *text) {
        return QCoreApplication::translate("MediaInformation", text);
    };
    if (!state.active)
        return tr("未播放媒体");
    const auto unknown = tr("未知");
    const auto text = [&unknown](const QString &value) {
        return value.isEmpty() ? unknown : value;
    };
    // Network sources can carry credentials and signed query parameters. The
    // report and its copy action are meant to be shareable, so show the host
    // and path only and tell the reader that details were withheld. Plain
    // local paths (including paths containing '?') are never rewritten.
    const auto source = [&tr](const QString &path) {
        const QUrl url(path);
        if (!url.isValid() || url.isLocalFile() || url.scheme().isEmpty() || url.host().isEmpty())
            return path;
        const bool withheld =
            !url.userInfo().isEmpty() || url.hasQuery() || !url.fragment().isEmpty();
        if (!withheld)
            return path;
        return url.adjusted(QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment)
                   .toDisplayString() +
               tr("（已隐藏凭据与查询参数）");
    };
    QStringList lines;
    const auto field = [&lines](const QString &label, const QString &value) {
        lines.append(label + QStringLiteral(": ") + value);
    };
    field(tr("文件"), text(source(state.path)));
    field(tr("容器格式"), text(info.fileFormat));
    field(
        tr("文件大小"),
        info.fileSize >= 0
            ? tr("%1（%2 字节）").arg(QLocale().formattedDataSize(info.fileSize)).arg(info.fileSize)
            : unknown);
    lines.append(QString{});
    field(tr("视频编码"), text(info.videoCodec));
    field(tr("视频尺寸"), info.videoWidth > 0 && info.videoHeight > 0
                              ? QStringLiteral("%1 × %2").arg(info.videoWidth).arg(info.videoHeight)
                              : unknown);
    field(tr("像素格式"), text(info.videoPixelFormat));
    const auto dynamicRange =
        info.isHdr() ? tr("HDR · %1").arg(info.videoTransfer.toUpper()) : tr("SDR");
    field(tr("视频色彩"), dynamicRange);
    field(tr("视频原色"), text(info.videoPrimaries));
    field(tr("容器帧率（估计值）"),
          info.containerFrameRate > 0
              ? QStringLiteral("%1 fps").arg(info.containerFrameRate, 0, 'f', 3)
              : unknown);
    field(tr("实际视频解码"), state.hardwareDecoder == QStringLiteral("no")
                                  ? tr("软件解码")
                                  : text(state.hardwareDecoder));
    lines.append(QString{});
    field(tr("音频编码"), text(info.audioCodec));
    field(tr("采样率"),
          info.audioSampleRate > 0 ? QStringLiteral("%1 Hz").arg(info.audioSampleRate) : unknown);
    field(tr("声道"), text(info.audioChannels));
    field(tr("采样格式"), text(info.audioSampleFormat));
    return lines.join(QLatin1Char('\n'));
}
