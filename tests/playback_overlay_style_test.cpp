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
        QVERIFY(qAbs(fill.red() - 236) <= 2);
        QVERIFY(qAbs(fill.green() - 236) <= 2);
        QVERIFY(qAbs(fill.blue() - 239) <= 2);

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

        QVERIFY(!video.graphicsEffect());
        QVERIFY(video.styleSheet().isEmpty());
        QVERIFY(!playback.graphicsEffect());
        QVERIFY(playback.styleSheet().isEmpty());
        QVERIFY(playback.testAttribute(Qt::WA_TranslucentBackground));
        AppearancePreferences::applyTheme(*qApp, AppearancePreferences::Theme::System);
    }
};

QTEST_MAIN(PlaybackOverlayStyleTest)

#include "playback_overlay_style_test.moc"
