#pragma once

#include <QList>
#include <QString>

class QApplication;
class QSettings;
class QWidget;

inline constexpr int glassOpacityMinimum = 40;
inline constexpr int glassOpacityMaximum = 100;
inline constexpr int glassOpacityDefault = 88;
inline constexpr int glassBlurMinimum = 0;
inline constexpr int glassBlurMaximum = 100;
inline constexpr int glassBlurDefault = 38;

[[nodiscard]] constexpr int normalizedGlassOpacity(int value) {
    return value < glassOpacityMinimum   ? glassOpacityMinimum
           : value > glassOpacityMaximum ? glassOpacityMaximum
                                         : value;
}

[[nodiscard]] constexpr int normalizedGlassBlur(int value) {
    return value < glassBlurMinimum   ? glassBlurMinimum
           : value > glassBlurMaximum ? glassBlurMaximum
                                      : value;
}

class AppearancePreferences final {
  public:
    enum class Theme { Dark, Light, System };
    enum class Accent { Ocean, Emerald, Violet, Rose, Amber };

    struct Settings {
        Theme theme = Theme::Dark;
        Accent accent = Accent::Ocean;
        bool glassEnabled = true;
        int glassOpacity = glassOpacityDefault;
        int glassBlur = glassBlurDefault;

        [[nodiscard]] Settings normalized() const {
            return {theme, accent, glassEnabled, normalizedGlassOpacity(glassOpacity),
                    normalizedGlassBlur(glassBlur)};
        }
        bool operator==(const Settings &) const = default;
    };

    [[nodiscard]] static Settings loadSettings(QSettings &settings);
    static void saveSettings(QSettings &settings, const Settings &appearance);
    static void applySettings(QApplication &application, const Settings &appearance);

    [[nodiscard]] static QList<Accent> accents();
    [[nodiscard]] static QString accentName(Accent accent);
    [[nodiscard]] static QString accentKey(Accent accent);
    [[nodiscard]] static Accent accentFromKey(const QString &key);
    [[nodiscard]] static QList<Theme> themes();
    [[nodiscard]] static QString themeName(Theme theme);
    static void applyTheme(QApplication &application, Theme theme);
    static void showAboutDialog(QWidget *parent = nullptr);
};
