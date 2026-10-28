#pragma once

#include <QString>

// Session, history and track-preference files can contain signed media URLs.
// Keep them private to the owning user even when the process umask is
// permissive.
void restrictConfigurationFilePermissions(const QString &filePath);
