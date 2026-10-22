#include "m3u8store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringDecoder>
#include <QUrl>

namespace {
constexpr auto CurrentPrefix = "#NEKOPLAYER-CURRENT:";

QString ioError(const QString &path) {
    return QStringLiteral("Unable to read or write playlist: %1").arg(path);
}

bool isUrl(const QString &entry) {
    const QUrl url(entry, QUrl::StrictMode);
    return url.isValid() && !url.scheme().isEmpty();
}

bool hasUrlScheme(const QString &entry) {
    return !QUrl(entry, QUrl::StrictMode).scheme().isEmpty();
}

bool validEntry(const QString &entry) {
    return !entry.isEmpty() && !entry.contains(QChar::Null) && !entry.contains('\n') &&
           !entry.contains('\r');
}

bool parseCurrent(const QString &line, int &index) {
    bool ok = false;
    const auto value = line.mid(QString::fromLatin1(CurrentPrefix).size()).toInt(&ok);
    if (!ok || value < 0)
        return false;
    index = value;
    return true;
}
} // namespace

M3u8LoadResult M3u8Store::load(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {{}, -1, ioError(filePath)};
    const auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError)
        return {{}, -1, ioError(filePath)};

    QStringDecoder decoder(QStringDecoder::Utf8);
    QString text = decoder(bytes);
    if (decoder.hasError())
        return {{}, -1, QStringLiteral("Playlist is not valid UTF-8: %1").arg(filePath)};
    if (!text.isEmpty() && text.front() == QChar(0xfeff))
        text.remove(0, 1);

    QStringList paths;
    int current = -1;
    bool markerSeen = false;
    const auto base = QFileInfo(filePath).absolutePath();
    for (const auto &rawLine : text.split(QLatin1Char('\n'))) {
        auto line = rawLine;
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1String("#NEKOPLAYER-CURRENT"))) {
            if (markerSeen || !parseCurrent(line, current))
                return {{}, -1, QStringLiteral("Invalid current marker: %1").arg(filePath)};
            markerSeen = true;
            continue;
        }
        if (line.startsWith(QLatin1Char('#')))
            continue;
        if (!validEntry(line))
            return {{}, -1, QStringLiteral("Invalid playlist entry: %1").arg(filePath)};
        if (hasUrlScheme(line) && !isUrl(line))
            return {{}, -1, QStringLiteral("Invalid playlist URL: %1").arg(filePath)};
        if (!isUrl(line) && !QFileInfo(line).isAbsolute())
            line = QDir::cleanPath(QDir(base).absoluteFilePath(line));
        paths.append(line);
    }

    if (paths.isEmpty()) {
        if (markerSeen && current != -1)
            return {{}, -1, QStringLiteral("Current marker is out of range: %1").arg(filePath)};
        return {{}, -1, {}};
    }
    if (markerSeen && current >= paths.size())
        return {{}, -1, QStringLiteral("Current marker is out of range: %1").arg(filePath)};
    return {paths, markerSeen ? current : 0, {}};
}

QString M3u8Store::save(const QString &filePath, const QStringList &paths, int currentIndex) {
    if ((paths.isEmpty() && currentIndex != -1) ||
        (!paths.isEmpty() && (currentIndex < 0 || currentIndex >= paths.size())))
        return QStringLiteral("Invalid current index: %1").arg(filePath);
    for (const auto &path : paths) {
        if (!validEntry(path) || path.trimmed() != path)
            return QStringLiteral("Invalid playlist entry: %1").arg(filePath);
    }

    QByteArray contents("#EXTM3U\n");
    if (currentIndex >= 0)
        contents += QByteArray(CurrentPrefix) + QByteArray::number(currentIndex) + '\n';
    for (const auto &path : paths)
        contents += path.toUtf8() + '\n';

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size() || !file.commit()) {
        file.cancelWriting();
        return ioError(filePath);
    }
    return {};
}
