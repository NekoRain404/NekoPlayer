#include "desktop/mprisplayer.h"

#include "playback/mpv/mpvcontroller.h"
#include "playback/playlist/playlistcontroller.h"
#include "playback/playlist/playlistmodel.h"

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QFileInfo>
#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <QWidget>
#include <algorithm>
#include <cmath>

namespace {
constexpr auto serviceName = "org.mpris.MediaPlayer2.NekoPlayer";
constexpr auto objectPath = "/org/mpris/MediaPlayer2";
constexpr auto playerInterface = "org.mpris.MediaPlayer2.Player";
constexpr auto trackPath = "/org/nekoplayer/Track/0";

QString displayTitle(const MpvController &player) {
    const auto path = player.state().path;
    if (path.isEmpty())
        return {};
    const QFileInfo file(path);
    const auto name = file.fileName();
    return name.isEmpty() ? path : name;
}
} // namespace

class MprisRootAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit CONSTANT)
    Q_PROPERTY(bool CanRaise READ canRaise CONSTANT)
    Q_PROPERTY(bool HasTrackList READ hasTrackList CONSTANT)
    Q_PROPERTY(QString Identity READ identity CONSTANT)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry CONSTANT)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes CONSTANT)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes CONSTANT)
  public:
    explicit MprisRootAdaptor(MprisPlayer *parent)
        : QDBusAbstractAdaptor(parent), player_(parent) {}

    [[nodiscard]] bool canQuit() const { return true; }
    [[nodiscard]] bool canRaise() const { return true; }
    [[nodiscard]] bool hasTrackList() const { return false; }
    [[nodiscard]] QString identity() const { return QStringLiteral("NekoPlayer"); }
    [[nodiscard]] QString desktopEntry() const { return QStringLiteral("nekoplayer"); }
    [[nodiscard]] QStringList supportedUriSchemes() const {
        return {QStringLiteral("file"), QStringLiteral("http"), QStringLiteral("https"),
                QStringLiteral("rtsp"), QStringLiteral("rtsps"), QStringLiteral("rtmp"),
                QStringLiteral("rtmps")};
    }
    [[nodiscard]] QStringList supportedMimeTypes() const {
        return {QStringLiteral("video/mp4"), QStringLiteral("video/x-matroska"),
                QStringLiteral("video/webm"), QStringLiteral("audio/mpeg"),
                QStringLiteral("audio/flac"), QStringLiteral("audio/ogg")};
    }

  public slots:
    void Raise() { player_->raise(); }
    void Quit() { player_->quit(); }

  private:
    MprisPlayer *player_;
};

class MprisPlayerAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(QString LoopStatus READ loopStatus WRITE setLoopStatus)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(bool Shuffle READ shuffle WRITE setShuffle)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double MinimumRate READ minimumRate CONSTANT)
    Q_PROPERTY(double MaximumRate READ maximumRate CONSTANT)
    Q_PROPERTY(bool CanGoNext READ canGoNext)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
    Q_PROPERTY(bool CanPlay READ canPlay CONSTANT)
    Q_PROPERTY(bool CanPause READ canPause CONSTANT)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ canControl CONSTANT)
  public:
    explicit MprisPlayerAdaptor(MprisPlayer *parent)
        : QDBusAbstractAdaptor(parent), player_(parent) {}

    [[nodiscard]] QString playbackStatus() const { return player_->playbackStatus(); }
    [[nodiscard]] QString loopStatus() const { return player_->loopStatus(); }
    [[nodiscard]] double rate() const { return player_->rate(); }
    [[nodiscard]] bool shuffle() const { return player_->shuffle(); }
    [[nodiscard]] QVariantMap metadata() const { return player_->metadata(); }
    [[nodiscard]] double volume() const { return player_->volume(); }
    [[nodiscard]] qlonglong position() const { return player_->position(); }
    [[nodiscard]] double minimumRate() const { return player_->minimumRate(); }
    [[nodiscard]] double maximumRate() const { return player_->maximumRate(); }
    [[nodiscard]] bool canGoNext() const { return player_->canGoNext(); }
    [[nodiscard]] bool canGoPrevious() const { return player_->canGoPrevious(); }
    [[nodiscard]] bool canPlay() const { return true; }
    [[nodiscard]] bool canPause() const { return true; }
    [[nodiscard]] bool canSeek() const { return player_->canSeek(); }
    [[nodiscard]] bool canControl() const { return true; }

    void setLoopStatus(const QString &status) { player_->setLoopStatus(status); }
    void setRate(double value) { player_->setRate(value); }
    void setShuffle(bool enabled) { player_->setShuffle(enabled); }
    void setVolume(double value) { player_->setVolume(value); }

  public slots:
    void Next() { player_->next(); }
    void Previous() { player_->previous(); }
    void Pause() { player_->pause(); }
    void PlayPause() { player_->playPause(); }
    void Stop() { player_->stop(); }
    void Play() { player_->play(); }
    void Seek(qlonglong offset) { player_->seekRelative(offset); }
    void SetPosition(const QDBusObjectPath &trackId, qlonglong position) {
        player_->setPosition(trackId.path(), position);
    }
    void OpenUri(const QString &uri) { player_->openUri(uri); }

  private:
    MprisPlayer *player_;
};

