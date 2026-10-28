#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <atomic>
#include <memory>

struct MediaLibraryEntry {
    QString path;
    QString displayName;
};

// Watched media folders and their indexed files. Persistence and scanning are
// owned by this object; only the cancellable QtConcurrent worker runs off the
// GUI thread, and it receives value copies plus a shared cancellation flag.
class MediaLibrary final : public QObject {
    Q_OBJECT
  public:
    static constexpr int MaximumFolders = 50;
    static constexpr int MaximumEntries = 100000;
    static constexpr int MaximumPathLength = 8192;

    explicit MediaLibrary(QString filePath, QObject *parent = nullptr);
    ~MediaLibrary() override;

    [[nodiscard]] const QStringList &folders() const;
    [[nodiscard]] bool addFolder(const QString &absoluteDir);
    [[nodiscard]] bool removeFolder(const QString &absoluteDir);

    [[nodiscard]] const QList<MediaLibraryEntry> &entries() const;
    [[nodiscard]] bool recursive() const;
    void setRecursive(bool recursive);

    // Both return an empty string on success. A refused save leaves an unknown
    // or malformed configuration untouched. With configuration disabled the
    // library stays in memory and never touches the file system.
    [[nodiscard]] QString load();
    [[nodiscard]] QString save();
    void clearEntries();

    void startScan();
    void cancelScan();
    [[nodiscard]] bool isScanning() const;

  signals:
    void foldersChanged();
    void entriesChanged();
    void recursiveChanged(bool recursive);
    void scanStarted(int folderCount);
    void scanProgress(int foldersProcessed, int folderCount);
    void scanFinished(bool cancelled, const QString &error);

  private:
    void scanNextFolder(quint64 generation);
    void finishScan(const QString &error);
    QString filePath_;
    QStringList folders_;
    QList<MediaLibraryEntry> entries_;
    bool recursive_ = false;
    bool scanning_ = false;
    quint64 scanGeneration_ = 0;
    int scanIndex_ = 0;
    bool scanOverflow_ = false;
    QStringList scanFolders_;
    QStringList scanPaths_;
    QStringList scanErrors_;
    std::shared_ptr<std::atomic_bool> cancellation_;
};
