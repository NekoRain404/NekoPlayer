#include "ui/actionfeedbackgeometry.h"
#include "ui/actionfeedbacktext.h"

#include <QtTest>

class ActionFeedbackTest final : public QObject {
    Q_OBJECT
  private slots:
    void seekFeedbackDescribesDirectionAndMagnitude() {
        QCOMPARE(ActionFeedbackText::seek(5), QStringLiteral("前进 5 秒"));
        QCOMPARE(ActionFeedbackText::seek(-5), QStringLiteral("后退 5 秒"));
        QCOMPARE(ActionFeedbackText::seek(0), QString());
    }

    void absoluteSeekFeedbackUsesPlaybackTimeFormat() {
        QCOMPARE(ActionFeedbackText::seekTo(3661.9), QStringLiteral("跳转至 01:01:01"));
        QCOMPARE(ActionFeedbackText::seekTo(-1), QStringLiteral("跳转至 00:00:00"));
    }

    void volumeFeedbackClampsToSupportedRange() {
        QCOMPARE(ActionFeedbackText::volume(45), QStringLiteral("音量 45%"));
        QCOMPARE(ActionFeedbackText::volume(-5), QStringLiteral("音量 0%"));
        QCOMPARE(ActionFeedbackText::volume(105), QStringLiteral("音量 100%"));
    }

    void toggleFeedbackDescribesTheRequestedState() {
        QCOMPARE(ActionFeedbackText::mute(true), QStringLiteral("已静音"));
        QCOMPARE(ActionFeedbackText::mute(false), QStringLiteral("取消静音"));
        QCOMPARE(ActionFeedbackText::playback(true), QStringLiteral("已暂停"));
        QCOMPARE(ActionFeedbackText::playback(false), QStringLiteral("继续播放"));
    }

    void feedbackPanelStaysInsideTinyViewport() {
        const auto layout = ActionFeedbackGeometry::arrange(QSize(20, 12), QSize(180, 48));
        const QRect viewport(QPoint(0, 0), QSize(20, 12));
        QVERIFY(viewport.contains(layout.panel));
        QVERIFY(viewport.contains(layout.text));
    }

    void feedbackPanelUsesCenteredPositionAtNormalSize() {
        const auto layout = ActionFeedbackGeometry::arrange(QSize(1920, 1080), QSize(240, 48));
        QCOMPARE(layout.panel, QRect(822, 700, 276, 68));
        QCOMPARE(layout.text, QRect(840, 710, 240, 48));
    }
};

QTEST_GUILESS_MAIN(ActionFeedbackTest)
#include "action_feedback_test.moc"
