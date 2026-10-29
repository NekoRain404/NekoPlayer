#pragma once

#include <QString>

// Window caption for a media source path. Empty paths use the application name.
[[nodiscard]] QString windowTitleForPath(const QString &path);
