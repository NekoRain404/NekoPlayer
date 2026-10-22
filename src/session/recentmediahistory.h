#pragma once

#include <QObject>
#include <QStringList>

class RecentMediaHistory final : public QObject {
    Q_OBJECT
  public:
    explicit RecentMediaHistory(QObject *parent = nullptr);

    [[nodiscard]] const QStringList &entries() const;
    [[nodiscard]] QString load(const QString &path);
    [[nodiscard]] QString save() const;
    void remember(const QString &source);
    void clear();

  signals:
    void changed();

  private:
    QStringList entries_;
    QString path_;
    QString loadError_;
};
