#include "mpvcontroller_p.h"

namespace {
// Mirror LanguagePreferences::normalized() so playback stays independent of the
// session layer while both layers accept the same user input.
QStringList normalizedLanguages(const QStringList &languages) {
    constexpr qsizetype maximumLanguages = 16;
    QStringList result;
    for (const auto &language : languages) {
        const auto trimmed = language.trimmed();
        if (trimmed.isEmpty())
            continue;
        if (result.size() >= maximumLanguages)
            break;
        result.append(trimmed);
    }
    return result;
}

QByteArray mpvLanguageList(const QStringList &languages) {
    return languages.join(QLatin1Char(',')).toUtf8();
}
} // namespace

void MpvController::setLanguagePreferences(const QStringList &audio, const QStringList &subtitle) {
    const auto audioLanguages = normalizedLanguages(audio);
    const auto subtitleLanguages = normalizedLanguages(subtitle);
    // Commands run asynchronously in submission order, so applying these before a
    // load lets mpv autoselect while explicit aid/sid selections still win later.
    impl_->command({"set", "alang", mpvLanguageList(audioLanguages)});
    impl_->command({"set", "slang", mpvLanguageList(subtitleLanguages)});
}

void MpvController::setAutoLoadExternalSubtitles(bool enabled) {
    // Exact matches only: movie.mkv pairs with movie.srt, not unrelated siblings.
    impl_->command({"set", "sub-auto", enabled ? "exact" : "no"});
}
