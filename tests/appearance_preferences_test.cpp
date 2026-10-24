#include "playback/mpvcontroller.h"
#include "ui/appearancetokens.h"
#include "ui/appearancepreferences.h"
#include "ui/transportbar.h"
#include "ui/transportbarmetrics.h"

#include <QAction>
#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QPalette>
#include <QSignalSpy>
#include <QSlider>
#include <QToolButton>
#include <QTranslator>
#include <QVBoxLayout>
#include <QtTest>

class AppearancePreferencesTest final : public QObject {
    Q_OBJECT
  private slots:
    void darkAndLightTokensKeepSemanticContrastAndSharedMetrics() {
        const auto dark = appearanceTokens(AppearancePreferences::Theme::Dark);
        const auto light = appearanceTokens(AppearancePreferences::Theme::Light);
        QVERIFY(dark.canvas.value() < dark.primaryText.value());
        QVERIFY(light.canvas.value() > light.primaryText.value());
        QVERIFY(dark.accent != dark.canvas);
        QVERIFY(light.accent != light.canvas);
        QCOMPARE(dark.spaceUnit, light.spaceUnit);
        QCOMPARE(dark.transportHitTarget, 32);
    }

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
        QVERIFY(style.contains(QStringLiteral("QWidget#transportBar")));
        QVERIFY(style.contains(QStringLiteral("QFrame#transportDivider")));
        QVERIFY(style.contains(QStringLiteral("QLabel#timeLabel")));
        QVERIFY(style.contains(QStringLiteral("QSlider#volumeSlider")));
        QVERIFY(style.contains(QStringLiteral("QWidget#welcomePanel")));
        QVERIFY(style.contains(QStringLiteral("QLabel#playlistCount")));
        QVERIFY(style.contains(QStringLiteral("QPushButton#welcomeOpenButton")));
        QVERIFY(style.contains(QStringLiteral("QSlider#timelineSlider")));
        QVERIFY(style.contains(QStringLiteral("QToolButton:pressed")));
        QVERIFY(style.contains(QStringLiteral("QToolButton:checked")));
        QVERIFY(style.contains(QStringLiteral("QToolButton#transportPrimaryButton:hover")));
        QVERIFY(style.contains(QStringLiteral("QToolButton#transportPrimaryButton:checked")));
        QVERIFY(style.contains(QStringLiteral("QListView#playlistView::item:hover")));
        QVERIFY(style.contains(QStringLiteral("QListView#playlistView::item:selected")));
        QVERIFY(style.contains(QStringLiteral("QPushButton:focus")));
        QVERIFY(style.contains(QStringLiteral("QPushButton:pressed")));
        QVERIFY(style.contains(QStringLiteral("QSlider:focus")));
        QVERIFY(style.contains(QStringLiteral("QSlider::handle:horizontal:pressed")));
        QVERIFY(style.contains(QStringLiteral("QSlider::handle:horizontal:disabled")));
        QVERIFY(style.contains(QStringLiteral("QMenu::item:disabled")));
        QVERIFY(style.contains(QStringLiteral("QFocusFrame")));

