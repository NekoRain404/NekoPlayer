#include "desktop/windowsintegration.h"

#include <QAction>
#include <QCoreApplication>
#include <QHash>
#include <QWidget>

#include <array>
#include <utility>

#ifdef Q_OS_WIN
#include <QAbstractNativeEventFilter>

// windows.h must stay first; it provides the Win32 types used below.
// clang-format off
#include <windows.h>
// clang-format on
#endif

namespace {
#ifdef Q_OS_WIN
constexpr int hotkeyBase = 0x4e4b; // 'NK'
constexpr int hotkeyCount = 4;

class HotkeyFilter final : public QAbstractNativeEventFilter {
  public:
    QHash<int, QAction *> actions;

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override {
        Q_UNUSED(eventType)
        Q_UNUSED(result)
        auto *msg = static_cast<MSG *>(message);
        if (msg->message != WM_HOTKEY)
            return false;
        const auto found = actions.constFind(static_cast<int>(msg->wParam));
        if (found == actions.cend())
            return false;
        if (auto *action = found.value(); action != nullptr)
            action->trigger();
        return true;
    }
};

HotkeyFilter *filter = nullptr;
QWidget *hotkeyWindow = nullptr;
bool registered[hotkeyCount] = {};
DWORD previousExecutionState = 0;
#endif
} // namespace

int WindowsIntegration::installMediaHotkeys(QWidget &window, QAction *playPause, QAction *stop,
                                            QAction *previous, QAction *next) {
#ifdef Q_OS_WIN
    releaseMediaHotkeys();
    hotkeyWindow = new QWidget(&window);
    hotkeyWindow->setObjectName(QStringLiteral("globalMediaHotkeyWindow"));
    hotkeyWindow->setAttribute(Qt::WA_DontShowOnScreen);
    const auto handle = reinterpret_cast<HWND>(hotkeyWindow->winId());
    if (handle == nullptr)
        return 0;

    filter = new HotkeyFilter;
    qApp->installNativeEventFilter(filter);
    const std::array<std::pair<UINT, QAction *>, hotkeyCount> entries{{
        {VK_MEDIA_PLAY_PAUSE, playPause},
        {VK_MEDIA_STOP, stop},
        {VK_MEDIA_PREV_TRACK, previous},
        {VK_MEDIA_NEXT_TRACK, next},
    }};
    int count = 0;
    for (int index = 0; index < hotkeyCount; ++index) {
        const auto id = hotkeyBase + index;
        const auto virtualKey = entries[static_cast<size_t>(index)].first;
        if (!RegisterHotKey(handle, id, MOD_NOREPEAT, virtualKey) &&
            !RegisterHotKey(handle, id, 0, virtualKey))
            continue;
        registered[index] = true;
        filter->actions.insert(id, entries[static_cast<size_t>(index)].second);
        ++count;
    }
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, qApp, [] { releaseMediaHotkeys(); });
    return count;
#else
    Q_UNUSED(window)
    Q_UNUSED(playPause)
    Q_UNUSED(stop)
    Q_UNUSED(previous)
    Q_UNUSED(next)
    return 0;
#endif
}

void WindowsIntegration::releaseMediaHotkeys() {
#ifdef Q_OS_WIN
    if (hotkeyWindow != nullptr) {
        const auto handle = reinterpret_cast<HWND>(hotkeyWindow->winId());
        for (int index = 0; index < hotkeyCount; ++index) {
            if (registered[index])
                UnregisterHotKey(handle, hotkeyBase + index);
            registered[index] = false;
        }
    }
    if (filter != nullptr) {
        if (auto *application = QCoreApplication::instance())
            application->removeNativeEventFilter(filter);
        delete filter;
        filter = nullptr;
    }
    delete hotkeyWindow;
    hotkeyWindow = nullptr;
#endif
}

void WindowsIntegration::setPlaybackScreenAwake(bool active) {
#ifdef Q_OS_WIN
    if (active) {
        if (previousExecutionState == 0)
            previousExecutionState =
                SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
        return;
    }
    if (previousExecutionState != 0) {
        SetThreadExecutionState(ES_CONTINUOUS);
        previousExecutionState = 0;
    }
#else
    Q_UNUSED(active)
#endif
}
