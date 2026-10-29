#pragma once

#include <QString>

// Durable user intent, independent of track selection and playback progress.
struct SubtitlePreferences {
    bool visible = true;
    double scale = 1;
    double position = 100;
    QString font = QStringLiteral("sans-serif");
    // Colors use mpv's #AARRGGBB spelling; defaults match libmpv.
    QString textColor = QStringLiteral("#FFFFFFFF");
    QString outlineColor = QStringLiteral("#FF000000");
    QString backgroundColor = QStringLiteral("#AF000000");
    double outlineSize = 1.65;
    double shadowOffset = 0;
    // "auto" or an explicit codepage such as UTF-8 / GB18030.
    QString codepage = QStringLiteral("auto");

    [[nodiscard]] SubtitlePreferences normalized() const;
    bool operator==(const SubtitlePreferences &) const = default;
};
