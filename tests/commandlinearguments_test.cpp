#include "cli/commandlinearguments.h"

#include <QtTest>

class CommandLineArgumentsTest final : public QObject {
    Q_OBJECT
  private slots:
    void findsOptionBeforePositionalSeparator() {
        char program[] = "NekoPlayer";
        char option[] = "--check-backend";
        char separator[] = "--";
        char positional[] = "--check-backend";
        char *arguments[]{program, option, separator, positional};

        QVERIFY(CommandLineArguments::hasOptionBeforeSeparator(4, arguments, option));
    }

    void ignoresOptionShapedPositionalPath() {
        char program[] = "NekoPlayer";
        char separator[] = "--";
        char positional[] = "--check-backend";
        char *arguments[]{program, separator, positional};

        QVERIFY(!CommandLineArguments::hasOptionBeforeSeparator(3, arguments, positional));
    }
};

QTEST_GUILESS_MAIN(CommandLineArgumentsTest)
#include "commandlinearguments_test.moc"
