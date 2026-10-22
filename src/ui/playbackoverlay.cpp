#include "playbackoverlay.h"
#include "playback/mpvcontroller.h"
#include "playbackoverlaytext.h"

#include <QFrame>
#include <QLabel>
#include <QProgressBar>
#include <QResizeEvent>
#include <QVBoxLayout>

PlaybackOverlay::PlaybackOverlay(MpvController &controller, QWidget *parent)
    : QWidget(parent), controller_(controller), information_(new QLabel(this)),
      seekPanel_(new QFrame(this)), seekLabel_(new QLabel(seekPanel_)),
      progress_(new QProgressBar(seekPanel_)) {
    setObjectName(QStringLiteral("playbackOverlay"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);
    information_->setObjectName(QStringLiteral("playbackInformationOverlay"));
    information_->setTextFormat(Qt::PlainText);
    information_->setWordWrap(true);
    information_->setFocusPolicy(Qt::NoFocus);
    information_->setAccessibleName(tr("当前播放信息"));
    seekPanel_->setObjectName(QStringLiteral("seekProgressOverlay"));
    seekLabel_->setTextFormat(Qt::PlainText);
    seekLabel_->setAlignment(Qt::AlignCenter);
    progress_->setObjectName(QStringLiteral("seekProgressBar"));
    progress_->setAccessibleName(tr("定位播放进度"));
    progress_->setRange(0, 10000);
    progress_->setTextVisible(false);
    progress_->setFixedHeight(8);
    setStyleSheet(QStringLiteral(
        "QLabel#playbackInformationOverlay { color: #f2f4f8; background: rgba(20,22,28,210);"
        " border-radius: 6px; padding: 12px; font-family: monospace; font-size: 14px; }"
        "QFrame#seekProgressOverlay { background: rgba(20,22,28,225); border-radius: 6px; }"
        "QFrame#seekProgressOverlay QLabel { color: #f2f4f8; background: transparent;"
        " font-size: 16px; }"
        "QProgressBar { background: #454953; border: none; border-radius: 4px; }"
        "QProgressBar::chunk { background: #8ba9ef; border-radius: 4px; }"));
    auto *layout = new QVBoxLayout(seekPanel_);
    layout->setContentsMargins(18, 12, 18, 14);
    layout->setSpacing(10);
    layout->addWidget(seekLabel_);
    layout->addWidget(progress_);
    seekTimer_.setSingleShot(true);
    connect(&seekTimer_, &QTimer::timeout, this, &PlaybackOverlay::clearSeek);
    connect(&controller_, &MpvController::stateChanged, this, &PlaybackOverlay::present);
    connect(&controller_, &MpvController::mediaInformationChanged, this, &PlaybackOverlay::present);
    connect(&controller_, &MpvController::tuningChanged, this, &PlaybackOverlay::present);
    // A new load/stop must not retain the preceding media's transient progress.
    connect(&controller_, &MpvController::fileOpenRequested, this, &PlaybackOverlay::clearSeek);
    connect(&controller_, &MpvController::stopRequested, this, &PlaybackOverlay::clearSeek);
    information_->hide();
    seekPanel_->hide();
    hide();
}

void PlaybackOverlay::setInformationVisible(bool visible) {
    informationVisible_ = visible;
    present();
}

bool PlaybackOverlay::showSeek(const QString &message, int durationMs) {
    if (!formatSeekProgress(controller_.state()).available)
        return false;
    seekMessage_ = message;
    seekVisible_ = true;
    seekTimer_.start(qMax(1, durationMs));
    present();
    raise();
    return true;
}

void PlaybackOverlay::clearSeek() {
    seekTimer_.stop();
    seekVisible_ = false;
    seekMessage_.clear();
    seekPanel_->hide();
    updateVisibility();
}

void PlaybackOverlay::present() {
    if (informationVisible_) {
        information_->setText(
            formatPlaybackOverlayInformation(controller_.mediaInformation(), controller_.state()));
    }
    if (seekVisible_) {
        const auto progress = formatSeekProgress(controller_.state());
        if (!progress.available) {
            clearSeek();
        } else {
            progress_->setValue(progress.value);
            progress_->setAccessibleDescription(progress.text);
            seekLabel_->setText(seekMessage_ + QStringLiteral("  ·  ") + progress.text);
        }
    }
    information_->setVisible(informationVisible_);
    seekPanel_->setVisible(seekVisible_);
    arrangePanels();
    updateVisibility();
}

void PlaybackOverlay::updateVisibility() {
    setVisible(informationVisible_ || seekVisible_);
    if (informationVisible_ || seekVisible_)
        raise();
}

void PlaybackOverlay::arrangePanels() {
    const int panelWidth = qMax(1, qMin(680, width() - 40));
    information_->setFixedWidth(panelWidth);
    const int infoHeight = information_->heightForWidth(panelWidth);
    information_->setGeometry(20, 20, panelWidth,
                              qMin(qMax(1, height() - 40), qMax(1, infoHeight)));
    const int seekHeight = seekPanel_->sizeHint().height();
    seekPanel_->setGeometry((width() - panelWidth) / 2, qMax(0, height() - seekHeight - 32),
                            panelWidth, seekHeight);
}

void PlaybackOverlay::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    arrangePanels();
}
