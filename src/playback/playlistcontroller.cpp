#include "playlistcontroller.h"

#include "mpvcontroller.h"
#include "playlistmodel.h"

#include <QFileInfo>
#include <QPersistentModelIndex>
#include <QUrl>
#include <cmath>

namespace {
bool isPlayablePath(const QString &path) {
    if (QFileInfo(path).isFile())
        return true;

    const QUrl url(path, QUrl::StrictMode);
    if (!url.isValid() || url.scheme().isEmpty())
        return false;
    if (url.scheme().compare(QStringLiteral("file"), Qt::CaseInsensitive) == 0)
        return QFileInfo(url.toLocalFile()).isFile();
    return true;
}
} // namespace

PlaylistController::PlaylistController(MpvController &player, QObject *parent)
    : QObject(parent), player_(player), model_(new PlaylistModel(this)) {
    connect(&player_, &MpvController::playbackFinished, this, &PlaylistController::playFinished);
    connect(&player_, &MpvController::mediaLoadFailed, this, &PlaylistController::mediaLoadFailed);
    connect(&player_, &MpvController::stopRequested, this,
            &PlaylistController::snapshotCurrentPlayback, Qt::DirectConnection);
    connect(&player_, &MpvController::stopRequested, this, [this] { setStopAfterCurrent(false); });
    const auto cancelResume = [this] { resumeEntry_ = QPersistentModelIndex{}; };
    connect(&player_, &MpvController::stopRequested, this, cancelResume);
    connect(&player_, &MpvController::fileOpenRequested, this, cancelResume);
    // FILE_LOADED can precede a no-decodable-frames error. Keep failure history
    // until a successful EOF or explicit navigation starts a new attempt chain.
    connect(model_, &PlaylistModel::rowsInserted, this, &PlaylistController::updateFileLoop);
    connect(model_, &PlaylistModel::rowsRemoved, this, &PlaylistController::updateFileLoop);
    connect(model_, &PlaylistModel::modelReset, this, &PlaylistController::updateFileLoop);
    connect(model_, &PlaylistModel::rowsInserted, this,
            [this](const QModelIndex &, int first, int last) {
                if (shuffleEnabled_)
                    shuffle_.append(*model_, first, last);
            });
    connect(model_, &PlaylistModel::rowsRemoved, this, [this] { shuffle_.prune(); });
    connect(model_, &PlaylistModel::modelReset, this, [this] { shuffle_ = {}; });
    connect(model_, &PlaylistModel::rowsMoved, this,
            [this](const QModelIndex &, int from, int, const QModelIndex &, int destination) {
                const auto to = destination > from ? destination - 1 : destination;
                QSet<int> remapped;
                for (auto row : std::as_const(failedRows_)) {
                    if (row == from)
                        row = to;
                    else if (from < row && row <= to)
                        --row;
                    else if (to <= row && row < from)
                        ++row;
                    remapped.insert(row);
                }
                failedRows_ = std::move(remapped);
                emit currentChanged(model_->currentIndex());
            });
}

PlaylistModel *PlaylistController::model() { return model_; }
const PlaylistModel *PlaylistController::model() const { return model_; }

bool PlaylistController::hasNext() const {
    if (shuffleEnabled_)
        return model_->currentIndex() >= 0 &&
               (shuffle_.hasNext() || repeatMode_ == RepeatMode::All);
    return model_->currentIndex() >= 0 &&
           (repeatMode_ == RepeatMode::All || model_->currentIndex() + 1 < model_->rowCount());
}

bool PlaylistController::hasPrevious() const {
    if (shuffleEnabled_)
        return model_->currentIndex() >= 0 && shuffle_.hasPrevious();
    return model_->currentIndex() >= 0 &&
           (repeatMode_ == RepeatMode::All || model_->currentIndex() > 0);
}

