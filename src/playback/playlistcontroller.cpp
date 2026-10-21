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
    if (model_->currentIndex() < 0)
        playFirstAvailable(firstNewRow);
}

void PlaylistController::playAt(int row) { playFirstAvailable(row); }

void PlaylistController::playNext() {
    if (hasNext())
        playFirstAvailable(model_->currentIndex() + 1);
}

void PlaylistController::playPrevious() {
    if (hasPrevious())
        playFirstAvailable(model_->currentIndex() - 1);
}

void PlaylistController::clear() {
    player_.stop();
    model_->clear();
    emit currentChanged(-1);
}

void PlaylistController::playFinished() {
    if (hasNext())
        playNext();
    else
        emit queueFinished();
}

void PlaylistController::mediaLoadFailed(const QString &message) {
    if (!hasNext())
        return;
    emit queueError(message);
    playNext();
}

bool PlaylistController::playFirstAvailable(int firstRow) {
    for (auto row = firstRow; row < model_->rowCount(); ++row) {
        const auto path = model_->pathAt(row);
        if (!QFileInfo(path).isFile()) {
            emit queueError(tr("文件不存在：%1").arg(path));
            continue;
        }
        model_->setCurrentIndex(row);
        player_.openFile(path);
        emit currentChanged(row);
        return true;
    }
    return false;
}
