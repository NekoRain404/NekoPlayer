#pragma once

class QAction;
class QWidget;

namespace WindowsIntegration {
// Registers global media keys (play/pause, stop, previous, next) on Windows and
// returns how many were accepted. The actions keep their normal in-app
// shortcuts; this only adds system-wide handling. No-op off Windows.
int installMediaHotkeys(QWidget &window, QAction *playPause, QAction *stop, QAction *previous,
                        QAction *next);

// Releases hotkeys and the native event filter. Safe to call repeatedly.
void releaseMediaHotkeys();

// Keeps the display and system awake while media is actively playing.
// No-op off Windows.
void setPlaybackScreenAwake(bool active);
} // namespace WindowsIntegration