        AppearancePreferences::applyTheme(*qApp, AppearancePreferences::Theme::Light);
        const auto lightStyle = qApp->styleSheet();
        QVERIFY(lightStyle.contains(QStringLiteral("QToolButton:pressed")));
        QVERIFY(lightStyle.contains(QStringLiteral("QToolButton#transportPrimaryButton:hover")));
        QVERIFY(lightStyle.contains(QStringLiteral("QToolButton#transportPrimaryButton:checked")));
        QVERIFY(lightStyle.contains(QStringLiteral("QListView#playlistView::item:selected")));
        QVERIFY(lightStyle.contains(QStringLiteral("QSlider#timelineSlider")));
        QVERIFY(lightStyle.contains(QStringLiteral("QWidget#welcomePanel")));
        QVERIFY(lightStyle.contains(QStringLiteral("QFrame#transportDivider")));
        QVERIFY(lightStyle.contains(QStringLiteral("QLabel#timeLabel")));
        QVERIFY(lightStyle.contains(QStringLiteral("QSlider#volumeSlider")));
        QVERIFY(lightStyle.contains(QStringLiteral("QPushButton#welcomeOpenButton")));
        QVERIFY(lightStyle.contains(QStringLiteral("QPushButton:pressed")));
        QVERIFY(lightStyle.contains(QStringLiteral("QSlider::handle:horizontal:pressed")));
        QVERIFY(lightStyle.contains(QStringLiteral("QSlider::handle:horizontal:disabled")));
    }

    void transportButtonsAreKeyboardAndScreenReaderAccessible() {
        QAction open(QStringLiteral("打开文件"));
        QAction previous(QStringLiteral("上一个"));
        QAction pause(QStringLiteral("播放"));
        QAction next(QStringLiteral("下一个"));
        QAction stop(QStringLiteral("停止"));
        QAction mute(QStringLiteral("静音"));
        QAction fullscreen(QStringLiteral("全屏"));
        QWidget host;
        TransportBar transport(&open, &previous, &pause, &next, &stop, &mute, &fullscreen, &host);

        const auto buttons = transport.findChildren<QToolButton *>();
        QCOMPARE(buttons.size(), 7);
        for (auto *button : buttons) {
            QVERIFY(button->focusPolicy() != Qt::NoFocus);
            QVERIFY(!button->accessibleName().isEmpty());
            QCOMPARE(button->toolTip(), button->accessibleName());
        }

        auto *primary =
            transport.findChild<QToolButton *>(QStringLiteral("transportPrimaryButton"));
        QVERIFY(primary);
        QCOMPARE(primary->defaultAction(), &pause);
        QCOMPARE(primary->iconSize(), QSize(26, 26));
    }

    void timelineExposesPositionAndDuration() {
        QAction open(QStringLiteral("打开文件"));
        QAction previous(QStringLiteral("上一个"));
        QAction pause(QStringLiteral("播放"));
        QAction next(QStringLiteral("下一个"));
        QAction stop(QStringLiteral("停止"));
        QAction mute(QStringLiteral("静音"));
        QAction fullscreen(QStringLiteral("全屏"));
        QWidget host;
        TransportBar transport(&open, &previous, &pause, &next, &stop, &mute, &fullscreen, &host);
        auto *hostLayout = new QVBoxLayout(&host);
        hostLayout->setContentsMargins(0, 0, 0, 0);
        hostLayout->addWidget(&transport);
        PlaybackState state;
        state.active = true;
        state.seekable = true;
        state.position = 75;
        state.duration = 120;
        transport.present(state);

        auto *timeline = transport.findChild<QSlider *>(QStringLiteral("timelineSlider"));
        QVERIFY(timeline);
        QCOMPARE(timeline->toolTip(), QStringLiteral("00:01:15 / 00:02:00"));
        QCOMPARE(timeline->accessibleDescription(), QStringLiteral("00:01:15 / 00:02:00"));
    }

    void timelineHoverReportsPointerPosition() {
        QAction open(QStringLiteral("打开文件"));
        QAction previous(QStringLiteral("上一个"));
        QAction pause(QStringLiteral("播放"));
        QAction next(QStringLiteral("下一个"));
        QAction stop(QStringLiteral("停止"));
        QAction mute(QStringLiteral("静音"));
        QAction fullscreen(QStringLiteral("全屏"));
        QWidget host;
        TransportBar transport(&open, &previous, &pause, &next, &stop, &mute, &fullscreen, &host);
        auto *hostLayout = new QVBoxLayout(&host);
        hostLayout->setContentsMargins(0, 0, 0, 0);
        hostLayout->addWidget(&transport);
        PlaybackState state;
        state.active = true;
        state.seekable = true;
        state.duration = 100;
        transport.present(state);
        host.show();
        host.resize(560, 100);
        QCoreApplication::processEvents();

        auto *timeline = transport.findChild<QSlider *>(QStringLiteral("timelineSlider"));
        QVERIFY(timeline);
        QSignalSpy hoverSpy(&transport, &TransportBar::timelineHovered);
        const QPoint localPoint(timeline->width() / 2, timeline->height() / 2);
        QMouseEvent move(QEvent::MouseMove, localPoint, timeline->mapToGlobal(localPoint),
                         Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(timeline, &move);
        QTRY_VERIFY(hoverSpy.count() > 0);
        QVERIFY(qAbs(hoverSpy.at(0).at(0).toDouble() - 50.0) < 1.0);
        QVERIFY(!hoverSpy.at(0).at(1).value<QPoint>().isNull());
    }

    void transportMetricsFitNarrowFullscreenWidths() {
        const auto compact = TransportBarMetrics::forWidth(560);
        QVERIFY(compact.showTime);
        QVERIFY(compact.showVolume);
        QVERIFY(compact.minimumRowWidth <= 560);

        const auto narrow = TransportBarMetrics::forWidth(400);
        QVERIFY(!narrow.showTime);
        QVERIFY(narrow.showVolume);
        QVERIFY(narrow.minimumRowWidth <= 400);
        QCOMPARE(narrow.buttonExtent, 32);
    }

    void transportBarHidesOptionalLabelsWhenNarrow() {
        QAction open(QStringLiteral("打开文件"));
        QAction previous(QStringLiteral("上一个"));
        QAction pause(QStringLiteral("播放"));
        QAction next(QStringLiteral("下一个"));
        QAction stop(QStringLiteral("停止"));
        QAction mute(QStringLiteral("静音"));
        QAction fullscreen(QStringLiteral("全屏"));
        QWidget host;
        TransportBar transport(&open, &previous, &pause, &next, &stop, &mute, &fullscreen, &host);
        auto *hostLayout = new QVBoxLayout(&host);
        hostLayout->setContentsMargins(0, 0, 0, 0);
        hostLayout->addWidget(&transport);
        auto *time = transport.findChild<QLabel *>(QStringLiteral("timeLabel"));
        auto *volume = transport.findChild<QSlider *>(QStringLiteral("volumeSlider"));
        const auto dividers = transport.findChildren<QFrame *>(QStringLiteral("transportDivider"));
        QVERIFY(time && volume);
        QCOMPARE(dividers.size(), 2);

        host.setFixedSize(400, 100);
        host.show();
        QCoreApplication::processEvents();
        QCOMPARE(transport.width(), 400);
        QVERIFY(time->isHidden());
        QVERIFY(dividers.at(0)->isHidden());
        QCOMPARE(volume->width(), 60);
        for (auto *button : transport.findChildren<QToolButton *>()) {
            QCOMPARE(button->width(), 32);
            QCOMPARE(button->height(), 32);
        }

        host.setFixedSize(560, 100);
        host.layout()->activate();
        transport.resize(560, 100);
        QCoreApplication::processEvents();
        QVERIFY(TransportBarMetrics::forWidth(560).showTime);
        QVERIFY(TransportBarMetrics::forWidth(560).showVolume);
    }
};

QTEST_MAIN(AppearancePreferencesTest)
#include "appearance_preferences_test.moc"
