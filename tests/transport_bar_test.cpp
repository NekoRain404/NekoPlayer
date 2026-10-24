#include "playback/mpvcontroller.h"
#include "ui/transportbar.h"
#include "ui/transportbarmetrics.h"

#include <QAction>
#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
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

    void metricsFitNarrowFullscreenWidths() {
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

    void optionalLabelsHideWhenNarrow() {
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

QTEST_MAIN(TransportBarTest)
#include "transport_bar_test.moc"