PlaylistController::RepeatMode PlaylistController::repeatMode() const { return repeatMode_; }
bool PlaylistController::shuffleEnabled() const { return shuffleEnabled_; }
bool PlaylistController::stopAfterCurrent() const { return stopAfterCurrent_; }
double PlaylistController::pendingResumePosition() const {
    return resumeEntry_.isValid() && resumeEntry_.row() == model_->currentIndex() ? resumePosition_
                                                                                  : 0;
}

void PlaylistController::setResumePositionResolver(
    std::function<double(const QString &)> resolver) {
    resumePositionResolver_ = std::move(resolver);
}

void PlaylistController::snapshotCurrentPlayback() {
    const auto path = model_->currentPath();
    const auto &state = player_.state();
    if (!path.isEmpty() && state.active && state.seekable && state.path == path)
        emit playbackPositionSnapshot(path, player_.resumePosition(), state.duration);
}

void PlaylistController::setShuffleEnabled(bool enabled) {
    if (shuffleEnabled_ == enabled)
        return;
    shuffleEnabled_ = enabled;
    if (enabled)
        shuffle_.reset(*model_, model_->currentIndex());
    else
        shuffle_ = {};
    emit shuffleEnabledChanged(enabled);
}

void PlaylistController::setStopAfterCurrent(bool enabled) {
    if (stopAfterCurrent_ == enabled)
        return;
    stopAfterCurrent_ = enabled;
    updateFileLoop();
    emit stopAfterCurrentChanged(enabled);
}

void PlaylistController::setRepeatMode(RepeatMode mode) {
    if (mode != RepeatMode::Off && mode != RepeatMode::One && mode != RepeatMode::All)
        return;
    if (mode == repeatMode_)
        return;
    repeatMode_ = mode;
    updateFileLoop();
    emit repeatModeChanged(mode);
}

void PlaylistController::cycleRepeatMode() {
    switch (repeatMode_) {
    case RepeatMode::Off:
        setRepeatMode(RepeatMode::One);
        break;
    case RepeatMode::One:
        setRepeatMode(RepeatMode::All);
        break;
    case RepeatMode::All:
        setRepeatMode(RepeatMode::Off);
        break;
    }
}

void PlaylistController::updateFileLoop() {
    player_.setFileLoopEnabled(!stopAfterCurrent_ &&
                               (repeatMode_ == RepeatMode::One ||
                                (repeatMode_ == RepeatMode::All && model_->rowCount() == 1)));
}

void PlaylistController::addFiles(const QStringList &paths) {
    const auto firstNewRow = model_->rowCount();
    if (model_->addFiles(paths) == 0)
        return;
    if (model_->currentIndex() < 0 || exhausted_) {
        failedRows_.clear();
        if (playFirstAvailable(firstNewRow) == ScanResult::Exhausted)
            finishQueue();
    }
}

void PlaylistController::playAt(int row) {
    if (row < 0 || row >= model_->rowCount())
        return;
    failedRows_.clear();
    if (shuffleEnabled_)
        shuffle_.reset(*model_, row);
    playFirstAvailable(row);
}

void PlaylistController::restoreFiles(const QStringList &paths, int currentIndex,
                                      double resumePosition) {
    clear();
    model_->addFiles(paths);
    if (model_->rowCount() > 0)
        model_->setCurrentIndex(qBound(0, currentIndex, model_->rowCount() - 1));
    if (shuffleEnabled_)
        shuffle_.reset(*model_, model_->currentIndex());
    if (std::isfinite(resumePosition) && resumePosition > 0 && model_->currentIndex() >= 0) {
        resumeEntry_ = model_->index(model_->currentIndex());
        resumePosition_ = resumePosition;
    }
    emit currentChanged(model_->currentIndex());
}

void PlaylistController::playOrPause() {
    if (player_.state().active)
        player_.togglePause();
    else {
        failedRows_.clear();
        if (shuffleEnabled_)
            shuffle_.reset(*model_, model_->currentIndex());
        playFirstAvailable(qMax(0, model_->currentIndex()));
    }
}

