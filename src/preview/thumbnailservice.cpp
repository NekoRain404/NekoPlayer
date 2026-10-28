#include "thumbnailservice.h"

#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <QUrl>
#include <cmath>

namespace {
constexpr int maximumCacheEntries = 12;

QSize normalizedSize(const QSize &size) {
    return QSize(qMax(1, size.width()), qMax(1, size.height()));
}

ThumbnailResult failure(QString message) {
    ThumbnailResult result;
    result.error = std::move(message);
    return result;
}
} // namespace

ThumbnailService::ThumbnailService(QObject *parent) : QObject(parent) {
    pool_.setMaxThreadCount(1);
    pool_.setExpiryTimeout(1000);
}

ThumbnailService::~ThumbnailService() {
    cancel();
    pool_.waitForDone();
}

quint64 ThumbnailService::request(const QString &source, double seconds, QSize maximumSize) {
    Q_ASSERT(thread() == QThread::currentThread());

    maximumSize = normalizedSize(maximumSize);
    const auto path = localPath(source);
    if (path.isEmpty()) {
        cancel();
        const auto requestId = ++generation_;
        const auto result = failure(QStringLiteral("thumbnail source must be a local file"));
        QMetaObject::invokeMethod(
            this,
            [this, requestId, source, seconds, result] {
                deliver(requestId, source, seconds, QString(), result);
            },
            Qt::QueuedConnection);
        return requestId;
    }
    const QFileInfo file(path);
    if (!file.exists() || !file.isFile()) {
        cancel();
        const auto requestId = ++generation_;
        const auto result = failure(QStringLiteral("thumbnail source does not exist"));
        QMetaObject::invokeMethod(
            this,
            [this, requestId, path, seconds, result] {
                deliver(requestId, path, seconds, QString(), result);
            },
            Qt::QueuedConnection);
        return requestId;
    }
    if (!std::isfinite(seconds) || seconds < 0.0) {
        cancel();
        const auto requestId = ++generation_;
        const auto result = failure(QStringLiteral("thumbnail timestamp is invalid"));
        QMetaObject::invokeMethod(
            this,
            [this, requestId, path, seconds, result] {
                deliver(requestId, path, seconds, QString(), result);
            },
            Qt::QueuedConnection);
        return requestId;
    }

    // The preview UI hovers the timeline at whole-second granularity, so target
    // and cache by the second floor. Sub-second jitter then reuses one decode
    // instead of evicting the cache entry a fraction of a second later.
    const auto quantizedSeconds = std::floor(seconds);
    const auto key = cacheKey(path, quantizedSeconds, maximumSize);

    // Dragging the timeline produces a burst of requests for the same second.
    // Reuse the decode already running for this exact target instead of
    // cancelling it and starting the same work again.
    if (inFlightRequestId_ != 0 && inFlightKey_ == key)
        return inFlightRequestId_;

    cancel();
    const auto requestId = ++generation_;

    if (const auto cached = cache_.constFind(key); cached != cache_.cend()) {
        ThumbnailResult result;
        result.image = cached.value();
        result.timestamp = quantizedSeconds;
        QMetaObject::invokeMethod(
            this,
            [this, requestId, path, quantizedSeconds, key, result] {
                deliver(requestId, path, quantizedSeconds, key, result);
            },
            Qt::QueuedConnection);
        return requestId;
    }

    const auto cancellation = std::make_shared<std::atomic_bool>(false);
    cancellation_ = cancellation;
    inFlightRequestId_ = requestId;
    inFlightKey_ = key;
    const QPointer<ThumbnailService> guard(this);
    pool_.start([guard, requestId, path, quantizedSeconds, maximumSize, key, cancellation] {
        ThumbnailRequest request;
        request.source = path;
        request.seconds = quantizedSeconds;
        request.maximumSize = maximumSize;
        request.cancellation = cancellation;
        auto result = decodeThumbnail(request);
        if (!guard)
            return;
        QMetaObject::invokeMethod(
            guard.data(),
            [guard, requestId, path, quantizedSeconds, key, result = std::move(result)]() mutable {
                if (guard)
                    guard->deliver(requestId, path, quantizedSeconds, key, std::move(result));
            },
            Qt::QueuedConnection);
    });
    return requestId;
}

void ThumbnailService::cancel() {
    Q_ASSERT(thread() == QThread::currentThread());
    ++generation_;
    if (cancellation_)
        cancellation_->store(true, std::memory_order_relaxed);
    cancellation_.reset();
    inFlightRequestId_ = 0;
    inFlightKey_.clear();
    pool_.clear();
}

void ThumbnailService::clearCache() {
    Q_ASSERT(thread() == QThread::currentThread());
    cache_.clear();
    cacheOrder_.clear();
}

qsizetype ThumbnailService::cacheSize() const {
    Q_ASSERT(thread() == QThread::currentThread());
    return cache_.size();
}

bool ThumbnailService::isLocalSource(const QString &source) { return !localPath(source).isEmpty(); }

void ThumbnailService::deliver(quint64 requestId, const QString &source, double requestedSeconds,
                               const QString &key, ThumbnailResult result) {
    Q_ASSERT(thread() == QThread::currentThread());
    if (requestId != generation_)
        return;
    if (requestId == inFlightRequestId_) {
        inFlightRequestId_ = 0;
        inFlightKey_.clear();
    }
    if (result.succeeded() && !key.isEmpty())
        store(key, result.image);
    emit thumbnailReady(requestId, source, requestedSeconds, std::move(result));
}

QString ThumbnailService::localPath(const QString &source) {
    if (source.trimmed().isEmpty())
        return {};
    const QUrl url(source);
    if (url.isValid() && !url.scheme().isEmpty()) {
        if (url.scheme().compare(QStringLiteral("file"), Qt::CaseInsensitive) != 0)
            return {};
        return url.toLocalFile();
    }
    return source;
}

QString ThumbnailService::cacheKey(const QString &source, double seconds,
                                   const QSize &maximumSize) {
    return source + QChar(0x1f) + QString::number(seconds, 'g', 17) + QChar(0x1f) +
           QString::number(maximumSize.width()) + QChar('x') +
           QString::number(maximumSize.height());
}

void ThumbnailService::store(const QString &key, const QImage &image) {
    cache_.insert(key, image);
    cacheOrder_.removeAll(key);
    cacheOrder_.prepend(key);
    while (cacheOrder_.size() > maximumCacheEntries) {
        const auto evicted = cacheOrder_.takeLast();
        cache_.remove(evicted);
    }
}
