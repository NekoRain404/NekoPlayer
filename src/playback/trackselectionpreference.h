#pragma once

#include <QString>
#include <QStringList>
#include <optional>

struct TrackSelectionPreference {
    std::optional<qint64> audioTrackId;
    std::optional<qint64> subtitleTrackId;
    QString selectedExternalAudioPath;
    int selectedExternalAudioStream = -1;
    QStringList externalAudioPaths;
    QString selectedExternalSubtitlePath;
    int selectedExternalSubtitleStream = -1;
    QStringList externalSubtitlePaths;
};
