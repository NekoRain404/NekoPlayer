#pragma once

#include <QString>

// Durable user intent, independent of track selection and playback progress.
struct SubtitlePreferences {
    bool visible = true;
    double scale = 1;
    double position = 100;
    QString font = QStringLiteral("sans-serif");

    [[nodiscard]] SubtitlePreferences normalized() const;
    bool operator==(const SubtitlePreferences &) const = default;
};
