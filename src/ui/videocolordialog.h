#pragma once

#include "playback/videocoloradjustment.h"
#include <QDialog>
#include <array>

class MpvController;
class QDialogButtonBox;
class QLabel;
class QSlider;

class VideoColorDialog final : public QDialog {
    Q_OBJECT
  public:
    explicit VideoColorDialog(MpvController &player, QWidget *parent = nullptr);

  private:
    [[nodiscard]] VideoColorAdjustment draft() const;
    void display(const VideoColorAdjustment &adjustment);
    void preview();
    void apply();
    void updateApplyButton();

    MpvController &player_;
    VideoColorAdjustment rollback_;
    std::array<QSlider *, 5> sliders_{};
    std::array<QLabel *, 5> values_{};
    QDialogButtonBox *buttons_ = nullptr;
};
