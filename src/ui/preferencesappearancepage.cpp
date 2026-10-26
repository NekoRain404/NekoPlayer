#include "preferencesappearancepage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QStyle>

namespace {
QWidget *sliderRow(QSlider *slider, QLabel *value) {
    auto *row = new QWidget(slider->parentWidget());
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(slider, 1);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(value);
    return row;
}
} // namespace

PreferencesAppearancePage::PreferencesAppearancePage(
    const AppearancePreferences::Settings &baseline, QWidget *parent)
    : QWidget(parent), theme_(new QComboBox(this)), accent_(new QComboBox(this)),
      glass_(new QCheckBox(tr("启用玻璃质感"), this)), opacity_(new QSlider(Qt::Horizontal, this)),
      blur_(new QSlider(Qt::Horizontal, this)), opacityValue_(new QLabel(this)),
      blurValue_(new QLabel(this)), baseline_(baseline.normalized()) {
    theme_->setObjectName(QStringLiteral("appearanceThemeChoice"));
    theme_->setAccessibleName(tr("基础主题"));
    accent_->setObjectName(QStringLiteral("appearanceAccentChoice"));
    accent_->setAccessibleName(tr("强调色"));
    glass_->setObjectName(QStringLiteral("appearanceGlassChoice"));
    glass_->setAccessibleName(tr("启用玻璃质感"));
    opacity_->setObjectName(QStringLiteral("appearanceGlassOpacityChoice"));
    opacity_->setAccessibleName(tr("玻璃不透明度"));
    opacity_->setRange(glassOpacityMinimum, glassOpacityMaximum);
    opacityValue_->setObjectName(QStringLiteral("appearanceGlassOpacityValue"));
    opacityValue_->setMinimumWidth(38);
    blur_->setObjectName(QStringLiteral("appearanceGlassBlurChoice"));
    blur_->setAccessibleName(tr("玻璃模糊程度"));
    blur_->setRange(glassBlurMinimum, glassBlurMaximum);
    blurValue_->setObjectName(QStringLiteral("appearanceGlassBlurValue"));
    blurValue_->setMinimumWidth(38);
    for (const auto theme : AppearancePreferences::themes())
        theme_->addItem(AppearancePreferences::themeName(theme), static_cast<int>(theme));
    for (const auto accent : AppearancePreferences::accents())
        accent_->addItem(AppearancePreferences::accentName(accent), static_cast<int>(accent));
    theme_->setCurrentIndex(theme_->findData(static_cast<int>(baseline_.theme)));
    accent_->setCurrentIndex(accent_->findData(static_cast<int>(baseline_.accent)));
    glass_->setChecked(baseline_.glassEnabled);
    opacity_->setValue(baseline_.glassOpacity);
    blur_->setValue(baseline_.glassBlur);
    opacity_->setEnabled(glass_->isChecked());
    blur_->setEnabled(glass_->isChecked());
    opacityValue_->setEnabled(glass_->isChecked());
    blurValue_->setEnabled(glass_->isChecked());

    auto *form = new QFormLayout(this);
    form->addRow(tr("基础主题："), theme_);
    form->addRow(tr("强调色："), accent_);
    form->addRow(glass_);
    form->addRow(tr("玻璃不透明度："), sliderRow(opacity_, opacityValue_));
    form->addRow(tr("玻璃模糊程度："), sliderRow(blur_, blurValue_));
    auto *help =
        new QLabel(tr("更改会立即预览，应用后保存。玻璃质感仅作用于界面面板，不会模糊视频；"
                      "不透明度控制面板半透明程度，模糊程度调节面板渐变的柔化层次。"
                      "窗口外框的背景模糊由支持该能力的 Wayland 合成器提供。"),
                   this);
    help->setWordWrap(true);
    form->addRow(help);

    const auto updateOpacity = [this](int value) {
        opacityValue_->setText(QStringLiteral("%1%").arg(value));
        emit edited();
    };
    const auto updateBlur = [this](int value) {
        blurValue_->setText(QStringLiteral("%1%").arg(value));
        emit edited();
    };
    const auto notify = [this] { emit edited(); };
    connect(theme_, &QComboBox::currentIndexChanged, this, notify);
    connect(accent_, &QComboBox::currentIndexChanged, this, notify);
    connect(glass_, &QCheckBox::toggled, this, [this](bool enabled) {
        opacity_->setEnabled(enabled);
        blur_->setEnabled(enabled);
        opacityValue_->setEnabled(enabled);
        blurValue_->setEnabled(enabled);
        emit edited();
    });
    connect(opacity_, &QSlider::valueChanged, this, updateOpacity);
    connect(blur_, &QSlider::valueChanged, this, updateBlur);
    updateOpacity(opacity_->value());
    updateBlur(blur_->value());
}

AppearancePreferences::Settings PreferencesAppearancePage::draft() const {
    return {static_cast<AppearancePreferences::Theme>(theme_->currentData().toInt()),
            static_cast<AppearancePreferences::Accent>(accent_->currentData().toInt()),
            glass_->isChecked(), opacity_->value(), blur_->value()};
}

bool PreferencesAppearancePage::changed() const { return draft().normalized() != baseline_; }

void PreferencesAppearancePage::restoreDefaults() {
    const AppearancePreferences::Settings defaults;
    theme_->setCurrentIndex(theme_->findData(static_cast<int>(defaults.theme)));
    accent_->setCurrentIndex(accent_->findData(static_cast<int>(defaults.accent)));
    glass_->setChecked(defaults.glassEnabled);
    opacity_->setValue(defaults.glassOpacity);
    blur_->setValue(defaults.glassBlur);
}

void PreferencesAppearancePage::accept() { baseline_ = draft().normalized(); }
