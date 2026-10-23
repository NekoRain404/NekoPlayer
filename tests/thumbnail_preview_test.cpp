#include "ui/thumbnailpreview.h"

#include <QImage>
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

        preview.clear();
        QVERIFY(preview.isHidden());
    }
};

QTEST_MAIN(ThumbnailPreviewTest)
#include "thumbnail_preview_test.moc"
