#include "playlistcontroller.h"

#include "mpvcontroller.h"
#include "playlistmodel.h"

#include <QFileInfo>

PlaylistController::PlaylistController(MpvController &player, QObject *parent)
    : QObject(parent), player_(player), model_(new PlaylistModel(this)) {
    connect(&player_, &MpvController::playbackFinished, this, &PlaylistController::playFinished);
    connect(&player_, &MpvController::mediaLoadFailed, this, &PlaylistController::mediaLoadFailed);
    // FILE_LOADED can precede a no-decodable-frames error. Keep failure history
    // until a successful EOF or explicit navigation starts a new attempt chain.
    connect(model_, &PlaylistModel::rowsInserted, this, &PlaylistController::updateFileLoop);
    connect(model_, &PlaylistModel::rowsRemoved, this, &PlaylistController::updateFileLoop);
    connect(model_, &PlaylistModel::modelReset, this, &PlaylistController::updateFileLoop);
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
        if (!playFirstAvailable(firstNewRow))
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
    exhausted_ = false;
    navigationStep_ = 1;
    failedRows_.clear();
    player_.stop();
    model_->clear();
    emit currentChanged(-1);
}

void PlaylistController::playFinished() {
    failedRows_.clear();
    // Native looping normally suppresses EOF. Handle one already queued while
    // the asynchronous loop setting was changing by reopening the same row.
    const auto nextRow = model_->currentIndex() + (repeatMode_ == RepeatMode::One ? 0 : 1);
    if (!playFirstAvailable(nextRow, 1, repeatMode_ == RepeatMode::All))
        finishQueue();
}

void PlaylistController::mediaLoadFailed(const QString &message) {
    failedRows_.insert(model_->currentIndex());
    emit queueError(message);
    if (!playFirstAvailable(model_->currentIndex() + navigationStep_, navigationStep_,
                            repeatMode_ == RepeatMode::All))
        finishQueue();
}

void PlaylistController::finishQueue() {
    exhausted_ = true;
    emit queueFinished();
}

bool PlaylistController::playFirstAvailable(int firstRow, int step, bool wrap) {
    const auto count = model_->rowCount();
    auto row = firstRow;
    for (int visited = 0; visited < count; ++visited, row += step) {
        if (wrap)
            row = (row % count + count) % count;
        else if (row < 0 || row >= count)
            break;
        if (failedRows_.contains(row))
            continue;
        const auto path = model_->pathAt(row);
        if (!QFileInfo(path).isFile()) {
            failedRows_.insert(row);
            emit queueError(tr("文件不存在：%1").arg(path));
            continue;
        }
        exhausted_ = false;
        navigationStep_ = step;
        model_->setCurrentIndex(row);
        player_.openFile(path);
        emit currentChanged(row);
        return true;
    }
    return false;
}