struct MprisPlayer::Impl {
    MpvController &player;
    PlaylistController &playlist;
    QPointer<QWidget> window;
    QDBusConnection bus = QDBusConnection::sessionBus();
    bool registered = false;
    QString lastStatus;
    QString lastLoop;
    bool lastShuffle = false;
    double lastRate = 1.0;
    double lastVolume = 1.0;
    bool lastCanSeek = false;
    bool lastCanGoNext = false;
    bool lastCanGoPrevious = false;
    QVariantMap lastMetadata;

    Impl(MpvController &player, PlaylistController &playlist)
        : player(player), playlist(playlist) {}
};

MprisPlayer::MprisPlayer(MpvController &player, PlaylistController &playlist, QObject *parent)
    : QObject(parent), impl_(std::make_unique<Impl>(player, playlist)) {
    if (!impl_->bus.isConnected())
        return;
    const auto flags = QDBusConnection::ExportAdaptors;
    if (!impl_->bus.registerService(QString::fromLatin1(serviceName)))
        return;
    if (!impl_->bus.registerObject(QString::fromLatin1(objectPath), this, flags)) {
        impl_->bus.unregisterService(QString::fromLatin1(serviceName));
        return;
    }
    new MprisRootAdaptor(this);
    new MprisPlayerAdaptor(this);
    impl_->registered = true;
    impl_->lastStatus = playbackStatus();
    impl_->lastLoop = loopStatus();
    impl_->lastShuffle = shuffle();
    impl_->lastRate = rate();
    impl_->lastVolume = volume();
    impl_->lastCanSeek = canSeek();
    impl_->lastCanGoNext = canGoNext();
    impl_->lastCanGoPrevious = canGoPrevious();
    impl_->lastMetadata = metadata();

    connect(&impl_->player, &MpvController::stateChanged, this, [this] { refresh(); });
    connect(&impl_->player, &MpvController::fileLoaded, this, [this] { refresh(); });
    connect(&impl_->player, &MpvController::mediaInformationChanged, this, [this] { refresh(); });
    connect(&impl_->playlist, &PlaylistController::currentChanged, this, [this] { refresh(); });
    connect(&impl_->playlist, &PlaylistController::navigationChanged, this, [this] { refresh(); });
    connect(&impl_->playlist, &PlaylistController::repeatModeChanged, this, [this] { refresh(); });
    connect(&impl_->playlist, &PlaylistController::shuffleEnabledChanged, this,
            [this] { refresh(); });
    connect(&impl_->player, &MpvController::tuningChanged, this, [this] { refresh(); });
}

MprisPlayer::~MprisPlayer() {
    if (!impl_->registered)
        return;
    impl_->bus.unregisterObject(QString::fromLatin1(objectPath));
    impl_->bus.unregisterService(QString::fromLatin1(serviceName));
}

bool MprisPlayer::isRegistered() const { return impl_->registered; }

void MprisPlayer::setWindow(QWidget *window) { impl_->window = window; }

