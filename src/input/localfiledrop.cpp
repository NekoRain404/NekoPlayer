#include "localfiledrop.h"
#include "playback/mpvcontroller.h"

#include <QFileInfo>

LocalFileDrop::LocalFileDrop(MpvController &player, QObject *parent)
    : QObject(parent), player_(player) {}

bool LocalFileDrop::handleUrls(const QList<QUrl> &urls) {
    if (urls.isEmpty()) {
        emit errorOccurred(tr("请拖入本地媒体或字幕文件"));
        return false;
    }
    QStringList mediaPaths;
    QStringList subtitlePaths;
    QStringList folderPaths;
    for (const auto &url : urls) {
        // Qt also calls file://host/path local; do not reinterpret UNC paths
        // as local Linux paths (including the localhost authority spelling).
        if (!url.isValid() || !url.isLocalFile() || !url.host().isEmpty() ||
            url.toLocalFile().isEmpty()) {
            emit errorOccurred(tr("拖放仅支持本地媒体或字幕文件"));
            return false;
        }
        const QFileInfo file(url.toLocalFile());
        if (file.isDir()) {
            folderPaths.append(file.absoluteFilePath());
            continue;
        }
        const auto suffix = file.suffix().toLower();
        static const QStringList subtitleSuffixes{QStringLiteral("srt"), QStringLiteral("ass"),
                                                  QStringLiteral("ssa"), QStringLiteral("sub"),
                                                  QStringLiteral("vtt"), QStringLiteral("sup"),
                                                  QStringLiteral("idx"), QStringLiteral("mks")};
        if (subtitleSuffixes.contains(suffix))
            subtitlePaths.append(file.absoluteFilePath());
        else
            mediaPaths.append(file.absoluteFilePath());
    }
    if (!folderPaths.isEmpty()) {
        if (!mediaPaths.isEmpty() || !subtitlePaths.isEmpty()) {
            emit errorOccurred(tr("请单独拖入一个文件夹；文件夹与媒体或字幕不能混合拖放"));
            return false;
        }
        emit foldersDropped(folderPaths);
        return true;
    }
    if (!subtitlePaths.isEmpty()) {
        if (!mediaPaths.isEmpty() || subtitlePaths.size() != 1) {
            emit errorOccurred(tr("请单独拖入一个字幕文件；媒体文件可分批添加到播放列表"));
            return false;
        }
        if (!player_.state().active) {
            emit errorOccurred(tr("请先播放媒体，再拖入字幕文件"));
            return false;
        }
        player_.addSubtitle(subtitlePaths.constFirst());
    } else
        emit mediaFilesDropped(mediaPaths);
    return true;
}
