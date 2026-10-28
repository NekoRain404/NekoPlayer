#pragma once

#include "ffmpegthumbnaildecoder.h"

#include <QHash>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QStringList>
#include <QThreadPool>
#include <atomic>
#include <memory>

class ThumbnailService final : public QObject {
    Q_OBJECT

  public:
    explicit ThumbnailService(QObject *parent = nullptr);
    ~ThumbnailService() override;

    // The latest call owns the pending decode; an identical in-flight target is
    // coalesced and returns its existing id. All methods must run on this
    // object's thread.
    [[nodiscard]] quint64 request(const QString &source, double seconds,
                                  QSize maximumSize = QSize(256, 144));
    void cancel();
    void clearCache();

    [[nodiscard]] qsizetype cacheSize() const;
    [[nodiscard]] static bool isLocalSource(const QString &source);

  signals:
    void thumbnailReady(quint64 requestId, const QString &source, double requestedSeconds,
                        ThumbnailResult result);

  private:
    void deliver(quint64 requestId, const QString &source, double requestedSeconds,
                 const QString &cacheKey, ThumbnailResult result);
    [[nodiscard]] static QString localPath(const QString &source);
    [[nodiscard]] static QString cacheKey(const QString &source, double seconds,
                                          const QSize &maximumSize);
    void store(const QString &key, const QImage &image);

    QThreadPool pool_;
    std::shared_ptr<std::atomic_bool> cancellation_;
    quint64 generation_ = 0;
    quint64 inFlightRequestId_ = 0;
    QString inFlightKey_;
    QHash<QString, QImage> cache_;
    QStringList cacheOrder_;
};

Q_DECLARE_METATYPE(ThumbnailResult)
