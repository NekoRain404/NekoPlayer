#include "ui/languagemanager.h"

#include <QCoreApplication>
#include <QtTest>

class LanguageManagerTest final : public QObject {
    Q_OBJECT
  private slots:
    void switchesInstalledTranslationWithoutRestart() {
        LanguageManager languages(*QCoreApplication::instance());

        QVERIFY(languages.setLanguage(QStringLiteral("en")));
        QCOMPARE(QCoreApplication::translate("QObject", "深色"), QStringLiteral("Dark"));

        QVERIFY(languages.setLanguage(QStringLiteral("zh_CN")));
        QCOMPARE(QCoreApplication::translate("QObject", "深色"), QStringLiteral("深色"));
    }

    void rejectsUnsupportedLanguageWithoutChangingCurrentTranslation() {
        LanguageManager languages(*QCoreApplication::instance());
        QVERIFY(languages.setLanguage(QStringLiteral("en")));

        QVERIFY(!languages.setLanguage(QStringLiteral("fr")));
        QCOMPARE(languages.currentLanguage(), QStringLiteral("en"));
        QCOMPARE(QCoreApplication::translate("QObject", "深色"), QStringLiteral("Dark"));
    }

    void removesTranslatorWhenManagerIsDestroyed() {
        {
            LanguageManager languages(*QCoreApplication::instance());
            QVERIFY(languages.setLanguage(QStringLiteral("en")));
            QCOMPARE(QCoreApplication::translate("QObject", "深色"), QStringLiteral("Dark"));
        }
        QCOMPARE(QCoreApplication::translate("QObject", "深色"), QStringLiteral("深色"));
    }
};

QTEST_MAIN(LanguageManagerTest)
#include "languagemanager_test.moc"