void PlaylistController::playNext() {
    if (hasNext()) {
        failedRows_.clear();
        const auto result = shuffleEnabled_ ? playShuffled(1)
                                            : playFirstAvailable(model_->currentIndex() + 1, 1,
                                                                 repeatMode_ == RepeatMode::All);
        if (result == ScanResult::Exhausted)
            emit navigationChanged();
    }
}

void PlaylistController::playPrevious() {
    if (hasPrevious()) {
        failedRows_.clear();
        const auto result = shuffleEnabled_ ? playShuffled(-1)
                                            : playFirstAvailable(model_->currentIndex() - 1, -1,
                                                                 repeatMode_ == RepeatMode::All);
        if (result == ScanResult::Exhausted)
            emit navigationChanged();
    }
}

void PlaylistController::clear() {
    setStopAfterCurrent(false);
    playbackQueued_ = false;
    ++playbackRevision_;
    exhausted_ = false;
    navigationStep_ = 1;
    failedRows_.clear();
    if (player_.state().active || !player_.state().path.isEmpty())
        player_.stop();
    model_->clear();
    emit currentChanged(-1);
}

void PlaylistController::moveItem(int from, int to) {
    if (to < 0 || to >= model_->rowCount())
        return;
    model_->moveRow({}, from, {}, from < to ? to + 1 : to);
}

void PlaylistController::removeAt(int row) {
    if (row < 0 || row >= model_->rowCount())
        return;
    const bool removingCurrent = row == model_->currentIndex();
    if (removingCurrent) {
        // Removing the playing row is an explicit stop, so stopRequested
        // disarms stop-after-current just like player.stop() or clear().
        playbackQueued_ = false;
        ++playbackRevision_;
        exhausted_ = false;
        navigationStep_ = 1;
        player_.stop();
    }
    QSet<int> remapped;
    for (const auto failed : std::as_const(failedRows_)) {
        if (failed != row)
            remapped.insert(failed > row ? failed - 1 : failed);
    }
    failedRows_ = std::move(remapped);
    model_->removeAt(row);
    if (removingCurrent && shuffleEnabled_)
        shuffle_.reset(*model_, model_->currentIndex());
    emit currentChanged(model_->currentIndex());
}

void PlaylistController::playFinished() {
    if (!playbackQueued_)
        return;
    emit mediaPlaybackFinished(model_->currentPath());
    failedRows_.clear();
    if (stopAfterCurrent_) {
        // Disarming may re-enable native loop-file for repeat modes, but libmpv
        // already committed this EOF before delivering END_FILE, so the queue
        // finishes without reopening the file.
        setStopAfterCurrent(false);
        finishQueue();
        return;
    }
    if (shuffleEnabled_ && repeatMode_ != RepeatMode::One) {
        if (playShuffled(1) == ScanResult::Exhausted)
            finishQueue();
        return;
    }
    // Native looping normally suppresses EOF. Handle one already queued while
    // the asynchronous loop setting was changing by reopening the same row.
    const auto nextRow = model_->currentIndex() + (repeatMode_ == RepeatMode::One ? 0 : 1);
    if (playFirstAvailable(nextRow, 1, repeatMode_ == RepeatMode::All) == ScanResult::Exhausted)
        finishQueue();
}

void PlaylistController::mediaLoadFailed(const QString &message) {
    if (!playbackQueued_)
        return;
    failedRows_.insert(model_->currentIndex());
    if (shuffleEnabled_)
        shuffle_.rejectCurrent(navigationStep_);
    const auto revision = playbackRevision_;
    emit queueError(message);
    if (revision != playbackRevision_)
        return;
    if (shuffleEnabled_) {
        if (playShuffled(navigationStep_) == ScanResult::Exhausted)
            finishQueue();
        return;
    }
    if (playFirstAvailable(model_->currentIndex() + navigationStep_, navigationStep_,
                           repeatMode_ == RepeatMode::All) == ScanResult::Exhausted)
        finishQueue();
}

void PlaylistController::finishQueue() {
    playbackQueued_ = false;
    exhausted_ = true;
    emit navigationChanged();
    emit queueFinished();
}

