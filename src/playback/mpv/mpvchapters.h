#pragma once

#include "playback/media/mediachapter.h"
#include <QList>
#include <mpv/client.h>

QList<MediaChapter> parseMpvChapters(const mpv_node &node);
