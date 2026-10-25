#include "windowtitle.h"

#include "playback/mediasource.h"

QString windowTitleForPath(const QString &path) {
    if (path.isEmpty())
        return QStringLiteral("NekoPlayer");
    return MediaSource::displayName(path) + QStringLiteral(" — NekoPlayer");
}
