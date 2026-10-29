#include "session/sessionstore.h"
#include "ui/window/mainwindow.h"

#include <QAction>
#include <QTemporaryDir>
#include <QtTest>

namespace {
QAction *action(MainWindow &window, const char *name) {
    return window.findChild<QAction *>(QString::fromLatin1(name));
}
} // namespace

class AlwaysOnTopWindowTest final : public QObject {
    Q_OBJECT
  private slots:
    void actionTracksWindowFlag() {
        MainWindow window(true);
        auto *top = action(window, "alwaysOnTopAction");
        QVERIFY(top);
        QVERIFY(top->isCheckable());
        QVERIFY(!top->isChecked());
        QVERIFY(top->shortcut().isEmpty());
        QVERIFY(!window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));

        top->trigger();
        QVERIFY(top->isChecked());
        QCOMPARE(top->text(), QStringLiteral("窗口置顶"));
        QVERIFY(window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));

        top->trigger();
        QVERIFY(!top->isChecked());
        QVERIFY(!window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
    }

    void miniPlayerRestoresUserChoice() {
        MainWindow window(true);
        auto *top = action(window, "alwaysOnTopAction");
        auto *mini = action(window, "miniPlayerAction");
        QVERIFY(top && mini);

        top->trigger();
        QVERIFY(top->isChecked());
        mini->trigger();
        QVERIFY(window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        mini->trigger();
        QVERIFY(top->isChecked());
        QVERIFY(window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));

        top->trigger();
        QVERIFY(!top->isChecked());
        mini->trigger();
        QVERIFY(window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        mini->trigger();
        QVERIFY(!window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
    }

    void restoresAndPersistsChoice() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("always-on-top.ini"));
        SessionState saved;
        saved.window.alwaysOnTop = true;
        QVERIFY(SessionStore(path).save(saved).isEmpty());
        {
            MainWindow window(true, path);
            auto *top = action(window, "alwaysOnTopAction");
            QVERIFY(top);
            QVERIFY(top->isChecked());
            QVERIFY(window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        }
        QVERIFY(SessionStore(path).load().state.window.alwaysOnTop);
        {
            MainWindow window(true, path);
            auto *top = action(window, "alwaysOnTopAction");
            QVERIFY(top);
            QVERIFY(top->isChecked());
            top->trigger();
            QVERIFY(!top->isChecked());
        }
        QVERIFY(!SessionStore(path).load().state.window.alwaysOnTop);
    }

    void miniPlayerSaveKeepsUserChoice() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("always-on-top.ini"));
        SessionState saved;
        saved.window.alwaysOnTop = true;
        QVERIFY(SessionStore(path).save(saved).isEmpty());
        {
            MainWindow window(true, path);
            auto *mini = action(window, "miniPlayerAction");
            QVERIFY(mini);
            mini->trigger();
        }
        QVERIFY(SessionStore(path).load().state.window.alwaysOnTop);
    }
};

QTEST_MAIN(AlwaysOnTopWindowTest)
#include "always_on_top_window_test.moc"
