#include "naturalcompare.h"

#include <QCollator>
#include <QLocale>

namespace {
const QCollator &englishCollator() {
    // Some Qt backends ignore numeric mode under the C locale, so English plus
    // numeric/case settings are explicit. One collator per thread keeps collation
    // setup out of the comparison loop while staying safe off the GUI thread.
    thread_local const QCollator collator = [] {
        QCollator value{QLocale(QLocale::English)};
        value.setNumericMode(true);
        value.setCaseSensitivity(Qt::CaseInsensitive);
        return value;
    }();
    return collator;
}
} // namespace

int naturalCompare(const QString &left, const QString &right) {
    return englishCollator().compare(left, right);
}

bool naturalLess(const QString &left, const QString &right) {
    const auto compared = naturalCompare(left, right);
    return compared != 0 ? compared < 0 : left < right;
}
