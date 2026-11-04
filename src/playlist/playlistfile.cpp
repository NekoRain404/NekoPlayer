#include "playlist/playlistfile.h"
#include "playback/media/mediasource.h"

#include <QDir>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSaveFile>
#include <QStringDecoder>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <cmath>

namespace {
constexpr qint64 MaximumPlaylistBytes = 16 * 1024 * 1024;

QString ioError(const QString &path) {
    return QStringLiteral("Unable to read or write playlist: %1").arg(path);
}

QString readText(const QString &path, QString &error) {
    const QFileInfo info(path);
    if (!info.isFile() || info.size() > MaximumPlaylistBytes) {
        error = ioError(path);
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = ioError(path);
        return {};
    }
    const auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        error = ioError(path);
        return {};
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    QString text = decoder(bytes);
    if (decoder.hasError()) {
        error = QStringLiteral("Playlist is not valid UTF-8: %1").arg(path);
        return {};
    }
    if (!text.isEmpty() && text.front() == QChar(0xfeff))
        text.remove(0, 1);
    return text;
}

QString resolveSource(const QString &raw, const QString &base, QString &error,
                      const QString &filePath) {
    auto source = raw.trimmed();
    if (source.isEmpty() || source.contains(QChar::Null) || source.contains('\n') ||
        source.contains('\r')) {
        error = QStringLiteral("Invalid playlist source in %1").arg(filePath);
        return {};
    }
    if (!QDir::isAbsolutePath(source) && !QUrl(source, QUrl::StrictMode).scheme().size())
        source = QDir(base).absoluteFilePath(source);
    const auto normalized = MediaSource::normalize(source);
    if (normalized.isEmpty()) {
        error = QStringLiteral("Invalid playlist source in %1").arg(filePath);
        return {};
    }
    return QDir::isAbsolutePath(normalized) ? normalized : source;
}

bool validEntry(const M3u8Entry &entry, const QString &filePath, QString &error) {
    if (entry.source.trimmed() != entry.source || MediaSource::normalize(entry.source).isEmpty() ||
        !std::isfinite(entry.durationSeconds)) {
        error = QStringLiteral("Invalid playlist entry in %1").arg(filePath);
        return false;
    }
    return true;
}

QString cleanTitle(QString title) {
    title.remove(QLatin1Char('\r'));
    title.remove(QLatin1Char('\n'));
    title = title.trimmed();
    return title.size() > 512 ? title.left(511) + QChar(0x2026) : title;
}

M3u8LoadResult loadPls(const QString &path) {
    QString error;
    const auto text = readText(path, error);
    if (!error.isEmpty())
        return {{}, -1, error, {}};
    const auto base = QFileInfo(path).absolutePath();
    QMap<int, M3u8Entry> entries;
    const auto lines = text.split(QLatin1Char('\n'));
    for (const auto &raw : lines) {
        auto line = raw;
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char(';')) ||
            line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1Char('[')))
            continue;
        const auto equals = line.indexOf(QLatin1Char('='));
        if (equals <= 0)
            continue;
        const auto key = line.left(equals).trimmed();
        const auto value = line.mid(equals + 1).trimmed();
        int number = -1;
        if (key.startsWith(QLatin1String("File")))
            number = key.mid(4).toInt();
        else if (key.startsWith(QLatin1String("Title")))
            number = key.mid(5).toInt();
        else if (key.startsWith(QLatin1String("Length")))
            number = key.mid(6).toInt();
        else
            continue;
        if (number <= 0 || number > 100000)
            continue;
        auto &entry = entries[number];
        if (key.startsWith(QLatin1String("File"))) {
            entry.source = resolveSource(value, base, error, path);
            if (!error.isEmpty())
                return {{}, -1, error, {}};
        } else if (key.startsWith(QLatin1String("Title"))) {
            entry.title = value;
        } else {
            bool ok = false;
            const auto seconds = value.toDouble(&ok);
            entry.durationSeconds = ok && seconds >= 0 ? seconds : -1;
        }
    }
    QStringList paths;
    QList<M3u8Entry> result;
    for (const auto &entry : entries) {
        if (entry.source.isEmpty())
            continue;
        result.append(entry);
        paths.append(entry.source);
    }
    return {paths, paths.isEmpty() ? -1 : 0, {}, result};
}

