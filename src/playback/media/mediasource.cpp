#include "playback/media/mediasource.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <algorithm>

namespace MediaSource {
QString networkUrl(const QString &input) {
    const auto text = input.trimmed();
    if (text.isEmpty() || std::any_of(text.cbegin(), text.cend(), [](QChar character) {
            return character.unicode() < 0x20 || character.unicode() == 0x7f;
        }))
        return {};
    const QUrl url(text, QUrl::StrictMode);
    const auto scheme = url.scheme().toLower();
    if (!url.isValid() || url.host().isEmpty() ||
        (scheme != QLatin1String("http") && scheme != QLatin1String("https") &&
         scheme != QLatin1String("rtsp") && scheme != QLatin1String("rtsps") &&
         scheme != QLatin1String("rtmp") && scheme != QLatin1String("rtmps")))
        return {};
    // QUrl canonicalizes escapes such as %7e and %2f. Preserve the original path
    // and query bytes: signed links can depend on that exact representation.
    auto authorityEnd = text.size();
    const auto authorityStart = text.indexOf(QLatin1String("://")) + 3;
    for (const auto separator : {'/', '?', '#'}) {
        const auto at = text.indexOf(QLatin1Char(separator), authorityStart);
        if (at >= 0)
            authorityEnd = std::min(authorityEnd, at);
    }
    const auto authority =
        url.toEncoded(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment);
    auto tail = QUrl::toPercentEncoding(text.mid(authorityEnd), ":/?#[]@!$&'()*+,;=");
    // Qt always escapes '%', even in the exclude set. StrictMode already
    // validated every original escape, so restore those percent signs once.
    tail.replace("%25", "%");
    return QString::fromLatin1(authority + tail);
}

bool isNetwork(const QString &source) { return !networkUrl(source).isEmpty(); }

QString normalize(const QString &source) {
    if (source.isEmpty() || source.contains(QChar::Null))
        return {};
    if (QDir::isAbsolutePath(source))
        return QDir::cleanPath(source);
    const QUrl url(source, QUrl::StrictMode);
    if (url.isLocalFile()) {
        // Match drag-and-drop: a host component makes the authority ambiguous
        // (UNC/localhost spelling), so reject instead of reinterpreting it.
        if (!url.host().isEmpty())
            return {};
        const auto path = url.toLocalFile();
        if (!url.isValid() || url.hasQuery() || url.hasFragment() || path.contains(QChar::Null) ||
            !QDir::isAbsolutePath(path))
            return {};
        return QDir::cleanPath(path);
    }
    if (!url.scheme().isEmpty() || source.contains(QLatin1String("://")))
        return networkUrl(source);
    return QFileInfo(source).absoluteFilePath();
}

QString displayName(const QString &source) {
    const QUrl url(source, QUrl::StrictMode);
    if (url.isValid() && !url.scheme().isEmpty() && !url.isLocalFile()) {
        const auto name = url.fileName(QUrl::FullyDecoded);
        return name.isEmpty() ? url.host() : name;
    }
    return QFileInfo(url.isLocalFile() ? url.toLocalFile() : source).fileName();
}
} // namespace MediaSource
