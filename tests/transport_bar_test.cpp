#include "playback/mpvcontroller.h"
#include "ui/appearancestylesheet.h"
#include "ui/appearancetokens.h"
#include "ui/transportbar.h"
#include "ui/transportbarmetrics.h"

#include <QAction>
#include <QApplication>
#include <QFrame>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtTest>

class TransportBarTest final : public QObject {
    Q_OBJECT
  private slots:
    void controlsAreKeyboardAndScreenReaderAccessible() {
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

    void timeLabelTogglesElapsedAndRemaining() {
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
        host.resize(560, 100);
        host.show();
        QCoreApplication::processEvents();
        PlaybackState state;
        state.active = true;
        state.seekable = true;
        state.position = 75;
        state.duration = 120;
        transport.present(state);

        auto *time = transport.findChild<QLabel *>(QStringLiteral("timeLabel"));
        QVERIFY(time);
        QCOMPARE(time->text(), QStringLiteral("00:01:15 / 00:02:00"));
        QVERIFY(time->toolTip().contains(time->text()));
        QVERIFY(time->accessibleDescription().contains(time->text()));
        QVERIFY(time->toolTip().contains(QStringLiteral("切换")));
        QVERIFY(time->accessibleDescription().contains(QStringLiteral("切换")));
        QVERIFY(time->toolTip().contains(QStringLiteral("00:00:45")));
        QVERIFY(time->accessibleDescription().contains(QStringLiteral("00:00:45")));
        QVERIFY(time->accessibleDescription().contains(QStringLiteral("回车键")));

        const QPoint center = time->rect().center();
        QMouseEvent press(QEvent::MouseButtonPress, center, time->mapToGlobal(center),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(time, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, center, time->mapToGlobal(center),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(time, &release);

        QCOMPARE(time->text(), QStringLiteral("-00:00:45 / 00:02:00"));
        QVERIFY(time->toolTip().contains(time->text()));
        QVERIFY(time->accessibleDescription().contains(time->text()));
        QVERIFY(time->accessibleDescription().contains(QStringLiteral("已播放")));
        QVERIFY(time->toolTip().contains(QStringLiteral("00:01:15")));
        QVERIFY(time->accessibleDescription().contains(QStringLiteral("00:01:15")));

        state.position = 90;
        transport.present(state);
        QCOMPARE(time->text(), QStringLiteral("-00:00:30 / 00:02:00"));
        QVERIFY(time->toolTip().contains(time->text()));
    }

    void timeLabelSupportsKeyboardActivation() {
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
        host.setFocusPolicy(Qt::StrongFocus);
        pause.setShortcut(QKeySequence(Qt::Key_Space));
        host.addAction(&pause);
        int pauseTriggers = 0;
        connect(&pause, &QAction::triggered, &host, [&pauseTriggers] { ++pauseTriggers; });
        host.resize(560, 100);
        host.show();
        host.activateWindow();
        QCoreApplication::processEvents();
        PlaybackState state;
        state.active = true;
        state.seekable = true;
        state.position = 90;
        state.duration = 120;
        transport.present(state);

        auto *time = transport.findChild<QLabel *>(QStringLiteral("timeLabel"));
        QVERIFY(time);
        QCOMPARE(time->accessibleName(), QStringLiteral("播放时间"));
        QVERIFY(time->focusPolicy() != Qt::NoFocus);
        const auto unfocused = time->grab().toImage();
        time->setFocus(Qt::OtherFocusReason);
        QCoreApplication::processEvents();
        QVERIFY(time->hasFocus());
        QVERIFY(time->grab().toImage() != unfocused);

        QKeyEvent overrideEvent(QEvent::ShortcutOverride, Qt::Key_Space, Qt::NoModifier);
        QCoreApplication::sendEvent(time, &overrideEvent);
        QVERIFY(overrideEvent.isAccepted());

        QTest::keyClick(time, Qt::Key_Space);
        QCoreApplication::processEvents();
        QCOMPARE(time->text(), QStringLiteral("-00:00:30 / 00:02:00"));
        QCOMPARE(pauseTriggers, 0);

        QTest::keyClick(time, Qt::Key_Return);
        QCOMPARE(time->text(), QStringLiteral("00:01:30 / 00:02:00"));

        QTest::keyClick(time, Qt::Key_Enter);
        QCOMPARE(time->text(), QStringLiteral("-00:00:30 / 00:02:00"));

        QKeyEvent repeat(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QString(), true);
        QCoreApplication::sendEvent(time, &repeat);
        QCOMPARE(time->text(), QStringLiteral("-00:00:30 / 00:02:00"));

        host.setFocus(Qt::OtherFocusReason);
        QCoreApplication::processEvents();
        QCOMPARE(QApplication::focusWidget(), &host);
        QTest::keyClick(&host, Qt::Key_Space);
        QCoreApplication::processEvents();
        QCOMPARE(pauseTriggers, 1);
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

    void timelineWheelReportsDirectionWithoutMovingTheSlider() {
        QAction open(QStringLiteral("打开文件"));
        QAction previous(QStringLiteral("上一个"));
        QAction pause(QStringLiteral("播放"));
        QAction next(QStringLiteral("下一个"));
        QAction stop(QStringLiteral("停止"));
        QAction mute(QStringLiteral("静音"));
        QAction fullscreen(QStringLiteral("全屏"));
        QWidget host;
        TransportBar transport(&open, &previous, &pause, &next, &stop, &mute, &fullscreen, &host);
        PlaybackState state;
        state.active = true;
        state.seekable = true;
        state.position = 50;
        state.duration = 100;
        transport.present(state);

        auto *timeline = transport.findChild<QSlider *>(QStringLiteral("timelineSlider"));
        QVERIFY(timeline);
        QSignalSpy seekSpy(&transport, &TransportBar::wheelSeekRequested);
        const auto sendWheel = [timeline](int deltaY) {
            const QPointF localPoint(timeline->width() / 2.0, timeline->height() / 2.0);
            QWheelEvent wheel(localPoint, timeline->mapToGlobal(localPoint), QPoint(),
                              QPoint(0, deltaY), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase,
                              false);
            QCoreApplication::sendEvent(timeline, &wheel);
        };

        const int before = timeline->value();
        sendWheel(120);
        sendWheel(-120);
        sendWheel(30);
        QCOMPARE(seekSpy.count(), 3);
        QCOMPARE(seekSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(seekSpy.at(1).at(0).toInt(), -1);
        QCOMPARE(seekSpy.at(2).at(0).toInt(), 1);
        QCOMPARE(timeline->value(), before);
    }

    void metricsFitNarrowFullscreenWidths() {
        const auto narrow = TransportBarMetrics::forWidth(400);
        QVERIFY(!narrow.showTime);
        QVERIFY(narrow.showVolume);
        QCOMPARE(narrow.buttonExtent, 32);
        QCOMPARE(narrow.volumeWidth, 60);
        QVERIFY(narrow.minimumRowWidth <= 400);

        for (const int width : {480, 559}) {
            const auto hidden = TransportBarMetrics::forWidth(width);
            QVERIFY(!hidden.showTime);
            QVERIFY(hidden.showVolume);
            QCOMPARE(hidden.buttonExtent, 36);
            QCOMPARE(hidden.volumeWidth, 76);
            QCOMPARE(hidden.minimumRowWidth, 418);
            QVERIFY(hidden.minimumRowWidth <= width);
        }

        for (const int width : {560, 639}) {
            const auto compact = TransportBarMetrics::forWidth(width);
            QVERIFY(compact.showTime);
            QVERIFY(compact.showVolume);
            QCOMPARE(compact.buttonExtent, 32);
            QCOMPARE(compact.volumeWidth, 76);
            QCOMPARE(compact.timeWidth, 155);
            QCOMPARE(compact.minimumRowWidth, 545);
            QVERIFY(compact.minimumRowWidth <= width);
        }

        const auto full = TransportBarMetrics::forWidth(640);
        QVERIFY(full.showTime);
        QVERIFY(full.showVolume);
        QCOMPARE(full.buttonExtent, 36);
        QCOMPARE(full.volumeWidth, 110);
        QCOMPARE(full.timeWidth, 155);
        QCOMPARE(full.minimumRowWidth, 607);
        QVERIFY(full.minimumRowWidth <= 640);
    }

    void optionalLabelsHideWhenNarrow() {
        const auto previousStyleSheet = qApp->styleSheet();
        const auto restoreStyleSheet =
            qScopeGuard([previousStyleSheet] { qApp->setStyleSheet(previousStyleSheet); });
        qApp->setStyleSheet(
            appearanceStyleSheet(appearanceTokens(AppearancePreferences::Theme::Dark)));
        QCoreApplication::processEvents();

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

        host.setFixedSize(480, 100);
        host.layout()->activate();
        transport.resize(480, 100);
        QCoreApplication::processEvents();
        QCOMPARE(transport.width(), 480);
        QVERIFY(time->isHidden());
        QVERIFY(dividers.at(0)->isHidden());
        QCOMPARE(volume->width(), 76);
        for (auto *button : transport.findChildren<QToolButton *>()) {
            QCOMPARE(button->width(), 36);
            QCOMPARE(button->height(), 32);
        }

        host.setFixedSize(559, 100);
        host.layout()->activate();
        transport.resize(559, 100);
        QCoreApplication::processEvents();
        QCOMPARE(transport.width(), 559);
        QVERIFY(time->isHidden());
        QVERIFY(dividers.at(0)->isHidden());
        QCOMPARE(volume->width(), 76);
        for (auto *button : transport.findChildren<QToolButton *>()) {
            QCOMPARE(button->width(), 36);
            QCOMPARE(button->height(), 32);
        }

        host.setFixedSize(560, 100);
        host.layout()->activate();
        transport.resize(560, 100);
        QCoreApplication::processEvents();
        QVERIFY(!time->isHidden());
        QVERIFY(!dividers.at(0)->isHidden());
        QCOMPARE(time->minimumWidth(), 155);
        QCOMPARE(time->maximumWidth(), 155);
        QCOMPARE(volume->width(), 76);
        for (auto *button : transport.findChildren<QToolButton *>()) {
            QCOMPARE(button->width(), 32);
            QCOMPARE(button->height(), 32);
        }

        PlaybackState state;
        state.active = true;
        state.seekable = true;
        state.position = 75;
        state.duration = 120;
        transport.present(state);
        QCOMPARE(time->text(), QStringLiteral("00:01:15 / 00:02:00"));
        QVERIFY(time->fontMetrics().horizontalAdvance(time->text()) <=
                time->contentsRect().width());

        const QPoint center = time->rect().center();
        QMouseEvent press(QEvent::MouseButtonPress, center, time->mapToGlobal(center),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(time, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, center, time->mapToGlobal(center),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(time, &release);
        QCOMPARE(time->text(), QStringLiteral("-00:00:45 / 00:02:00"));
        QVERIFY(time->fontMetrics().horizontalAdvance(time->text()) <=
                time->contentsRect().width());
    }
};

QTEST_MAIN(TransportBarTest)
#include "transport_bar_test.moc"