void MprisPlayer::setWindowVisible(bool visible) {
    if (impl_->window)
        impl_->window->setVisible(visible);
}

QString MprisPlayer::playbackStatus() const {
    const auto &state = impl_->player.state();
    if (!state.active)
        return QStringLiteral("Stopped");
    return state.paused ? QStringLiteral("Paused") : QStringLiteral("Playing");
}

QVariantMap MprisPlayer::metadata() const {
    QVariantMap map;
    const auto &state = impl_->player.state();
    const auto title = displayTitle(impl_->player);
    map.insert(QStringLiteral("mpris:trackid"),
               QVariant::fromValue(QDBusObjectPath(QString::fromLatin1(trackPath))));
    if (!title.isEmpty())
        map.insert(QStringLiteral("xesam:title"), title);
    if (!state.path.isEmpty()) {
        auto url = QUrl::fromUserInput(state.path);
        // MPRIS is visible to every session-bus client; never publish URL
        // credentials, signed queries or fragments that the media report hides.
        if (!url.isLocalFile() && url.isValid() &&
            (!url.userInfo().isEmpty() || url.hasQuery() || !url.fragment().isEmpty())) {
            url.setUserInfo({});
            url.setQuery({});
            url.setFragment({});
        }
        map.insert(QStringLiteral("xesam:url"), url.toString());
    }
    if (std::isfinite(state.duration) && state.duration > 0)
        map.insert(QStringLiteral("mpris:length"),
                   static_cast<qlonglong>(std::llround(state.duration * 1000000.0)));
    return map;
}

qlonglong MprisPlayer::position() const {
    const auto seconds = impl_->player.state().position;
    return std::isfinite(seconds) && seconds > 0
               ? static_cast<qlonglong>(std::llround(seconds * 1000000.0))
               : 0;
}

double MprisPlayer::rate() const { return impl_->player.state().playbackSpeed; }

double MprisPlayer::volume() const {
    return std::clamp(impl_->player.state().volume / 100.0, 0.0, 1.0);
}

double MprisPlayer::minimumRate() const { return 0.25; }
double MprisPlayer::maximumRate() const { return 4.0; }
bool MprisPlayer::canSeek() const { return impl_->player.state().seekable; }
bool MprisPlayer::canGoNext() const { return impl_->playlist.hasNext(); }
bool MprisPlayer::canGoPrevious() const { return impl_->playlist.hasPrevious(); }
bool MprisPlayer::shuffle() const { return impl_->playlist.shuffleEnabled(); }

QString MprisPlayer::loopStatus() const {
    switch (impl_->playlist.repeatMode()) {
    case PlaylistController::RepeatMode::One:
        return QStringLiteral("Track");
    case PlaylistController::RepeatMode::All:
        return QStringLiteral("Playlist");
    case PlaylistController::RepeatMode::Off:
    default:
        return QStringLiteral("None");
    }
}

void MprisPlayer::setRate(double value) {
    if (std::isfinite(value))
        impl_->player.setPlaybackSpeed(value);
}

void MprisPlayer::setVolume(double value) {
    if (std::isfinite(value))
        impl_->player.setVolume(std::clamp(value, 0.0, 1.0) * 100.0);
}

void MprisPlayer::setShuffle(bool enabled) { impl_->playlist.setShuffleEnabled(enabled); }

void MprisPlayer::setLoopStatus(const QString &status) {
    if (status == QLatin1String("Track"))
        impl_->playlist.setRepeatMode(PlaylistController::RepeatMode::One);
    else if (status == QLatin1String("Playlist"))
        impl_->playlist.setRepeatMode(PlaylistController::RepeatMode::All);
    else if (status == QLatin1String("None"))
        impl_->playlist.setRepeatMode(PlaylistController::RepeatMode::Off);
}

void MprisPlayer::raise() {
    emit raiseRequested();
    if (impl_->window) {
        impl_->window->showNormal();
        impl_->window->raise();
        impl_->window->activateWindow();
    }
}

void MprisPlayer::quit() { emit quitRequested(); }

