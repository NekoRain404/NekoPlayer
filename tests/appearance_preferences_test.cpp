#include "ui/appearancepreferences.h"
#include "ui/transportbar.h"

#include <QAction>
#include <QApplication>
#include <QPalette>
#include <QToolButton>
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

    void exposesContentFirstInteractionStyle() {
        AppearancePreferences::applyTheme(*qApp, AppearancePreferences::Theme::Dark);
        const auto style = qApp->styleSheet();
        QVERIFY(style.contains(QStringLiteral("QWidget#playlistPane")));
        QVERIFY(style.contains(QStringLiteral("QSlider#timelineSlider")));
        QVERIFY(style.contains(QStringLiteral("QToolButton:pressed")));
        QVERIFY(style.contains(QStringLiteral("QToolButton:checked")));
        QVERIFY(style.contains(QStringLiteral("QListView#playlistView::item:hover")));
        QVERIFY(style.contains(QStringLiteral("QListView#playlistView::item:selected")));
        QVERIFY(style.contains(QStringLiteral("QPushButton:focus")));
        QVERIFY(style.contains(QStringLiteral("QPushButton:pressed")));
        QVERIFY(style.contains(QStringLiteral("QSlider:focus")));
        QVERIFY(style.contains(QStringLiteral("QMenu::item:disabled")));
        QVERIFY(style.contains(QStringLiteral("QFocusFrame")));

        AppearancePreferences::applyTheme(*qApp, AppearancePreferences::Theme::Light);
        const auto lightStyle = qApp->styleSheet();
        QVERIFY(lightStyle.contains(QStringLiteral("QToolButton:pressed")));
        QVERIFY(lightStyle.contains(QStringLiteral("QListView#playlistView::item:selected")));
        QVERIFY(lightStyle.contains(QStringLiteral("QSlider#timelineSlider")));
        QVERIFY(lightStyle.contains(QStringLiteral("QPushButton:pressed")));
    }

    void transportButtonsAreKeyboardAndScreenReaderAccessible() {
        QAction open(QStringLiteral("打开文件"));
        QAction previous(QStringLiteral("上一个"));
        QAction pause(QStringLiteral("播放"));
        QAction next(QStringLiteral("下一个"));
        QAction stop(QStringLiteral("停止"));
        QAction mute(QStringLiteral("静音"));
        QAction fullscreen(QStringLiteral("全屏"));
        TransportBar transport(&open, &previous, &pause, &next, &stop, &mute, &fullscreen);

        const auto buttons = transport.findChildren<QToolButton *>();
        QCOMPARE(buttons.size(), 7);
        for (auto *button : buttons) {
            QVERIFY(button->focusPolicy() != Qt::NoFocus);
            QVERIFY(!button->accessibleName().isEmpty());
            QCOMPARE(button->toolTip(), button->accessibleName());
        }
    }
};

QTEST_MAIN(AppearancePreferencesTest)
#include "appearance_preferences_test.moc"
