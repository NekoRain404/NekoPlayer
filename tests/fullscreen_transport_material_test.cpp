#include "ui/appearancepreferences.h"
#include "ui/fullscreencontrols.h"
#include "ui/transportbar.h"

#include <QAction>
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QPropertyAnimation>
#include <QVBoxLayout>
#include <QtTest>

class FullscreenTransportMaterialTest final : public QObject {
    Q_OBJECT
  private slots:
    void fullscreenGlassIsMoreOpaqueThanWindowedGlass() {
        QWidget host;
        QVBoxLayout layout(&host);
        QWidget video(&host);
        QAction action;
        TransportBar transport(&action, &action, &action, &action, &action, &action, &action,
                               &host);
        layout.addWidget(&video);
        layout.addWidget(&transport);
        FullscreenControls controls(transport, video, layout);
        transport.resize(400, 120);
        AppearancePreferences::applySettings(*qApp, {AppearancePreferences::Theme::Dark,
                                                     AppearancePreferences::Accent::Ocean, true, 72,
                                                     glassBlurDefault});

        const auto windowedFill = renderBackground(transport);
        controls.enter();
        controls.reveal();
        auto *animation = controls.findChild<QPropertyAnimation *>();
        QVERIFY(animation);
        animation->setCurrentTime(animation->duration());
        const auto fullscreenFill = renderBackground(transport);

        QVERIFY(fullscreenFill.alpha() > windowedFill.alpha());
        QVERIFY(fullscreenFill.alpha() >= 225);
        QVERIFY(fullscreenFill != windowedFill);

        controls.leave();
        AppearancePreferences::applySettings(*qApp, {AppearancePreferences::Theme::Light,
                                                     AppearancePreferences::Accent::Rose, false, 72,
                                                     glassBlurDefault});
        controls.enter();
        controls.reveal();
        animation->setCurrentTime(animation->duration());
        const auto opaqueFill = renderBackground(transport);
        QCOMPARE(opaqueFill.alpha(), 255);
        controls.leave();
        AppearancePreferences::applyTheme(*qApp, AppearancePreferences::Theme::System);
    }

  private:
    static QColor renderBackground(QWidget &widget) {
        QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        widget.render(&painter, {}, {}, QWidget::DrawWindowBackground);
        return image.pixelColor(5, 5);
    }
};

QTEST_MAIN(FullscreenTransportMaterialTest)
#include "fullscreen_transport_material_test.moc"
