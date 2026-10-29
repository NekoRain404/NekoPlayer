#include "ui/panels/thumbnailpreview.h"
#include "ui/theme/appearancetokens.h"

#include <QImage>
#include <QPropertyAnimation>
#include <QtTest>

class ThumbnailPreviewTest final : public QObject {
    Q_OBJECT
  private slots:
    void presentsImageAndCanBeDismissed() {
        ThumbnailPreview preview;
        QImage image(QSize(160, 90), QImage::Format_RGB32);
        image.fill(Qt::red);
        preview.present(image, 12.5);
        QCOMPARE(preview.size(), QSize(248, 166));

        preview.show();
        QCoreApplication::processEvents();
        QVERIFY(!preview.grab().isNull());

        auto *animation = preview.findChild<QPropertyAnimation *>();
        QVERIFY(animation);
        animation->setCurrentTime(animation->duration());
        QCOMPARE(preview.contentOpacity(), 1.0);

        preview.clear();
        QCOMPARE(animation->duration(),
                 appearanceTokens(AppearancePreferences::Theme::Dark).motionDurationMs);
        QCOMPARE(animation->endValue().toReal(), 0.0);
        QVERIFY(preview.isVisible());
        animation->setCurrentTime(animation->duration());
        QVERIFY(preview.isHidden());
    }

    void imageRefreshDoesNotRestartAnActiveReveal() {
        ThumbnailPreview preview;
        QImage image(QSize(160, 90), QImage::Format_RGB32);
        image.fill(Qt::red);
        preview.present(image, 12.5);
        preview.show();
        QCoreApplication::processEvents();
        auto *animation = preview.findChild<QPropertyAnimation *>();
        QVERIFY(animation);
        animation->setCurrentTime(animation->duration() / 2);
        const auto opacity = preview.contentOpacity();
        QVERIFY(opacity > 0.0 && opacity < 1.0);

        image.fill(Qt::blue);
        preview.present(image, 13.0);
        QCOMPARE(animation->currentTime(), animation->duration() / 2);
        QCOMPARE(preview.contentOpacity(), opacity);
    }
};

QTEST_MAIN(ThumbnailPreviewTest)
#include "thumbnail_preview_test.moc"
