#include "playback/mpvcontroller.h"
#include "playback/videowidget.h"
#include "ui/appearancepreferences.h"
#include "ui/playbackoverlay.h"

#include <QApplication>
#include <QFrame>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QProgressBar>
#include <QtTest>

class PlaybackOverlayStyleTest final : public QObject {
    Q_OBJECT
  private slots:
    void playbackInformationAndSeekUseThemeTokens() {
        MpvController player(true, true);
        VideoWidget video(player);
        QWidget host;
        host.resize(800, 600);
        PlaybackOverlay playback(player, &host);
        playback.setGeometry(host.rect());
        AppearancePreferences::applySettings(
            *qApp, {AppearancePreferences::Theme::Light, AppearancePreferences::Accent::Rose});
        playback.setInformationVisible(true);
        host.show();
        playback.show();
        QCoreApplication::processEvents();

        auto *information =
            playback.findChild<QLabel *>(QStringLiteral("playbackInformationOverlay"));
        QVERIFY(information && information->isVisible());
        QImage image(information->size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        information->render(&image);
        const auto fill = image.pixelColor(image.width() / 2, 3);
        QVERIFY(fill.red() > 200);
        QVERIFY(fill.green() > 200);
        QVERIFY(fill.blue() > 200);

        auto *progress = playback.findChild<QProgressBar *>(QStringLiteral("seekProgressBar"));
        auto *seekPanel = playback.findChild<QFrame *>(QStringLiteral("seekProgressOverlay"));
        QVERIFY(progress && seekPanel);
        seekPanel->setGeometry(0, 0, 400, 90);
        seekPanel->show();
        progress->setGeometry(40, 40, 300, 8);
        progress->setValue(7500);
        progress->show();
        QImage progressImage(progress->size(), QImage::Format_ARGB32_Premultiplied);
        progressImage.fill(Qt::transparent);
        progress->render(&progressImage);
        QCOMPARE(progressImage.pixelColor(150, 4), QColor(179, 60, 109));

        QImage lowOpacity(information->size(), QImage::Format_ARGB32_Premultiplied);
        QImage highOpacity(information->size(), QImage::Format_ARGB32_Premultiplied);
        for (const auto [opacity, target] : {std::pair{glassOpacityMinimum, &lowOpacity},
                                             std::pair{glassOpacityMaximum, &highOpacity}}) {
            AppearancePreferences::applySettings(*qApp, {AppearancePreferences::Theme::Light,
                                                         AppearancePreferences::Accent::Rose, true,
                                                         opacity, 72});
            target->fill(Qt::transparent);
            information->render(target);
        }
        QVERIFY(lowOpacity.pixelColor(lowOpacity.width() / 2, 3) !=
                highOpacity.pixelColor(highOpacity.width() / 2, 3));

        QVERIFY(!video.graphicsEffect());
        QVERIFY(video.styleSheet().isEmpty());
        QVERIFY(!playback.graphicsEffect());
        QVERIFY(playback.styleSheet().isEmpty());
        QVERIFY(playback.testAttribute(Qt::WA_TranslucentBackground));
        AppearancePreferences::applyTheme(*qApp, AppearancePreferences::Theme::System);
    }

    void emptyVideoSurfaceClearColorStaysOpaqueWhenGlassIsEnabled() {
        QPalette palette;
        palette.setColor(QPalette::Base, QColor(11, 22, 33, 24));

        QCOMPARE(VideoWidget::emptySurfaceClearColor(palette, true), QColor(11, 22, 33, 255));
        QCOMPARE(VideoWidget::emptySurfaceClearColor(palette, false), QColor(Qt::black));
    }
};

QTEST_MAIN(PlaybackOverlayStyleTest)

#include "playback_overlay_style_test.moc"
