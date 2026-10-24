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
    Volume,
    Mute,
    Remove,
    MoveUp,
    MoveDown,
    Information,
    MediaFile
};

QIcon icon(Symbol symbol, const QPalette &palette);
} // namespace AppearanceIcons
