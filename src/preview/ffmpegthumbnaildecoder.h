#pragma once

#include <QImage>
#include <QSize>
#include <QString>
#include <atomic>
#include <memory>

struct ThumbnailRequest {
    QString source;
    double seconds = 0.0;
    QSize maximumSize = QSize(256, 144);
    std::shared_ptr<std::atomic_bool> cancellation;
};

struct ThumbnailResult {
    QImage image;
    double timestamp = 0.0;
    QString error;

    [[nodiscard]] bool succeeded() const { return !image.isNull() && error.isEmpty(); }
};

// Decodes one local video frame without touching the primary libmpv instance.
ThumbnailResult decodeThumbnail(const ThumbnailRequest &request);
