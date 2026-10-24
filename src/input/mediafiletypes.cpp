#include "mediafiletypes.h"

const QStringList &supportedMediaExtensions() {
    static const QStringList extensions{
        QStringLiteral("mkv"),  QStringLiteral("mp4"),  QStringLiteral("avi"),
        QStringLiteral("mov"),  QStringLiteral("webm"), QStringLiteral("m4v"),
        QStringLiteral("ts"),   QStringLiteral("m2ts"), QStringLiteral("mts"),
        QStringLiteral("mpg"),  QStringLiteral("mpeg"), QStringLiteral("wmv"),
        QStringLiteral("flv"),  QStringLiteral("ogv"),  QStringLiteral("3gp"),
        QStringLiteral("vob"),  QStringLiteral("mp3"),  QStringLiteral("flac"),
        QStringLiteral("wav"),  QStringLiteral("m4a"),  QStringLiteral("aac"),
        QStringLiteral("ogg"),  QStringLiteral("opus"), QStringLiteral("wma"),
        QStringLiteral("aiff"), QStringLiteral("aif"),  QStringLiteral("mka"),
        QStringLiteral("ac3"),  QStringLiteral("dts"),  QStringLiteral("ape")};
    return extensions;
}

QString mediaFileNameFilterPatterns() {
    QStringList patterns;
    patterns.reserve(supportedMediaExtensions().size());
    for (const auto &extension : supportedMediaExtensions())
        patterns.append(QStringLiteral("*.") + extension);
    return patterns.join(QLatin1Char(' '));
}
