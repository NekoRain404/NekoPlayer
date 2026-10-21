#include "subtitlesettingsdialog.h"
#include "playback/mpvcontroller.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

SubtitleSettingsDialog::SubtitleSettingsDialog(MpvController &player, QWidget *parent)
    : QDialog(parent), player_(player), visible_(new QCheckBox(tr("显示字幕"), this)),
      scale_(new QDoubleSpinBox(this)), position_(new QDoubleSpinBox(this)),
      font_(new QFontComboBox(this)) {
    setObjectName(QStringLiteral("subtitleSettingsDialog"));
    setWindowTitle(tr("字幕显示设置"));
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    visible_->setObjectName(QStringLiteral("subtitleVisibleChoice"));
    scale_->setObjectName(QStringLiteral("subtitleScaleChoice"));
    scale_->setRange(0.25, 4.0);
    scale_->setSingleStep(0.1);
    scale_->setDecimals(2);
    scale_->setSuffix(tr(" 倍"));
    position_->setObjectName(QStringLiteral("subtitlePositionChoice"));
    position_->setRange(0, 100);
    position_->setSingleStep(2);
    position_->setDecimals(1);
    position_->setSuffix(QStringLiteral(" %"));
    font_->setObjectName(QStringLiteral("subtitleFontChoice"));
    font_->setEditable(true);
    font_->lineEdit()->setMaxLength(256);
    form->addRow(visible_);
    form->addRow(tr("缩放(&S)："), scale_);
    form->addRow(tr("垂直位置(&P)："), position_);
    form->addRow(tr("默认字体(&F)："), font_);
    layout->addLayout(form);
    auto *help = new QLabel(tr("隐藏字幕会保留轨道选择。位置越小越靠上，100% 为原始位置。"
                               "默认字体用于未指定字体的文本字幕；ASS 保留自带样式，"
                               "缩放或移动可能影响其排版。偏好在正常退出时保存。"),
                            this);
    help->setWordWrap(true);
    layout->addWidget(help);
    auto *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                                 QDialogButtonBox::Apply | QDialogButtonBox::RestoreDefaults,
                             this);
    buttons->setObjectName(QStringLiteral("subtitleSettingsButtons"));
    applyButton_ = buttons->button(QDialogButtonBox::Apply);
    layout->addWidget(buttons);
    display(player_.subtitlePreferences());
    connect(visible_, &QCheckBox::toggled, this, &SubtitleSettingsDialog::updateApplyButton);
    connect(scale_, &QDoubleSpinBox::valueChanged, this,
            &SubtitleSettingsDialog::updateApplyButton);
    connect(position_, &QDoubleSpinBox::valueChanged, this,
            &SubtitleSettingsDialog::updateApplyButton);
    connect(font_, &QFontComboBox::currentTextChanged, this,
            &SubtitleSettingsDialog::updateApplyButton);
    connect(&player_, &MpvController::subtitlePreferencesChanged, this,
            &SubtitleSettingsDialog::updateApplyButton);
    connect(applyButton_, &QPushButton::clicked, this, &SubtitleSettingsDialog::apply);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this,
            [this] { display({}); });
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        apply();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    updateApplyButton();
    resize(520, sizeHint().height());
}

SubtitlePreferences SubtitleSettingsDialog::draft() const {
    return SubtitlePreferences{visible_->isChecked(), scale_->value(), position_->value(),
                               font_->currentText()}
        .normalized();
}

void SubtitleSettingsDialog::display(const SubtitlePreferences &preferences) {
    visible_->setChecked(preferences.visible);
    scale_->setValue(preferences.scale);
    position_->setValue(preferences.position);
    // Keep generic and unavailable family names exactly; QFont may substitute them.
    font_->setEditText(preferences.font);
}

void SubtitleSettingsDialog::apply() {
    scale_->interpretText();
    position_->interpretText();
    player_.setSubtitlePreferences(draft());
    display(player_.subtitlePreferences());
    updateApplyButton();
}

void SubtitleSettingsDialog::updateApplyButton() {
    applyButton_->setEnabled(player_.isReady() && draft() != player_.subtitlePreferences());
}