M3u8LoadResult loadXspf(const QString &path) {
    QString error;
    const auto text = readText(path, error);
    if (!error.isEmpty())
        return {{}, -1, error, {}};
    const auto base = QFileInfo(path).absolutePath();
    QXmlStreamReader reader(text);
    QStringList paths;
    QList<M3u8Entry> entries;
    QString location;
    QString title;
    double duration = -1;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QLatin1String("track")) {
            location.clear();
            title.clear();
            duration = -1;
            while (!reader.atEnd()) {
                reader.readNext();
                if (reader.isEndElement() && reader.name() == QLatin1String("track"))
                    break;
                if (!reader.isStartElement())
                    continue;
                if (reader.name() == QLatin1String("location"))
                    location = reader.readElementText().trimmed();
                else if (reader.name() == QLatin1String("title"))
                    title = reader.readElementText().trimmed();
                else if (reader.name() == QLatin1String("duration")) {
                    bool ok = false;
                    const auto milliseconds = reader.readElementText().trimmed().toDouble(&ok);
                    duration = ok && milliseconds >= 0 ? milliseconds / 1000.0 : -1;
                }
            }
            if (!location.isEmpty()) {
                auto source = resolveSource(location, base, error, path);
                if (!error.isEmpty())
                    return {{}, -1, error, {}};
                entries.append({source, duration, title});
                paths.append(source);
            }
        }
    }
    if (reader.hasError())
        return {{}, -1, QStringLiteral("Invalid XSPF playlist %1: %2").arg(path, reader.errorString()), {}};
    return {paths, paths.isEmpty() ? -1 : 0, {}, entries};
}

QString savePls(const QString &path, const QList<M3u8Entry> &entries) {
    QByteArray contents("[playlist]\n");
    contents += "NumberOfEntries=" + QByteArray::number(entries.size()) + "\nVersion=2\n";
    for (qsizetype index = 0; index < entries.size(); ++index) {
        const auto number = QByteArray::number(index + 1);
        contents += "File" + number + '=' + entries[index].source.toUtf8() + '\n';
        const auto title = cleanTitle(entries[index].title);
        if (!title.isEmpty())
            contents += "Title" + number + '=' + title.toUtf8() + '\n';
        if (entries[index].durationSeconds >= 0)
            contents += "Length" + number + '=' +
                        QByteArray::number(static_cast<qlonglong>(entries[index].durationSeconds)) +
                        '\n';
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size() ||
        !file.commit()) {
        file.cancelWriting();
        return ioError(path);
    }
    return {};
}

QString saveXspf(const QString &path, const QList<M3u8Entry> &entries) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return ioError(path);
    QXmlStreamWriter writer(&file);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement(QStringLiteral("playlist"));
    writer.writeAttribute(QStringLiteral("version"), QStringLiteral("1"));
    writer.writeAttribute(QStringLiteral("xmlns"), QStringLiteral("http://xspf.org/ns/0/"));
    writer.writeStartElement(QStringLiteral("trackList"));
    for (const auto &entry : entries) {
        writer.writeStartElement(QStringLiteral("track"));
        writer.writeTextElement(QStringLiteral("location"), entry.source);
        if (!cleanTitle(entry.title).isEmpty())
            writer.writeTextElement(QStringLiteral("title"), cleanTitle(entry.title));
        if (entry.durationSeconds >= 0)
            writer.writeTextElement(QStringLiteral("duration"),
                                    QString::number(static_cast<qlonglong>(
                                        std::llround(entry.durationSeconds * 1000.0))));
        writer.writeEndElement();
    }
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndDocument();
    if (writer.hasError() || !file.commit()) {
        file.cancelWriting();
        return ioError(path);
    }
    return {};
}
} // namespace

M3u8LoadResult PlaylistFile::load(const QString &filePath) {
    const auto suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QLatin1String("pls"))
        return loadPls(filePath);
    if (suffix == QLatin1String("xspf"))
        return loadXspf(filePath);
    return M3u8Store::load(filePath);
}

QString PlaylistFile::save(const QString &filePath, const QList<M3u8Entry> &entries,
                           int currentIndex) {
    for (const auto &entry : entries) {
        QString error;
        if (!validEntry(entry, filePath, error))
            return error;
    }
    const auto suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QLatin1String("pls"))
        return savePls(filePath, entries);
    if (suffix == QLatin1String("xspf"))
        return saveXspf(filePath, entries);
    return M3u8Store::saveEntries(filePath, entries, currentIndex);
}
