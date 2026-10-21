#include "timeseekdialog.h"
#include "playback/playbacktime.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

TimeSeekDialog::TimeSeekDialog(double position, double duration, QWidget *parent)
    : QDialog(parent), input_(new QLineEdit(formatPlaybackTime(position, true), this)) {
    setObjectName(QStringLiteral("timeSeekDialog"));
    setWindowTitle(tr("跳转到指定时间"));
    auto *layout = new QVBoxLayout(this);
    auto *label = new QLabel(tr("秒数、分:秒 或 时:分:秒（可含三位毫秒）"), this);
    label->setBuddy(input_);
    input_->setObjectName(QStringLiteral("timeSeekInput"));
    input_->setAccessibleName(tr("目标播放时间"));
    layout->addWidget(label);
    layout->addWidget(input_);
    layout->addWidget(new QLabel(tr("媒体时长：%1").arg(formatPlaybackTime(duration, true)), this));
    auto *error = new QLabel(this);
    error->setObjectName(QStringLiteral("timeSeekError"));
    layout->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setObjectName(QStringLiteral("timeSeekButtons"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    const auto validate = [this, error, buttons, duration] {
        const auto seconds = selectedSeconds();
        const bool valid = seconds && *seconds <= duration;
        buttons->button(QDialogButtonBox::Ok)->setEnabled(valid);
        error->setText(valid ? QString{} : tr("请输入不超过媒体时长的有效时间。"));
    };
    connect(input_, &QLineEdit::textChanged, this, validate);
    validate();
    input_->selectAll();
    input_->setFocus();
}

std::optional<double> TimeSeekDialog::selectedSeconds() const {
    return parsePlaybackTime(input_->text());
}
