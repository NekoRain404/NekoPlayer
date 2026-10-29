#include "ui/window/windowtitle.h"

#include "playback/media/mediasource.h"

#include <QChar>

QString windowTitleForPath(const QString &path) {
    if (path.isEmpty())
        return QStringLiteral("NekoPlayer");
    auto name = MediaSource::displayName(path);
    for (auto &character : name) {
        const auto category = character.category();
        if (category == QChar::Other_Control || category == QChar::Other_Format ||
            category == QChar::Separator_Line || category == QChar::Separator_Paragraph)
            character = QLatin1Char(' ');
    }
    name = name.simplified();
    if (name.isEmpty())
        return QStringLiteral("NekoPlayer");
    return name + QStringLiteral(" — NekoPlayer");
}
