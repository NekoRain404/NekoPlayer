#include "mediainformationdialog.h"
#include "mediainformationtext.h"
#include "playback/mpvcontroller.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

MediaInformationDialog::MediaInformationDialog(MpvController &player, QWidget *parent)
    : QDialog(parent), player_(player), report_(new QPlainTextEdit(this)) {
    setObjectName(QStringLiteral("mediaInformationDialog"));
    setWindowTitle(tr("媒体信息"));
    auto *layout = new QVBoxLayout(this);
    report_->setObjectName(QStringLiteral("mediaInformationText"));
    report_->setAccessibleName(tr("当前媒体信息"));
    report_->setReadOnly(true);
    report_->setUndoRedoEnabled(false);
    report_->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(report_);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    copy_ = buttons->addButton(tr("复制"), QDialogButtonBox::ActionRole);
    copy_->setObjectName(QStringLiteral("copyMediaInformationButton"));
    connect(copy_, &QPushButton::clicked, this,
            [this] { QApplication::clipboard()->setText(report_->toPlainText()); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    layout->addWidget(buttons);
    connect(&player_, &MpvController::mediaInformationChanged, this,
            &MediaInformationDialog::updateReport);
    connect(&player_, &MpvController::stateChanged, this, &MediaInformationDialog::updateState);
    updateReport();
    resize(640, 480);
}

void MediaInformationDialog::changeEvent(QEvent *event) {
    QDialog::changeEvent(event);
    if (event->type() != QEvent::LanguageChange)
        return;
    setWindowTitle(tr("媒体信息"));
    report_->setAccessibleName(tr("当前媒体信息"));
    copy_->setText(tr("复制"));
    updateReport();
}

void MediaInformationDialog::updateState() {
    const auto &state = player_.state();
    if (lastActive_ != state.active || lastPath_ != state.path ||
        lastDecoder_ != state.hardwareDecoder)
        updateReport();
}

void MediaInformationDialog::updateReport() {
    const auto &state = player_.state();
    lastActive_ = state.active;
    lastPath_ = state.path;
    lastDecoder_ = state.hardwareDecoder;
    const auto text = formatMediaInformation(player_.mediaInformation(), state);
    if (report_->toPlainText() != text)
        report_->setPlainText(text);
}
