#include "playlistcontroller.h"

#include "mpvcontroller.h"
#include "playlistmodel.h"

#include <QFileInfo>
#include <QPersistentModelIndex>

PlaylistController::PlaylistController(MpvController &player, QObject *parent)
    : QObject(parent), player_(player), model_(new PlaylistModel(this)) {
    connect(&player_, &MpvController::playbackFinished, this, &PlaylistController::playFinished);
    connect(&player_, &MpvController::mediaLoadFailed, this, &PlaylistController::mediaLoadFailed);
    // FILE_LOADED can precede a no-decodable-frames error. Keep failure history
    // until a successful EOF or explicit navigation starts a new attempt chain.
    connect(model_, &PlaylistModel::rowsInserted, this, &PlaylistController::updateFileLoop);
    connect(model_, &PlaylistModel::rowsRemoved, this, &PlaylistController::updateFileLoop);
    connect(model_, &PlaylistModel::modelReset, this, &PlaylistController::updateFileLoop);
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
    return model_->currentIndex() >= 0 &&
           (repeatMode_ == RepeatMode::All || model_->currentIndex() + 1 < model_->rowCount());
}

bool PlaylistController::hasPrevious() const {
    return model_->currentIndex() >= 0 &&
           (repeatMode_ == RepeatMode::All || model_->currentIndex() > 0);
}

PlaylistController::RepeatMode PlaylistController::repeatMode() const { return repeatMode_; }

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
    player_.setFileLoopEnabled(repeatMode_ == RepeatMode::One ||
                               (repeatMode_ == RepeatMode::All && model_->rowCount() == 1));
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
    playFirstAvailable(row);
}

void PlaylistController::restoreFiles(const QStringList &paths, int currentIndex) {
    clear();
    model_->addFiles(paths);
    if (model_->rowCount() > 0)
        model_->setCurrentIndex(qBound(0, currentIndex, model_->rowCount() - 1));
    emit currentChanged(model_->currentIndex());
}

void PlaylistController::playOrPause() {
    if (player_.state().active)
        player_.togglePause();
    else {
        failedRows_.clear();
        playFirstAvailable(qMax(0, model_->currentIndex()));
    }
}

void PlaylistController::playNext() {
    if (hasNext()) {
        failedRows_.clear();
        playFirstAvailable(model_->currentIndex() + 1, 1, repeatMode_ == RepeatMode::All);
    }
}

void PlaylistController::playPrevious() {
    if (hasPrevious()) {
        failedRows_.clear();
        playFirstAvailable(model_->currentIndex() - 1, -1, repeatMode_ == RepeatMode::All);
    }
}

void PlaylistController::clear() {
    playbackQueued_ = false;
    ++playbackRevision_;
    exhausted_ = false;
    navigationStep_ = 1;
    failedRows_.clear();
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
    if (row == model_->currentIndex()) {
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
    emit currentChanged(model_->currentIndex());
}

void PlaylistController::playFinished() {
    if (!playbackQueued_)
        return;
    failedRows_.clear();
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
    const auto revision = playbackRevision_;
    emit queueError(message);
    if (revision != playbackRevision_)
        return;
    if (playFirstAvailable(model_->currentIndex() + navigationStep_, navigationStep_,
                           repeatMode_ == RepeatMode::All) == ScanResult::Exhausted)
        finishQueue();
}

void PlaylistController::finishQueue() {
    playbackQueued_ = false;
    exhausted_ = true;
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
        if (!QFileInfo(path).isFile()) {
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
        exhausted_ = false;
        playbackQueued_ = true;
        navigationStep_ = step;
        model_->setCurrentIndex(row);
        player_.openFile(path);
        emit currentChanged(row);
        return ScanResult::Started;
    }
    return ScanResult::Exhausted;
}
