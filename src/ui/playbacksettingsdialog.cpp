#include "playbacksettingsdialog.h"
#include "playback/mpvcontroller.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

PlaybackSettingsDialog::PlaybackSettingsDialog(MpvController &player, QWidget *parent)
    : QDialog(parent), player_(player), hardwareDecoding_(new QComboBox(this)),
      actualDecoder_(new QLabel(this)) {
    setObjectName(QStringLiteral("playbackSettingsDialog"));
    setWindowTitle(tr("播放设置"));
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    hardwareDecoding_->setObjectName(QStringLiteral("hardwareDecodingChoice"));
    hardwareDecoding_->setAccessibleName(tr("视频解码方式"));
    hardwareDecoding_->addItem(tr("自动（安全）"), true);
    hardwareDecoding_->addItem(tr("软件解码"), false);
    hardwareDecoding_->setCurrentIndex(player_.hardwareDecodingEnabled() ? 0 : 1);
    form->addRow(tr("视频解码方式(&D)："), hardwareDecoding_);
    actualDecoder_->setObjectName(QStringLiteral("actualDecoder"));
    actualDecoder_->setTextFormat(Qt::PlainText);
    actualDecoder_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(tr("实际解码器："), actualDecoder_);
    layout->addLayout(form);
    auto *help = new QLabel(tr("自动模式优先尝试安全的硬件解码方式，不可用时回退到软件解码。"
                               "实际使用方式取决于视频格式与显卡驱动。应用切换时画面可能短暂中断；"
                               "偏好将在正常退出时保存。"),
                            this);
    help->setWordWrap(true);
    layout->addWidget(help);
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    buttons->setObjectName(QStringLiteral("playbackSettingsButtons"));
    applyButton_ = buttons->button(QDialogButtonBox::Apply);
    layout->addWidget(buttons);
    connect(applyButton_, &QPushButton::clicked, this, &PlaybackSettingsDialog::apply);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        apply();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(hardwareDecoding_, &QComboBox::currentIndexChanged, this,
            &PlaybackSettingsDialog::updateApplyButton);
    connect(&player_, &MpvController::hardwareDecodingPreferenceChanged, this,
            &PlaybackSettingsDialog::updateApplyButton);
    connect(&player_, &MpvController::stateChanged, this,
            &PlaybackSettingsDialog::updateActualDecoder);
    updateApplyButton();
    updateActualDecoder();
    resize(480, sizeHint().height());
}

void PlaybackSettingsDialog::apply() {
    player_.setHardwareDecodingEnabled(hardwareDecoding_->currentData().toBool());
    updateApplyButton();
}

void PlaybackSettingsDialog::updateApplyButton() {
    applyButton_->setEnabled(player_.isReady() && hardwareDecoding_->currentData().toBool() !=
                                                      player_.hardwareDecodingEnabled());
}

void PlaybackSettingsDialog::updateActualDecoder() {
    const auto &state = player_.state();
    if (lastActive_ == state.active && lastDecoder_ == state.hardwareDecoder &&
        !actualDecoder_->text().isEmpty())
        return;
    lastActive_ = state.active;
    lastDecoder_ = state.hardwareDecoder;
    if (!state.active)
        actualDecoder_->setText(tr("未播放"));
    else if (state.hardwareDecoder.isEmpty())
        actualDecoder_->setText(tr("未检测到视频解码器"));
    else if (state.hardwareDecoder == QLatin1String("no"))
        actualDecoder_->setText(tr("软件解码"));
    else
        actualDecoder_->setText(state.hardwareDecoder);
}
