#include "ui/theme/appicon.h"

#include <QColor>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QRect>
#include <QSize>
#include <QtTest>
#include <array>

namespace {
constexpr std::array<int, 7> kExpectedSizes = {16, 24, 32, 48, 64, 128, 256};
const QColor kAccentColor(0x8b, 0xa9, 0xef);
const QColor kTileColor(0x1f, 0x22, 0x28);

bool isClose(const QColor &actual, const QColor &expected, int tolerance = 8) {
    return qAbs(actual.red() - expected.red()) <= tolerance &&
           qAbs(actual.green() - expected.green()) <= tolerance &&
           qAbs(actual.blue() - expected.blue()) <= tolerance;
}

bool hasAccentPixel(const QImage &image) {
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            if (color.alpha() > 128 && color.blue() - color.red() > 40 &&
                color.blue() - color.green() > 20)
                return true;
        }
    }
    return false;
}

bool hasOpaquePixelIn(const QImage &image, const QRect &region) {
    for (int y = region.top(); y <= region.bottom(); ++y)
        for (int x = region.left(); x <= region.right(); ++x)
            if (image.pixelColor(x, y).alpha() >= 128)
                return true;
    return false;
}
} // namespace

class AppIconTest final : public QObject {
    Q_OBJECT
  private slots:
    void providesExpectedSizes();
    void paintsTileAndPlayMark();
    void remainsLegibleAtSmallSizes();
};

void AppIconTest::providesExpectedSizes() {
    const QIcon icon = AppIcon::windowIcon();
    QVERIFY(!icon.isNull());
    const auto sizes = icon.availableSizes();
    for (const int size : kExpectedSizes)
        QVERIFY2(sizes.contains(QSize(size, size)),
                 qPrintable(QStringLiteral("missing %1 px icon").arg(size)));
}

void AppIconTest::paintsTileAndPlayMark() {
    const QIcon icon = AppIcon::windowIcon();
    for (const int size : kExpectedSizes) {
        const QPixmap pixmap = icon.pixmap(QSize(size, size));
        QVERIFY(!pixmap.isNull());
        QCOMPARE(pixmap.size(), QSize(size, size));
        QCOMPARE(pixmap.toImage().pixelColor(0, 0).alpha(), 0);
    }

    const QImage image = icon.pixmap(QSize(256, 256)).toImage();
    QCOMPARE(image.pixelColor(0, 0).alpha(), 0);
    QVERIFY(isClose(image.pixelColor(40, 150), kTileColor));
    QVERIFY(isClose(image.pixelColor(144, 144), kAccentColor));
}

void AppIconTest::remainsLegibleAtSmallSizes() {
    const QIcon icon = AppIcon::windowIcon();
    for (const int size : kExpectedSizes) {
        const QImage image = icon.pixmap(QSize(size, size)).toImage();
        QVERIFY2(hasAccentPixel(image),
                 qPrintable(QStringLiteral("no play mark at %1 px").arg(size)));
    }
    const QImage small = icon.pixmap(QSize(16, 16)).toImage();
    QCOMPARE(small.size(), QSize(16, 16));
    const int earHeight = small.height() / 4;
    const int leftEarWidth = small.width() / 2;
    QVERIFY2(hasOpaquePixelIn(small, QRect(0, 0, leftEarWidth, earHeight)),
             "left ear is not opaque at 16 px");
    QVERIFY2(
        hasOpaquePixelIn(small, QRect(leftEarWidth, 0, small.width() - leftEarWidth, earHeight)),
        "right ear is not opaque at 16 px");
}

QTEST_MAIN(AppIconTest)
#include "app_icon_test.moc"
