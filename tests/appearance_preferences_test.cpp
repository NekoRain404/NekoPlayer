#include "ui/appearancepreferences.h"

#include <QApplication>
#include <QPalette>
#include <QTranslator>
#include <QtTest>

class AppearancePreferencesTest final : public QObject {
    Q_OBJECT
  private slots:
    void exposesThreeThemesAndAppliesPalettes() {
        QCOMPARE(AppearancePreferences::themes().size(), 3);
        AppearancePreferences::applyTheme(*qApp, AppearancePreferences::Theme::Dark);
        QVERIFY(qApp->palette().color(QPalette::Window).value() < 80);
        QVERIFY(!qApp->styleSheet().isEmpty());

        AppearancePreferences::applyTheme(*qApp, AppearancePreferences::Theme::Light);
        QVERIFY(qApp->palette().color(QPalette::Window).value() > 220);

        AppearancePreferences::applyTheme(*qApp, AppearancePreferences::Theme::System);
        QVERIFY(qApp->styleSheet().isEmpty());
    }

    void loadsBundledEnglishTranslations() {
        QTranslator translator;
        QVERIFY(translator.load(QStringLiteral(":/i18n/NekoPlayer_en.qm")));
        qApp->installTranslator(&translator);
        QCOMPARE(AppearancePreferences::themeName(AppearancePreferences::Theme::Dark),
                 QStringLiteral("Dark"));
        QCOMPARE(AppearancePreferences::themeName(AppearancePreferences::Theme::Light),
                 QStringLiteral("Light"));
        qApp->removeTranslator(&translator);
    }
};

QTEST_MAIN(AppearancePreferencesTest)
#include "appearance_preferences_test.moc"
