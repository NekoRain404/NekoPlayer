#include "appearancepreferences.h"
#include "appearancestylesheet.h"
#include "appearancetokens.h"
#include "playback/backendinfo.h"

#include <QApplication>
#include <QMessageBox>
#include <QSettings>
#include <QStyle>
#include <QtGlobal>

AppearancePreferences::Settings AppearancePreferences::loadSettings(QSettings &settings) {
    Settings appearance;
    const auto theme = settings.value(QStringLiteral("appearance/theme"), QStringLiteral("dark"));
    if (theme.toString() == QLatin1String("light"))
        appearance.theme = Theme::Light;
    else if (theme.toString() == QLatin1String("system"))
        appearance.theme = Theme::System;
    appearance.accent =
        accentFromKey(settings.value(QStringLiteral("appearance/accent")).toString());
    appearance.glassEnabled =
        settings.value(QStringLiteral("appearance/glassEnabled"), true).toBool();
    return appearance;
}

void AppearancePreferences::saveSettings(QSettings &settings, const Settings &appearance) {
    const auto theme = appearance.theme == Theme::Light    ? QStringLiteral("light")
                       : appearance.theme == Theme::System ? QStringLiteral("system")
                                                           : QStringLiteral("dark");
    settings.setValue(QStringLiteral("appearance/theme"), theme);
    settings.setValue(QStringLiteral("appearance/accent"), accentKey(appearance.accent));
    settings.setValue(QStringLiteral("appearance/glassEnabled"), appearance.glassEnabled);
}

void AppearancePreferences::applySettings(QApplication &application, const Settings &appearance) {
    const auto tokens = appearanceTokens(appearance.theme, appearance.accent);
    if (appearance.theme == Theme::System) {
        auto palette = application.style()->standardPalette();
        // Keep the platform's disabled selection/link roles intact; only the
        // enabled groups should express the user's accent.
        for (const auto group : {QPalette::Active, QPalette::Inactive}) {
            palette.setColor(group, QPalette::Highlight, tokens.selection);
            palette.setColor(group, QPalette::HighlightedText, tokens.selectedText);
            palette.setColor(group, QPalette::Link, tokens.accent);
        }
        application.setPalette(palette);
    } else {
        application.setPalette(paletteForTokens(tokens));
    }
    application.setStyleSheet(appearanceStyleSheet(tokens, appearance.glassEnabled));
}

QList<AppearancePreferences::Accent> AppearancePreferences::accents() {
    return {Accent::Ocean, Accent::Emerald, Accent::Violet, Accent::Rose, Accent::Amber};
}

QString AppearancePreferences::accentName(Accent accent) {
    switch (accent) {
    case Accent::Ocean:
        return QObject::tr("Ocean");
    case Accent::Emerald:
        return QObject::tr("Emerald");
    case Accent::Violet:
        return QObject::tr("Violet");
    case Accent::Rose:
        return QObject::tr("Rose");
    case Accent::Amber:
        return QObject::tr("Amber");
    }
    return QObject::tr("Ocean");
}

QString AppearancePreferences::accentKey(Accent accent) {
    switch (accent) {
    case Accent::Ocean:
        return QStringLiteral("ocean");
    case Accent::Emerald:
        return QStringLiteral("emerald");
    case Accent::Violet:
        return QStringLiteral("violet");
    case Accent::Rose:
        return QStringLiteral("rose");
    case Accent::Amber:
        return QStringLiteral("amber");
    }
    return QStringLiteral("ocean");
}

AppearancePreferences::Accent AppearancePreferences::accentFromKey(const QString &key) {
    for (const auto accent : accents()) {
        if (accentKey(accent) == key) {
            return accent;
        }
    }
    return Accent::Ocean;
}

QList<AppearancePreferences::Theme> AppearancePreferences::themes() {
    return {Theme::Dark, Theme::Light, Theme::System};
}

QString AppearancePreferences::themeName(Theme theme) {
    switch (theme) {
    case Theme::Dark:
        return QObject::tr("深色");
    case Theme::Light:
        return QObject::tr("浅色");
    case Theme::System:
        return QObject::tr("跟随系统");
    }
    return QObject::tr("跟随系统");
}

void AppearancePreferences::applyTheme(QApplication &application, Theme theme) {
    if (theme == Theme::System) {
        application.setStyleSheet({});
        application.setPalette(application.style()->standardPalette());
        return;
    }

    const auto tokens = appearanceTokens(theme);
    application.setPalette(paletteForTokens(tokens));
    application.setStyleSheet(appearanceStyleSheet(tokens));
}

void AppearancePreferences::showAboutDialog(QWidget *parent) {
    const auto text = QObject::tr("NekoPlayer 0.1.0\n\nQt runtime: %1\nlibmpv client API: %2")
                          .arg(QString::fromLatin1(qVersion()))
                          .arg(BackendInfo::clientApiVersion());
    QMessageBox::about(parent, QObject::tr("关于 NekoPlayer"), text);
}
