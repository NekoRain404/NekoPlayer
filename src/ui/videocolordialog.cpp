#include "videocolordialog.h"
#include "playback/mpvcontroller.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

VideoColorDialog::VideoColorDialog(MpvController &player, QWidget *parent)
    : QDialog(parent), player_(player), rollback_(player.videoColorAdjustment()),
      buttons_(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                                        QDialogButtonBox::Apply | QDialogButtonBox::RestoreDefaults,
                                    this)) {
    setObjectName(QStringLiteral("videoColorDialog"));
    setWindowTitle(tr("视频色彩调整"));
    setAttribute(Qt::WA_TranslucentBackground, true);
    buttons_->setObjectName(QStringLiteral("videoColorButtons"));
    auto *form = new QFormLayout;
    const std::array<QString, 5> labels = {tr("亮度"), tr("对比度"), tr("饱和度"), tr("色调"),
                                           tr("伽马")};
    const std::array<const char *, 5> names = {"brightness", "contrast", "saturation", "hue",
                                               "gamma"};
    for (std::size_t index = 0; index < sliders_.size(); ++index) {
        auto *row = new QWidget(this);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        sliders_[index] = new QSlider(Qt::Horizontal, row);
        sliders_[index]->setObjectName(QString::fromLatin1(names[index]) +
                                       QStringLiteral("Slider"));
        sliders_[index]->setRange(-100, 100);
        sliders_[index]->setTracking(true);
        sliders_[index]->setAccessibleName(labels[index]);
        values_[index] = new QLabel(row);
        values_[index]->setObjectName(QString::fromLatin1(names[index]) + QStringLiteral("Value"));
        values_[index]->setMinimumWidth(36);
        values_[index]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rowLayout->addWidget(sliders_[index], 1);
        rowLayout->addWidget(values_[index]);
        form->addRow(labels[index] + QStringLiteral("："), row);
        connect(sliders_[index], &QSlider::valueChanged, this, [this, index](int value) {
            values_[index]->setText(QString::number(value));
            preview();
        });
    }

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons_);
    connect(buttons_->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
            &VideoColorDialog::apply);
    connect(buttons_->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this,
            [this] { display({}); });
    connect(buttons_, &QDialogButtonBox::accepted, this, [this] {
        apply();
        accept();
    });
    connect(buttons_, &QDialogButtonBox::rejected, this, [this] {
        player_.setVideoColorAdjustment(rollback_);
        reject();
    });
    display(player_.videoColorAdjustment());
    updateApplyButton();
    resize(520, sizeHint().height());
}

VideoColorAdjustment VideoColorDialog::draft() const {
    return {static_cast<double>(sliders_[0]->value()), static_cast<double>(sliders_[1]->value()),
            static_cast<double>(sliders_[2]->value()), static_cast<double>(sliders_[3]->value()),
            static_cast<double>(sliders_[4]->value())};
}

void VideoColorDialog::display(const VideoColorAdjustment &adjustment) {
    const auto normalized = adjustment.normalized();
    const std::array values = {normalized.brightness, normalized.contrast, normalized.saturation,
                               normalized.hue, normalized.gamma};
    for (std::size_t index = 0; index < sliders_.size(); ++index) {
        const QSignalBlocker blocker(sliders_[index]);
        const int value = qRound(values[index]);
        sliders_[index]->setValue(value);
        values_[index]->setText(QString::number(value));
    }
    preview();
}

void VideoColorDialog::preview() {
    player_.setVideoColorAdjustment(draft());
    updateApplyButton();
}

void VideoColorDialog::apply() {
    player_.setVideoColorAdjustment(draft());
    rollback_ = player_.videoColorAdjustment();
    updateApplyButton();
}

void VideoColorDialog::updateApplyButton() {
    buttons_->button(QDialogButtonBox::Apply)
        ->setEnabled(player_.isReady() && draft() != rollback_);
}
