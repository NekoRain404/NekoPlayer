#include "ui/theme/languagemanager.h"

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
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Apply"), QStringLiteral("应用"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"), QStringLiteral("取消"));
    }

    void translatesHighFrequencyPlaybackContexts() {
        LanguageManager languages(*QCoreApplication::instance());
        QVERIFY(languages.setLanguage(QStringLiteral("en")));

        QCOMPARE(QCoreApplication::translate("ActionFeedbackText", "前进 %1 秒"),
                 QStringLiteral("Seek Forward %1 Seconds"));
        QCOMPARE(QCoreApplication::translate("ActionFeedbackText", "跳转至 %1"),
                 QStringLiteral("Seek to %1"));
        QCOMPARE(QCoreApplication::translate("ActionFeedbackText", "已静音"),
                 QStringLiteral("Muted"));
        QCOMPARE(QCoreApplication::translate("MediaInformation", "视频编码"),
                 QStringLiteral("Video codec"));
        QCOMPARE(QCoreApplication::translate("MediaInformation", "采样格式"),
                 QStringLiteral("Sample format"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "正在播放 · %1"),
                 QStringLiteral("Playing · %1"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "无法播放：%1"),
                 QStringLiteral("Could not play: %1"));
        QCOMPARE(QCoreApplication::translate("FolderMenu", "取消"), QStringLiteral("Cancel"));
        QCOMPARE(QCoreApplication::translate("VideoMenu", "去隔行"), QStringLiteral("Deinterlace"));
        QCOMPARE(QCoreApplication::translate("PlaylistPane", "从列表移除"),
                 QStringLiteral("Remove from Playlist"));
        QCOMPARE(QCoreApplication::translate("PlaybackOverlay", "当前播放信息"),
                 QStringLiteral("Current Playback Information"));
        QCOMPARE(QCoreApplication::translate("TransportBar", "播放进度"),
                 QStringLiteral("Playback position"));
        QCOMPARE(QCoreApplication::translate("MediaInformationDialog", "复制"),
                 QStringLiteral("Copy"));
        QCOMPARE(QCoreApplication::translate("NavigationMenu", "定位"), QStringLiteral("Navigate"));
        QCOMPARE(QCoreApplication::translate("TuningMenus", "播放速度"),
                 QStringLiteral("Playback Speed"));
        QCOMPARE(QCoreApplication::translate("AbLoopMenu", "将当前位置设为起点 A"),
                 QStringLiteral("Set Current Position as A"));
        QCOMPARE(QCoreApplication::translate("PlaylistModeMenu", "不循环"),
                 QStringLiteral("No Repeat"));
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
