#include "playback/mpv/mpvcontroller.h"
#include "ui/dialogs/preferencesdialog.h"
#include "ui/theme/appearancepreferences.h"
#include "ui/theme/appearancetokens.h"
#include "ui/window/mainwindow.h"

#include <QApplication>
#include <QCheckBox>
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

    void disablingGlassInPreferencesKeepsMainWindowTransportOpaque() {
        MpvController player(true, true);
        MainWindow window(true);
        PreferencesDialog preferences(
            player, nullptr, {}, true, 5,
            {AppearancePreferences::Theme::Dark, AppearancePreferences::Accent::Ocean, true});
        auto *glass = preferences.findChild<QCheckBox *>(QStringLiteral("appearanceGlassChoice"));
        auto *transport = window.findChild<QWidget *>(QStringLiteral("transportBar"));
        QVERIFY(glass && transport);
        QVERIFY(glass->isChecked());

        glass->setChecked(false);

        QVERIFY(!qApp->property("glassEnabled").toBool());
        QVERIFY(!transport->testAttribute(Qt::WA_TranslucentBackground));
        const auto expected = appearanceTokens(AppearancePreferences::Theme::Dark,
                                               AppearancePreferences::Accent::Ocean)
                                  .surface;
        transport->resize(400, transport->sizeHint().height());
        QImage image(transport->size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        transport->render(&image, {}, {}, QWidget::DrawWindowBackground);
        QCOMPARE(image.pixelColor(20, 5), expected);
        preferences.reject();
        AppearancePreferences::applyTheme(*qApp, AppearancePreferences::Theme::System);
    }
};

QTEST_MAIN(MainWindowTransportMaterialTest)
#include "main_window_transport_material_test.moc"
