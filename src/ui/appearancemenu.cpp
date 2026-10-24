#include "appearancemenu.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QMenu>

AppearanceMenu::AppearanceMenu(QWidget *parent, QSettings &store, bool configurationEnabled)
    : QObject(parent), menu_(new QMenu(parent)),
      settings_(configurationEnabled ? AppearancePreferences::loadSettings(store)
                                     : AppearancePreferences::Settings{}),
      settingsFile_(store.fileName()), settingsFormat_(store.format()),
      configurationEnabled_(configurationEnabled) {
    auto *group = new QActionGroup(menu_);
    group->setExclusive(true);
    for (const auto theme : AppearancePreferences::themes()) {
        auto *action = menu_->addAction(AppearancePreferences::themeName(theme));
        const auto name = theme == AppearancePreferences::Theme::Dark    ? QStringLiteral("Dark")
                          : theme == AppearancePreferences::Theme::Light ? QStringLiteral("Light")
                                                                         : QStringLiteral("System");
        action->setObjectName(QStringLiteral("theme%1Action").arg(name));
        action->setData(static_cast<int>(theme));
        action->setCheckable(true);
        action->setChecked(settings_.theme == theme);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, theme] {
            auto appearance = settings_;
            appearance.theme = theme;
            setSettings(appearance);
        });
    }
}

void AppearanceMenu::setSettings(const AppearancePreferences::Settings &appearance) {
    settings_ = appearance;
    AppearancePreferences::applySettings(*qApp, settings_);
    for (auto *action : menu_->actions())
        action->setChecked(action->data().toInt() == static_cast<int>(appearance.theme));
    if (configurationEnabled_) {
        QSettings store(settingsFile_, settingsFormat_);
        AppearancePreferences::saveSettings(store, settings_);
    }
}
