#include "appearancepreferences.h"
#include "appearancestylesheet.h"
#include "appearancetokens.h"

#include <QApplication>
#include <QMessageBox>
#include <QStyle>
#include <QtGlobal>
#include <mpv/client.h>

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
    const auto apiVersion = mpv_client_api_version();
    const auto apiMajor = static_cast<qulonglong>(apiVersion >> 16U);
    const auto apiMinor = static_cast<qulonglong>(apiVersion & 0xffffU);
    const auto text = QObject::tr("NekoPlayer 0.1.0\n\nQt runtime: %1\nlibmpv client API: %2.%3")
                          .arg(QString::fromLatin1(qVersion()))
                          .arg(apiMajor)
                          .arg(apiMinor);
    QMessageBox::about(parent, QObject::tr("关于 NekoPlayer"), text);
}
