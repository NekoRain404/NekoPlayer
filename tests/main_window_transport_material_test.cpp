#include "ui/appearancepreferences.h"
#include "ui/appearancetokens.h"
#include "ui/mainwindow.h"

#include <QApplication>
#include <QImage>
#include <QtTest>

class MainWindowTransportMaterialTest final : public QObject {
    Q_OBJECT
  private slots:
    void transportRendersOpaqueSurfaceWithGlassOnAndOff() {
        MainWindow window(true);
        auto *transport = window.findChild<QWidget *>(QStringLiteral("transportBar"));
        QVERIFY(transport);
        QVERIFY(!transport->testAttribute(Qt::WA_TranslucentBackground));
        const auto expected = appearanceTokens(AppearancePreferences::Theme::Dark,
                                               AppearancePreferences::Accent::Ocean)
                                  .surface;
        for (const bool glass : {false, true}) {
            AppearancePreferences::applySettings(
                *qApp,
                {AppearancePreferences::Theme::Dark, AppearancePreferences::Accent::Ocean, glass});
            transport->resize(400, transport->sizeHint().height());
            QImage image(transport->size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            transport->render(&image, {}, {}, QWidget::DrawWindowBackground);
            QCOMPARE(image.pixelColor(20, 5), expected);
        }
        AppearancePreferences::applyTheme(*qApp, AppearancePreferences::Theme::System);
    }
};

QTEST_MAIN(MainWindowTransportMaterialTest)
#include "main_window_transport_material_test.moc"
