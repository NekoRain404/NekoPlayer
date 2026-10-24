#pragma once

#include "appearancepreferences.h"
#include <QObject>
#include <QSettings>

class QMenu;
class QWidget;

// Owns the current session appearance, including changes made with --no-config.
class AppearanceMenu final : public QObject {
  public:
    AppearanceMenu(QWidget *parent, QSettings &store, bool configurationEnabled);
    [[nodiscard]] QMenu *menu() const { return menu_; }
    [[nodiscard]] AppearancePreferences::Settings settings() const { return settings_; }
    void setSettings(const AppearancePreferences::Settings &appearance);

  private:
    QMenu *menu_;
    AppearancePreferences::Settings settings_;
    QString settingsFile_;
    QSettings::Format settingsFormat_;
    bool configurationEnabled_;
};
