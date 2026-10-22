#pragma once

#include <QList>
#include <QString>

class QApplication;
class QWidget;

class AppearancePreferences final {
  public:
    enum class Theme { Dark, Light, System };

    [[nodiscard]] static QList<Theme> themes();
    [[nodiscard]] static QString themeName(Theme theme);
    static void applyTheme(QApplication &application, Theme theme);
    static void showAboutDialog(QWidget *parent = nullptr);
};