PlaylistController::ScanResult PlaylistController::playFirstAvailable(int firstRow, int step,
                                                                      bool wrap) {
    const auto revision = ++playbackRevision_;
    const auto count = model_->rowCount();
    QList<QPersistentModelIndex> remaining;
    bool snapshot = false;
    auto row = firstRow;
    for (int visited = 0; visited < count; ++visited, row += step) {
        if (snapshot) {
            if (remaining.isEmpty())
                break;
            const auto entry = remaining.takeFirst();
            if (!entry.isValid())
                continue;
            row = entry.row();
        } else if (wrap)
            row = (row % count + count) % count;
        else if (row < 0 || row >= count)
            break;
        if (failedRows_.contains(row))
            continue;
        const auto path = model_->pathAt(row);
        if (!isPlayablePath(path)) {
            if (resumeEntry_.isValid() && resumeEntry_.row() == row)
                resumeEntry_ = QPersistentModelIndex{};
            // Only the error path needs identity snapshots. Callbacks may edit
            // arbitrary rows; visit each surviving original candidate once.
            if (!snapshot) {
                for (int offset = 1; visited + offset < count; ++offset) {
                    auto next = row + offset * step;
                    if (wrap)
                        next = (next % count + count) % count;
                    else if (next < 0 || next >= count)
                        break;
                    remaining.append(QPersistentModelIndex(model_->index(next)));
                }
                snapshot = true;
            }
            failedRows_.insert(row);
            emit queueError(tr("文件不存在：%1").arg(path));
            if (revision != playbackRevision_)
                return ScanResult::Cancelled;
            continue;
        }
        startRow(row, step);
        return ScanResult::Started;
    }
    return ScanResult::Exhausted;
}

void PlaylistController::startRow(int row, int step) {
    snapshotCurrentPlayback();
    const auto startPosition =
        resumeEntry_.isValid() && resumeEntry_.row() == row
            ? resumePosition_
            : (resumePositionResolver_ ? resumePositionResolver_(model_->pathAt(row)) : 0);
    resumeEntry_ = QPersistentModelIndex{};
    exhausted_ = false;
    playbackQueued_ = true;
    navigationStep_ = step;
    if (shuffleEnabled_)
        shuffle_.select(*model_, row);
    model_->setCurrentIndex(row);
    player_.openFileAtPosition(model_->pathAt(row), startPosition);
    emit currentChanged(row);
}

PlaylistController::ScanResult PlaylistController::playShuffled(int step) {
    const auto revision = ++playbackRevision_;
    // Include bounded history as well as a fresh cycle: failed historical items
    // must not prevent reaching a surviving earlier item or permit endless wrap.
    const auto budget = shuffle_.scanBudget() + model_->rowCount();
    for (int visited = 0; visited < budget; ++visited) {
        const auto entry =
            step < 0 ? shuffle_.previous() : shuffle_.next(*model_, repeatMode_ == RepeatMode::All);
        if (!entry.isValid())
            break;
        if (failedRows_.contains(entry.row())) {
            shuffle_.rejectCurrent(step);
            continue;
        }
        const auto path = model_->pathAt(entry.row());
        if (!isPlayablePath(path)) {
            if (resumeEntry_ == entry)
                resumeEntry_ = QPersistentModelIndex{};
            failedRows_.insert(entry.row());
            shuffle_.rejectCurrent(step);
            emit queueError(tr("文件不存在：%1").arg(path));
            if (revision != playbackRevision_)
                return ScanResult::Cancelled;
            continue;
        }
        startRow(entry.row(), step);
        return ScanResult::Started;
    }
    // A failed load selected an entry that has since been removed from history.
    // After exhausting backward candidates, all survivors are forward history.
    // A missing-file scan with the old item still playing retains its cursor.
    if (step < 0 && failedRows_.contains(model_->currentIndex()))
        shuffle_.rewindHistory();
    return ScanResult::Exhausted;
}
