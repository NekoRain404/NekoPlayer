#include "ui/dialogs/equalizerdialog.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <array>

namespace {
constexpr std::array bands{31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
}

EqualizerDialog::EqualizerDialog(const QList<double> &gains, QWidget *parent) : QDialog(parent) {
    setObjectName(QStringLiteral("equalizerDialog"));
    setWindowTitle(tr("自定义均衡器"));
    setAttribute(Qt::WA_TranslucentBackground, true);
    auto *layout = new QVBoxLayout(this);
    auto *grid = new QGridLayout;
    for (size_t index = 0; index < bands.size(); ++index) {
        auto *label = new QLabel(QStringLiteral("%1 Hz").arg(bands[index]), this);
        auto *slider = new QSlider(Qt::Horizontal, this);
        slider->setObjectName(QStringLiteral("equalizerBand%1").arg(index));
        slider->setAccessibleName(tr("%1 Hz 增益").arg(bands[index]));
        slider->setRange(-120, 120);
        slider->setSingleStep(5);
        auto *value = new QLabel(this);
        value->setMinimumWidth(64);
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        sliders_.append(slider);
        values_.append(value);
        grid->addWidget(label, static_cast<int>(index), 0);
        grid->addWidget(slider, static_cast<int>(index), 1);
        grid->addWidget(value, static_cast<int>(index), 2);
        connect(slider, &QSlider::valueChanged, this, &EqualizerDialog::updateValues);
    }
    grid->setColumnStretch(1, 1);
    layout->addLayout(grid);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto *reset = buttons->addButton(tr("全部归零"), QDialogButtonBox::ResetRole);
    connect(reset, &QPushButton::clicked, this, [this] {
        for (auto *slider : sliders_)
            slider->setValue(0);
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    setGains(gains);
    updateValues();
}

QList<double> EqualizerDialog::gains() const {
    QList<double> result;
    for (const auto *slider : sliders_)
        result.append(slider->value() / 10.0);
    return result;
}

void EqualizerDialog::setGains(const QList<double> &gains) {
    for (qsizetype index = 0; index < sliders_.size(); ++index) {
        const auto value = index < gains.size() ? gains.at(index) : 0.0;
        sliders_.at(index)->setValue(
            std::clamp(static_cast<int>(qRound(value * 10.0)), -120, 120));
    }
}

void EqualizerDialog::updateValues() {
    for (qsizetype index = 0; index < values_.size(); ++index)
        values_.at(index)->setText(QStringLiteral("%1%2 dB")
                                       .arg(sliders_.at(index)->value() >= 0 ? QStringLiteral("+")
                                                                             : QString{})
                                       .arg(sliders_.at(index)->value() / 10.0, 0, 'f', 1));
}
