#pragma once

#include <QCollatorSortKey>
#include <QString>

// Deterministic natural ("episode 2" before "episode 10") comparison using a
// pinned English collation. Never consults the environment or the user locale.
// Safe to call from any thread, including the GUI thread.
int naturalCompare(const QString &left, const QString &right);

// Strict weak ordering matching naturalCompare, with a raw-string tiebreak so
// callers such as folder scanning get a deterministic total order.
bool naturalLess(const QString &left, const QString &right);

// Precomputes the collation key for text with the same pinned English collator
// that naturalCompare uses. Comparing two keys has the same ordering as
// naturalCompare, but pays the collation cost once per string instead of once
// per comparison. Keys are only comparable with keys created on the same thread.
QCollatorSortKey naturalSortKey(const QString &text);
