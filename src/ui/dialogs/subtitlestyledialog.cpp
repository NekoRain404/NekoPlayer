#include "ui/dialogs/subtitlestyledialog.h"

#include "playback/mpv/mpvcontroller.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace {
QString colorText(const QColor &color) {
    return QStringLiteral("#%1%2%3%4")
        .arg(color.alpha(), 2, 16, QLatin1Char('0'))
        .arg(color.red(), 2, 16, QLatin1Char('0'))
        .arg(color.green(), 2, 16, QLatin1Char('0'))
        .arg(color.blue(), 2, 16, QLatin1Char('0'))
        .toUpper();
}

QColor colorFromText(const QString &text) {
    const auto value = text.trimmed();
    if (value.size() != 9 || !value.startsWith(QLatin1Char('#')))
        return QColor(Qt::white);
    bool alphaValid = false;
    bool rValid = false;
    bool gValid = false;
    bool bValid = false;
    const auto alpha = value.mid(1, 2).toInt(&alphaValid, 16);
    const auto red = value.mid(3, 2).toInt(&rValid, 16);
    const auto green = value.mid(5, 2).toInt(&gValid, 16);
    const auto blue = value.mid(7, 2).toInt(&bValid, 16);
    if (!alphaValid || !rValid || !gValid || !bValid)
        return QColor(Qt::white);
    return QColor(red, green, blue, alpha);
}

void updateColorButton(QPushButton *button, const QString &color) {
    button->setProperty("subtitleColor", color);
    button->setText(color);
    button->setStyleSheet(QStringLiteral("QPushButton{background:%1;color:%2;}")
                              .arg(color, colorFromText(color).lightness() < 128
                                              ? QStringLiteral("#ffffff")
                                              : QStringLiteral("#000000")));
}
} // namespace

SubtitleStyleDialog::SubtitleStyleDialog(MpvController &controller, QWidget *parent)
    : QDialog(parent), controller_(controller) {
    setObjectName(QStringLiteral("subtitleStyleDialog"));
    setWindowTitle(tr("字幕样式"));
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    textColor_ =
        createColorButton(controller_.subtitlePreferences().textColor, "subtitleTextColor");
    outlineColor_ =
        createColorButton(controller_.subtitlePreferences().outlineColor, "subtitleOutlineColor");
    backgroundColor_ = createColorButton(controller_.subtitlePreferences().backgroundColor,
                                         "subtitleBackgroundColor");
    form->addRow(tr("文字颜色："), textColor_);
    form->addRow(tr("描边颜色："), outlineColor_);
    form->addRow(tr("背景颜色："), backgroundColor_);

    outlineSize_ = new QDoubleSpinBox(this);
    outlineSize_->setObjectName(QStringLiteral("subtitleOutlineSize"));
    outlineSize_->setRange(0.0, 10.0);
    outlineSize_->setSingleStep(0.25);
    outlineSize_->setDecimals(2);
    form->addRow(tr("描边宽度："), outlineSize_);

    shadowOffset_ = new QDoubleSpinBox(this);
    shadowOffset_->setObjectName(QStringLiteral("subtitleShadowOffset"));
    shadowOffset_->setRange(0.0, 10.0);
    shadowOffset_->setSingleStep(0.5);
    shadowOffset_->setDecimals(2);
    form->addRow(tr("阴影偏移："), shadowOffset_);

    codepage_ = new QComboBox(this);
    codepage_->setObjectName(QStringLiteral("subtitleCodepage"));
    codepage_->setEditable(true);
    codepage_->addItems({QStringLiteral("auto"), QStringLiteral("UTF-8"), QStringLiteral("GB18030"),
                         QStringLiteral("BIG5"), QStringLiteral("Shift_JIS"),
                         QStringLiteral("CP1252")});
    form->addRow(tr("文字编码："), codepage_);
    layout->addLayout(form);

    auto *hint =
        new QLabel(tr("编码选择用于非 UTF-8 的外挂字幕；自动模式交给 libmpv 判断。"), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        controller_.setSubtitlePreferences(draft());
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this,
            [this] { setControls(SubtitlePreferences{}); });
    layout->addWidget(buttons);
    setControls(controller_.subtitlePreferences());
}

QPushButton *SubtitleStyleDialog::createColorButton(const QString &color, const char *objectName) {
    auto *button = new QPushButton(this);
    button->setObjectName(QString::fromLatin1(objectName));
    button->setMinimumWidth(120);
    updateColorButton(button, color);
    connect(button, &QPushButton::clicked, this, [this, button] { chooseColor(button); });
    return button;
}

void SubtitleStyleDialog::chooseColor(QPushButton *button) {
    const auto current = colorFromText(button->property("subtitleColor").toString());
    const auto chosen =
        QColorDialog::getColor(current, this, tr("选择字幕颜色"), QColorDialog::ShowAlphaChannel);
    if (chosen.isValid())
        updateColorButton(button, colorText(chosen));
}

SubtitlePreferences SubtitleStyleDialog::draft() const {
    auto preferences = controller_.subtitlePreferences();
    preferences.textColor = textColor_->property("subtitleColor").toString();
    preferences.outlineColor = outlineColor_->property("subtitleColor").toString();
    preferences.backgroundColor = backgroundColor_->property("subtitleColor").toString();
    preferences.outlineSize = outlineSize_->value();
    preferences.shadowOffset = shadowOffset_->value();
    preferences.codepage = codepage_->currentText();
    return preferences.normalized();
}

void SubtitleStyleDialog::setControls(const SubtitlePreferences &preferences) {
    const auto normalized = preferences.normalized();
    updateColorButton(textColor_, normalized.textColor);
    updateColorButton(outlineColor_, normalized.outlineColor);
    updateColorButton(backgroundColor_, normalized.backgroundColor);
    outlineSize_->setValue(normalized.outlineSize);
    shadowOffset_->setValue(normalized.shadowOffset);
    codepage_->setCurrentText(normalized.codepage);
}
