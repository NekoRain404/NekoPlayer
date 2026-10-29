#include "ui/panels/seekslidergeometry.h"

#include <QTest>

class SeekSliderGeometryTest final : public QObject {
    Q_OBJECT

  private slots:
    void horizontalTrackMapsToItsEndpointsAndMiddle() {
        QStyleOptionSlider option;
        option.orientation = Qt::Horizontal;
        option.minimum = 0;
        option.maximum = 10000;
        const QRect groove(10, 0, 120, 8);
        const QSize handle(20, 12);

        QCOMPARE(SeekSliderGeometry::valueAtPosition(option, groove, handle, QPoint(20, 4)), 0);
        QCOMPARE(SeekSliderGeometry::valueAtPosition(option, groove, handle, QPoint(70, 4)), 5000);
        QCOMPARE(SeekSliderGeometry::valueAtPosition(option, groove, handle, QPoint(120, 4)),
                 10000);
        QCOMPARE(SeekSliderGeometry::valueAtPosition(option, groove, handle, QPoint(-50, 4)), 0);
        QCOMPARE(SeekSliderGeometry::valueAtPosition(option, groove, handle, QPoint(500, 4)),
                 10000);
    }

    void invertedTrackReversesDirection() {
        QStyleOptionSlider option;
        option.orientation = Qt::Horizontal;
        option.minimum = 0;
        option.maximum = 10000;
        option.upsideDown = true;
        const QRect groove(10, 0, 120, 8);
        const QSize handle(20, 12);

        QCOMPARE(SeekSliderGeometry::valueAtPosition(option, groove, handle, QPoint(20, 4)), 10000);
        QCOMPARE(SeekSliderGeometry::valueAtPosition(option, groove, handle, QPoint(120, 4)), 0);
    }

    void verticalTrackUsesVerticalCoordinate() {
        QStyleOptionSlider option;
        option.orientation = Qt::Vertical;
        option.minimum = 0;
        option.maximum = 100;
        const QRect groove(0, 20, 8, 120);
        const QSize handle(12, 20);

        QCOMPARE(SeekSliderGeometry::valueAtPosition(option, groove, handle, QPoint(4, 30)), 0);
        QCOMPARE(SeekSliderGeometry::valueAtPosition(option, groove, handle, QPoint(4, 80)), 50);
        QCOMPARE(SeekSliderGeometry::valueAtPosition(option, groove, handle, QPoint(4, 130)), 100);
    }
};

QTEST_GUILESS_MAIN(SeekSliderGeometryTest)
#include "seek_slider_geometry_test.moc"
