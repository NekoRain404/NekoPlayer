#pragma once

#include <QString>

// Deterministic natural ("episode 2" before "episode 10") comparison using a
// pinned English collation. Never consults the environment or the user locale.
// Safe to call from any thread, including the GUI thread.
int naturalCompare(const QString &left, const QString &right);

// Strict weak ordering matching naturalCompare, with a raw-string tiebreak so
// callers such as folder scanning get a deterministic total order.
bool naturalLess(const QString &left, const QString &right);
