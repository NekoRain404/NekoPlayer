#include "networkopendialog.h"
#include "playback/mediasource.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

NetworkOpenDialog::NetworkOpenDialog(QWidget *parent) : QDialog(parent) {
    setObjectName(QStringLiteral("networkOpenDialog"));
    setWindowTitle(tr("打开网络地址"));
    resize(620, 180);
    auto *layout = new QVBoxLayout(this);
    auto *label = new QLabel(tr("媒体地址："), this);
    address_ = new QLineEdit(this);
    address_->setObjectName(QStringLiteral("networkAddress"));
    address_->setPlaceholderText(QStringLiteral("https://… / rtsp://… / rtmp://…"));
    address_->setClearButtonEnabled(true);
    label->setBuddy(address_);
    auto *hint = new QLabel(tr("支持 HTTP(S)、RTSP(S)、RTMP(S) 媒体地址与直播流。"), this);
    hint->setWordWrap(true);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel, this);
    auto *open = buttons->button(QDialogButtonBox::Open);
    open->setText(tr("播放"));
    open->setEnabled(false);
    connect(address_, &QLineEdit::textChanged, this,
            [this, open] { open->setEnabled(!mediaUrl().isEmpty()); });
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (!mediaUrl().isEmpty())
            accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(label);
    layout->addWidget(address_);
    layout->addWidget(hint);
    layout->addWidget(buttons);
    address_->setFocus();
}

QString NetworkOpenDialog::mediaUrl() const { return MediaSource::networkUrl(address_->text()); }
