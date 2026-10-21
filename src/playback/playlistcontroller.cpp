#include "playlistcontroller.h"

#include "mpvcontroller.h"
#include "playlistmodel.h"

#include <QFileInfo>

PlaylistController::PlaylistController(MpvController &player, QObject *parent)
    : QObject(parent), player_(player), model_(new PlaylistModel(this)) {
    connect(&player_, &MpvController::playbackFinished, this, &PlaylistController::playFinished);
    connect(&player_, &MpvController::mediaLoadFailed, this, &PlaylistController::mediaLoadFailed);
}

PlaylistModel *PlaylistController::model() { return model_; }
const PlaylistModel *PlaylistController::model() const { return model_; }

bool PlaylistController::hasNext() const {
    return model_->currentIndex() >= 0 && model_->currentIndex() + 1 < model_->rowCount();
}

bool PlaylistController::hasPrevious() const { return model_->currentIndex() > 0; }

void PlaylistController::addFiles(const QStringList &paths) {
    const auto firstNewRow = model_->rowCount();
    if (model_->addFiles(paths) == 0)
        return;
    if ((model_->currentIndex() < 0 || exhausted_) && !playFirstAvailable(firstNewRow))
        finishQueue();
}

void PlaylistController::playAt(int row) { playFirstAvailable(row); }

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
    else
        playFirstAvailable(qMax(0, model_->currentIndex()));
}

void PlaylistController::playNext() {
    if (hasNext())
        playFirstAvailable(model_->currentIndex() + 1);
}

void PlaylistController::playPrevious() {
    if (hasPrevious())
        playFirstAvailable(model_->currentIndex() - 1, -1);
}

void PlaylistController::clear() {
    exhausted_ = false;
    navigationStep_ = 1;
    player_.stop();
    model_->clear();
    emit currentChanged(-1);
}

void PlaylistController::playFinished() {
    if (!playFirstAvailable(model_->currentIndex() + 1))
        finishQueue();
}

void PlaylistController::mediaLoadFailed(const QString &message) {
    emit queueError(message);
    if (!playFirstAvailable(model_->currentIndex() + navigationStep_, navigationStep_))
        finishQueue();
}

void PlaylistController::finishQueue() {
    exhausted_ = true;
    emit queueFinished();
}

bool PlaylistController::playFirstAvailable(int firstRow, int step) {
    for (auto row = firstRow; row >= 0 && row < model_->rowCount(); row += step) {
        const auto path = model_->pathAt(row);
        if (!QFileInfo(path).isFile()) {
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
