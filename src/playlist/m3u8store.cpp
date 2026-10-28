#include "m3u8store.h"
#include "playback/mediasource.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringDecoder>
#include <QUrl>
#include <cmath>

namespace {
constexpr auto CurrentPrefix = "#NEKOPLAYER-CURRENT:";

QString ioError(const QString &path) {
    return QStringLiteral("Unable to read or write playlist: %1").arg(path);
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
    constexpr qint64 maximumPlaylistBytes = 16 * 1024 * 1024;
    const QFileInfo info(filePath);
    if (!info.isFile() || info.size() > maximumPlaylistBytes)
        return {{}, -1, ioError(filePath), {}};
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {{}, -1, ioError(filePath), {}};
    const auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError)
        return {{}, -1, ioError(filePath), {}};

    QStringDecoder decoder(QStringDecoder::Utf8);
    QString text = decoder(bytes);
    if (decoder.hasError())
        return {{}, -1, QStringLiteral("Playlist is not valid UTF-8: %1").arg(filePath),
                {}};
    if (!text.isEmpty() && text.front() == QChar(0xfeff))
        text.remove(0, 1);

    QStringList paths;
    QList<M3u8Entry> entries;
    M3u8Entry pendingEntry;
    int current = -1;
    bool markerSeen = false;
    const auto base = QFileInfo(filePath).absolutePath();
    int lineNumber = 0;
    for (const auto &rawLine : text.split(QLatin1Char('\n'))) {
        ++lineNumber;
        auto line = rawLine;
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        if (line.isEmpty())
            continue;
        // HLS master/media playlists describe streams, not a queue. Importing
        // them as paths would play a segment and then overwrite the manifest.
        if (line.startsWith(QLatin1String("#EXT-X-")))
            return {{},
                    -1,
                    QStringLiteral("HLS playlists cannot be imported as a queue: %1").arg(filePath),
                    {}};
        if (line.startsWith(QLatin1String("#NEKOPLAYER-CURRENT"))) {
            if (markerSeen || !parseCurrent(line, current))
                return {{}, -1, QStringLiteral("Invalid current marker: %1").arg(filePath), {}};
            markerSeen = true;
            continue;
        }
        // #EXTINF:<seconds>,<title> applies to the next entry. Missing or
        // negative durations are kept as -1, and titles are optional.
        if (line.startsWith(QLatin1String("#EXTINF:"))) {
            const auto body = line.mid(8);
            const auto comma = body.indexOf(QLatin1Char(','));
            bool secondsValid = false;
            const auto secondsText = comma < 0 ? body : body.left(comma);
            const auto seconds = secondsText.trimmed().toDouble(&secondsValid);
            pendingEntry.durationSeconds = secondsValid && seconds >= 0 ? seconds : -1;
            pendingEntry.title = comma < 0 ? QString{} : body.mid(comma + 1).trimmed();
            continue;
        }
        if (line.startsWith(QLatin1Char('#')))
            continue;
        if (!validEntry(line))
            return {{},
                    -1,
                    QStringLiteral("Invalid playlist entry at line %1: %2")
                        .arg(lineNumber)
                        .arg(filePath),
                    {}};
        if (!QDir::isAbsolutePath(line) && !hasUrlScheme(line))
            line = QDir(base).absoluteFilePath(line);
        const auto source = MediaSource::normalize(line);
        if (source.isEmpty())
            return {{},
                    -1,
                    QStringLiteral("Invalid playlist source at line %1: %2")
                        .arg(lineNumber)
                        .arg(filePath),
                    {}};
        // Decode file URLs into the same absolute paths used by local opening
        // and history. Retain network spelling, including signed escape bytes.
        if (QDir::isAbsolutePath(source))
            line = source;
        pendingEntry.source = line;
        entries.append(pendingEntry);
        pendingEntry = M3u8Entry{};
        paths.append(line);
    }

    if (paths.isEmpty()) {
        if (markerSeen && current != -1)
            return {{}, -1, QStringLiteral("Current marker is out of range: %1").arg(filePath),
                {}};
        return {{}, -1, {}, {}};
    }
    if (markerSeen && current >= paths.size())
        return {{}, -1, QStringLiteral("Current marker is out of range: %1").arg(filePath),
                {}};
    return {paths, markerSeen ? current : 0, {}, entries};
}

namespace {
QString sanitizedTitle(QString title) {
    title.remove(QLatin1Char('\r'));
    title.remove(QLatin1Char('\n'));
    title = title.trimmed();
    if (title.size() > 512)
        title = title.left(511) + QChar(0x2026);
    return title;
}
} // namespace

QString M3u8Store::save(const QString &filePath, const QStringList &paths, int currentIndex) {
    QList<M3u8Entry> entries;
    entries.reserve(paths.size());
    for (const auto &path : paths)
        entries.append({path, -1, {}});
    return saveEntries(filePath, entries, currentIndex);
}

QString M3u8Store::saveEntries(const QString &filePath, const QList<M3u8Entry> &entries,
                               int currentIndex) {
    if ((entries.isEmpty() && currentIndex != -1) ||
        (!entries.isEmpty() && (currentIndex < -1 || currentIndex >= entries.size())))
        return QStringLiteral("Invalid current index: %1").arg(filePath);
    for (const auto &entry : entries) {
        if (!validEntry(entry.source) || entry.source.trimmed() != entry.source ||
            MediaSource::normalize(entry.source).isEmpty() || !std::isfinite(entry.durationSeconds))
            return QStringLiteral("Invalid playlist entry: %1").arg(filePath);
    }

    QByteArray contents("#EXTM3U\n");
    if (currentIndex >= 0)
        contents += QByteArray(CurrentPrefix) + QByteArray::number(currentIndex) + '\n';
    for (const auto &entry : entries) {
        const auto title = sanitizedTitle(entry.title);
        if (entry.durationSeconds >= 0 || !title.isEmpty()) {
            contents += "#EXTINF:" +
                        (entry.durationSeconds >= 0
                             ? QByteArray::number(entry.durationSeconds, 'g', 17)
                             : QByteArray("-1")) +
                        ',' + title.toUtf8() + '\n';
        }
        contents += entry.source.toUtf8() + '\n';
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size() ||
        !file.commit()) {
        file.cancelWriting();
        return ioError(filePath);
    }
    return {};
}
