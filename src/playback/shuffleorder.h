#pragma once

#include <QList>
#include <QPersistentModelIndex>

class QAbstractItemModel;

// Session navigation only. Persistent indexes distinguish duplicate paths and
// survive moves; never use their mutable row-based hash as a container key.
class ShuffleOrder final {
  public:
    void reset(const QAbstractItemModel &model, int currentRow);
    void select(const QAbstractItemModel &model, int row);
    void append(const QAbstractItemModel &model, int first, int last);
    void prune();
    [[nodiscard]] bool hasNext() const;
    [[nodiscard]] bool hasPrevious() const;
    [[nodiscard]] int scanBudget() const;
    QPersistentModelIndex next(const QAbstractItemModel &model, bool wrap);
    QPersistentModelIndex previous();
    void rejectCurrent(int direction);
    void rewindHistory();

  private:
    void refill(const QAbstractItemModel &model, int excludedRow);
    QList<QPersistentModelIndex> pending_;
    QList<QPersistentModelIndex> history_;
    int cursor_ = -1;
};
