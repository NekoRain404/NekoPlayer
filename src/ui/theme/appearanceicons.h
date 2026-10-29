#pragma once

#include <QIcon>
#include <QPalette>

namespace AppearanceIcons {
enum class Symbol {
    Open,
    Network,
    Playlist,
    Save,
    Add,
    Stop,
    Previous,
    Play,
    Pause,
    Next,
    Fullscreen,
    Restore,
    Seek,
    SeekBackward,
    SeekForward,
    Volume,
    Mute,
    Remove,
    MoveUp,
    MoveDown,
    Information,
    MediaFile,
    Settings,
    Trash,
    Close,
    MiniPlayer
};

QIcon icon(Symbol symbol, const QPalette &palette);
} // namespace AppearanceIcons
