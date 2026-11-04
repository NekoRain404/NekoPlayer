#pragma once

#include <QObject>
#include <QStringList>

// Cross-platform single-instance handoff. The first process owns a local
// endpoint named after the configuration directory; later processes forward
// their command-line arguments as one JSON line and exit.
class SingleInstance final : public QObject {
    Q_OBJECT
  public:
    enum class StartResult { Forwarded, Owner, Unavailable };
    explicit SingleInstance(QObject *parent = nullptr);
    ~SingleInstance() override;
    [[nodiscard]] StartResult start(const QStringList &arguments);
    // Resolved endpoint: an absolute socket path on Unix, a pipe name on Windows.
    [[nodiscard]] QString endpointName() const;

  signals:
    void argumentsReceived(const QStringList &arguments);

  private:
    void dispatch(const QStringList &arguments);

    class Private;
    Private *d_;
};
