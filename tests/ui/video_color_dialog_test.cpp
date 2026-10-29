#include "playback/mpv/mpvcontroller.h"
#include "ui/dialogs/videocolordialog.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QSlider>
#include <QtTest>

class VideoColorDialogTest final : public QObject {
    Q_OBJECT
  private slots:
    void cancelRollsBackToLastAppliedValues() {
        MpvController player(true, true);
        player.setVideoColorAdjustment({10, 0, 0, 0, 0});
        VideoColorDialog dialog(player);
        auto *brightness = dialog.findChild<QSlider *>(QStringLiteral("brightnessSlider"));
        QVERIFY(brightness);
        brightness->setValue(20);
        QCOMPARE(player.videoColorAdjustment().brightness, 20.0);
        auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("videoColorButtons"));
        QVERIFY(buttons);
        buttons->button(QDialogButtonBox::Cancel)->click();
        QCOMPARE(player.videoColorAdjustment().brightness, 10.0);
    }

    void applyMovesTheCancelRollbackPoint() {
        MpvController player(true, true);
        VideoColorDialog dialog(player);
        auto *brightness = dialog.findChild<QSlider *>(QStringLiteral("brightnessSlider"));
        QVERIFY(brightness);
        brightness->setValue(25);
        auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("videoColorButtons"));
        QVERIFY(buttons);
        buttons->button(QDialogButtonBox::Apply)->click();
        brightness->setValue(40);
        buttons->button(QDialogButtonBox::Cancel)->click();
        QCOMPARE(player.videoColorAdjustment().brightness, 25.0);
    }
};

QTEST_MAIN(VideoColorDialogTest)
#include "video_color_dialog_test.moc"
