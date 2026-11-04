#include "playback/mpv/mpvcontroller_p.h"

#include <QFileInfo>

bool MpvController::isRecording() const { return !impl_->recordingPath.isEmpty(); }

QString MpvController::recordingPath() const { return impl_->recordingPath; }

bool MpvController::startRecording(const QString &absolutePath) {
    auto &self = *impl_;
    if (!self.ready || !self.state.active || absolutePath.isEmpty() ||
        !QFileInfo(absolutePath).isAbsolute() || self.recordingPath == absolutePath)
        return false;
    const int status = mpv_set_property_string(self.client.get(), "stream-record",
                                               absolutePath.toUtf8().constData());
    self.report(status);
    if (status < 0)
        return false;
    self.recordingPath = absolutePath;
    emit recordingChanged();
    return true;
}

void MpvController::stopRecording() {
    auto &self = *impl_;
    if (self.recordingPath.isEmpty())
        return;
    self.report(mpv_set_property_string(self.client.get(), "stream-record", ""));
    self.recordingPath.clear();
    emit recordingChanged();
}
