#pragma once

#include <QStringList>

struct M3u8LoadResult {
    QStringList paths;
    int currentIndex = -1;
    QString error;
};

class M3u8Store final {
  public:
    [[nodiscard]] static M3u8LoadResult load(const QString &filePath);
    [[nodiscard]] static QString save(const QString &filePath, const QStringList &paths,
                                      int currentIndex);
};
