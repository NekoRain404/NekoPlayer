#pragma once

#include <QObject>
#include <QStringList>
#include <atomic>
#include <memory>

class FolderScanJob final : public QObject {
    Q_OBJECT
  public:
    explicit FolderScanJob(QObject *parent = nullptr);
    ~FolderScanJob() override;
    [[nodiscard]] bool isScanning() const;

  public slots:
    void start(const QString &directory, bool recursive = false);
    void cancel();

  signals:
    void scanningChanged();
    void completed(const QStringList &paths);
    void failed(const QString &message);

  private:
    std::shared_ptr<std::atomic_bool> cancellation_;
    quint64 generation_ = 0;
    bool scanning_ = false;
};
