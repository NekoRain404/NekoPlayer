#pragma once

#include "mediachapter.h"
#include <QList>
#include <mpv/client.h>

QList<MediaChapter> parseMpvChapters(const mpv_node &node);