void MprisPlayer::play() {
    if (impl_->player.state().active)
        impl_->player.setPaused(false);
    else
        impl_->playlist.playOrPause();
}

void MprisPlayer::pause() {
    if (impl_->player.state().active)
        impl_->player.setPaused(true);
}

void MprisPlayer::playPause() { impl_->playlist.playOrPause(); }
void MprisPlayer::stop() { impl_->player.stop(); }
void MprisPlayer::next() { impl_->playlist.playNext(); }
void MprisPlayer::previous() { impl_->playlist.playPrevious(); }

void MprisPlayer::seekRelative(qlonglong offsetMicroseconds) {
    impl_->player.seekRelative(static_cast<double>(offsetMicroseconds) / 1000000.0);
    const auto newPosition = std::max<qlonglong>(0, position() + offsetMicroseconds);
    emit seeked(newPosition);
    if (impl_->registered) {
        auto message = QDBusMessage::createSignal(QString::fromLatin1(objectPath),
                                                  QString::fromLatin1(playerInterface),
                                                  QStringLiteral("Seeked"));
        message << newPosition;
        impl_->bus.send(message);
    }
}

void MprisPlayer::setPosition(const QString &trackId, qlonglong positionMicroseconds) {
    if (trackId != QLatin1String(trackPath))
        return;
    impl_->player.seekAbsolute(static_cast<double>(positionMicroseconds) / 1000000.0);
    emit seeked(positionMicroseconds);
    if (impl_->registered) {
        auto message = QDBusMessage::createSignal(QString::fromLatin1(objectPath),
                                                  QString::fromLatin1(playerInterface),
                                                  QStringLiteral("Seeked"));
        message << positionMicroseconds;
        impl_->bus.send(message);
    }
}

void MprisPlayer::openUri(const QString &uri) {
    if (!uri.isEmpty())
        emit openUriRequested(uri);
}

void MprisPlayer::refresh() {
    if (!impl_->registered)
        return;
    QVariantMap changed;
    const auto status = playbackStatus();
    if (status != impl_->lastStatus) {
        impl_->lastStatus = status;
        changed.insert(QStringLiteral("PlaybackStatus"), status);
    }
    const auto loop = loopStatus();
    if (loop != impl_->lastLoop) {
        impl_->lastLoop = loop;
        changed.insert(QStringLiteral("LoopStatus"), loop);
    }
    if (shuffle() != impl_->lastShuffle) {
        impl_->lastShuffle = shuffle();
        changed.insert(QStringLiteral("Shuffle"), impl_->lastShuffle);
    }
    if (!qFuzzyCompare(rate(), impl_->lastRate)) {
        impl_->lastRate = rate();
        changed.insert(QStringLiteral("Rate"), impl_->lastRate);
    }
    if (!qFuzzyCompare(volume(), impl_->lastVolume)) {
        impl_->lastVolume = volume();
        changed.insert(QStringLiteral("Volume"), impl_->lastVolume);
    }
    if (canSeek() != impl_->lastCanSeek) {
        impl_->lastCanSeek = canSeek();
        changed.insert(QStringLiteral("CanSeek"), impl_->lastCanSeek);
    }
    if (canGoNext() != impl_->lastCanGoNext) {
        impl_->lastCanGoNext = canGoNext();
        changed.insert(QStringLiteral("CanGoNext"), impl_->lastCanGoNext);
    }
    if (canGoPrevious() != impl_->lastCanGoPrevious) {
        impl_->lastCanGoPrevious = canGoPrevious();
        changed.insert(QStringLiteral("CanGoPrevious"), impl_->lastCanGoPrevious);
    }
    const auto currentMetadata = metadata();
    if (currentMetadata != impl_->lastMetadata) {
        impl_->lastMetadata = currentMetadata;
        changed.insert(QStringLiteral("Metadata"), currentMetadata);
    }
    if (changed.isEmpty())
        return;
    auto message = QDBusMessage::createSignal(
        QString::fromLatin1(objectPath), QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    message << QString::fromLatin1(playerInterface) << changed << QStringList{};
    impl_->bus.send(message);
}

#include "mprisplayer.moc"
