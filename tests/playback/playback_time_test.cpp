#include "playback/media/playbacktime.h"

#include <QtTest>
#include <limits>

class PlaybackTimeTest final : public QObject {
    Q_OBJECT
  private slots:
    void validTimes_data() {
        QTest::addColumn<QString>("text");
        QTest::addColumn<double>("seconds");
        QTest::newRow("zero") << QStringLiteral("0") << 0.0;
        QTest::newRow("seconds") << QStringLiteral("125.123") << 125.123;
        QTest::newRow("minutes") << QStringLiteral("90:02.5") << 5402.5;
        QTest::newRow("hours") << QStringLiteral("01:02:03.004") << 3723.004;
        QTest::newRow("long") << QStringLiteral("123:59:59.999") << 446399.999;
        QTest::newRow("trimmed") << QStringLiteral(" 2:03 ") << 123.0;
    }
    void validTimes() {
        QFETCH(QString, text);
        QFETCH(double, seconds);
        const auto parsed = parsePlaybackTime(text);
        QVERIFY(parsed.has_value());
        QCOMPARE(*parsed, seconds);
    }
    void rejectsInvalidTimes() {
        for (const auto *text : {"",        " ",
                                 "-1",      "+1",
                                 "nan",     "inf",
                                 "1e3",     "1,2",
                                 "1:60",    "1:60:00",
                                 "1:2:60",  "1::2",
                                 ":12",     "12:",
                                 "1:2:3:4", "1.2345",
                                 "1.2:03",  "1.000:02",
                                 "1 :02",   "1.",
                                 ".5",      "9999999999999999999999999999999999999"})
            QVERIFY2(!parsePlaybackTime(QString::fromLatin1(text)).has_value(), text);
    }
    void displaysLongHoursAndMilliseconds() {
        QCOMPARE(formatPlaybackTime(0), QStringLiteral("00:00:00"));
        QCOMPARE(formatPlaybackTime(3723.004, true), QStringLiteral("01:02:03.004"));
        QCOMPARE(formatPlaybackTime(446399.999, true), QStringLiteral("123:59:59.999"));
        QCOMPARE(formatPlaybackTime(59.9996, true), QStringLiteral("00:01:00.000"));
        QCOMPARE(formatPlaybackTime(59.9996), QStringLiteral("00:00:59"));
        QCOMPARE(formatPlaybackTime(-1), QStringLiteral("--:--:--"));
        QCOMPARE(formatPlaybackTime(std::numeric_limits<double>::infinity()),
                 QStringLiteral("--:--:--"));
    }
};

QTEST_GUILESS_MAIN(PlaybackTimeTest)
#include "playback_time_test.moc"
