#pragma once

#include <QString>

struct MediaChapter {
    int index = 0;
    QString title;
    double start = 0;
    bool operator==(const MediaChapter &) const = default;
};
