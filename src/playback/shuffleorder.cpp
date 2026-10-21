#include "shuffleorder.h"

#include <QAbstractItemModel>
#include <QRandomGenerator>

namespace {
constexpr int maximumHistory = 1024;
}

void ShuffleOrder::reset(const QAbstractItemModel &model, int currentRow) {
    history_.clear();
    cursor_ = -1;
    const auto current = model.index(currentRow, 0);
    if (current.isValid()) {
        history_.append(QPersistentModelIndex(current));
        cursor_ = 0;
    }
    refill(model, currentRow);
}

void ShuffleOrder::select(const QAbstractItemModel &model, int row) {
    if (cursor_ < 0 || cursor_ >= history_.size() || history_[cursor_] != model.index(row, 0))
        reset(model, row);
}

void ShuffleOrder::refill(const QAbstractItemModel &model, int excludedRow) {
    pending_.clear();
    pending_.reserve(model.rowCount());
    for (int row = 0; row < model.rowCount(); ++row) {
        if (row != excludedRow)
            pending_.append(QPersistentModelIndex(model.index(row, 0)));
    }
}

void ShuffleOrder::append(const QAbstractItemModel &model, int first, int last) {
    for (int row = first; row <= last; ++row)
        pending_.append(QPersistentModelIndex(model.index(row, 0)));
}

void ShuffleOrder::prune() {
    pending_.removeIf([](const auto &entry) { return !entry.isValid(); });
    // At most 1024 history entries; edits are cold paths, navigation stays O(1).
    for (int i = int(history_.size()) - 1; i >= 0; --i) {
        if (!history_[i].isValid()) {
            history_.removeAt(i);
            if (i <= cursor_)
                --cursor_;
        }
    }
}

bool ShuffleOrder::hasNext() const { return cursor_ + 1 < history_.size() || !pending_.isEmpty(); }
bool ShuffleOrder::hasPrevious() const { return cursor_ > 0; }
int ShuffleOrder::scanBudget() const { return int(history_.size() + pending_.size()); }

QPersistentModelIndex ShuffleOrder::next(const QAbstractItemModel &model, bool wrap) {
    if (cursor_ + 1 < history_.size())
        return history_[++cursor_];
    int avoidFirst = -1;
    if (pending_.isEmpty() && wrap) {
        const auto currentRow =
            cursor_ >= 0 && cursor_ < history_.size() ? history_[cursor_].row() : -1;
        refill(model, -1);
        if (model.rowCount() > 1)
            avoidFirst = currentRow;
    }
    if (pending_.isEmpty())
        return {};
    auto chosen =
        QRandomGenerator::global()->bounded(int(pending_.size()) - (avoidFirst >= 0 ? 1 : 0));
    if (avoidFirst >= 0 && chosen >= avoidFirst)
        ++chosen;
    const auto entry = pending_[chosen];
    pending_[chosen] = pending_.last();
    pending_.removeLast();
    history_.append(entry);
    if (history_.size() > maximumHistory)
        history_.removeFirst();
    cursor_ = int(history_.size()) - 1;
    return entry;
}

QPersistentModelIndex ShuffleOrder::previous() {
    if (!hasPrevious())
        return {};
    return history_[--cursor_];
}

void ShuffleOrder::rejectCurrent(int direction) {
    if (cursor_ < 0 || cursor_ >= history_.size())
        return;
    history_.removeAt(cursor_);
    // The scan immediately continues in this direction. A backward scan keeps
    // the erased slot as its boundary so previous() visits its predecessor.
    if (direction > 0)
        --cursor_;
}

void ShuffleOrder::rewindHistory() { cursor_ = -1; }
