#pragma once

#include <QList>
#include <QString>

class QApplication;
class QSettings;
class QWidget;

class AppearancePreferences final {
  public:
    enum class Theme { Dark, Light, System };
    enum class Accent { Ocean, Emerald, Violet, Rose, Amber };

    struct Settings {
        Theme theme = Theme::Dark;
        Accent accent = Accent::Ocean;
        bool glassEnabled = true;
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
