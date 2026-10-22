#include "appearancepreferences.h"

#include <QApplication>
#include <QColor>
#include <QMessageBox>
#include <QPalette>
#include <QStyle>
#include <QtGlobal>
#include <mpv/client.h>

namespace {
QPalette darkPalette() {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(31, 34, 40));
    palette.setColor(QPalette::WindowText, QColor(232, 234, 238));
    palette.setColor(QPalette::Base, QColor(22, 24, 29));
    palette.setColor(QPalette::AlternateBase, QColor(38, 41, 48));
    palette.setColor(QPalette::ToolTipBase, QColor(38, 41, 48));
    palette.setColor(QPalette::ToolTipText, QColor(245, 246, 248));
    palette.setColor(QPalette::Text, QColor(232, 234, 238));
    palette.setColor(QPalette::Button, QColor(43, 47, 55));
    palette.setColor(QPalette::ButtonText, QColor(232, 234, 238));
    palette.setColor(QPalette::BrightText, QColor(255, 100, 100));
    palette.setColor(QPalette::Link, QColor(100, 170, 255));
    palette.setColor(QPalette::Highlight, QColor(48, 125, 220));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(130, 133, 140));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(130, 133, 140));
    return palette;
}

QPalette lightPalette() {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(245, 246, 248));
    palette.setColor(QPalette::WindowText, QColor(32, 35, 40));
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::AlternateBase, QColor(237, 239, 242));
    palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 220));
    palette.setColor(QPalette::ToolTipText, QColor(32, 35, 40));
    palette.setColor(QPalette::Text, QColor(32, 35, 40));
    palette.setColor(QPalette::Button, QColor(235, 237, 240));
    palette.setColor(QPalette::ButtonText, QColor(32, 35, 40));
    palette.setColor(QPalette::BrightText, QColor(190, 30, 30));
    palette.setColor(QPalette::Link, QColor(20, 95, 185));
    palette.setColor(QPalette::Highlight, QColor(48, 125, 220));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(145, 148, 153));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(145, 148, 153));
    return palette;
}

QString themeStyleSheet(AppearancePreferences::Theme theme) {
    if (theme == AppearancePreferences::Theme::Dark) {
        return QStringLiteral(
            "QMainWindow, QWidget#transportBar { background: #1f2228; color: #e8eaee; }"
            "QMenuBar, QMenu, QStatusBar { background: #262930; color: #e8eaee; }"
            "QMenuBar::item:selected, QMenu::item:selected { background: #414655; }"
            "QLabel { color: #bfc3cf; }"
            "QToolButton { color: #e8eaee; background: transparent; border: none; border-radius: "
            "4px; }"
            "QToolButton:hover { background: #3b3f4b; }"
            "QToolButton:disabled { color: #777b85; }"
            "QSlider::groove:horizontal { height: 4px; background: #454953; border-radius: 2px; }"
            "QSlider::sub-page:horizontal { background: #8ba9ef; border-radius: 2px; }"
            "QSlider::handle:horizontal { background: #dce5fa; width: 10px; margin: -4px 0; "
            "border-radius: 5px; }"
            "QMenu { background: #262930; color: #e8eaee; border: 1px solid #454a54; }"
            "QMenu::item:selected { background: #307ddc; }"
            "QToolTip { color: #f5f6f8; background: #262930; border: 1px solid #555b66; }");
    }
    return QStringLiteral(
        "QMainWindow, QWidget#transportBar { background: #f5f6f8; color: #202328; }"
        "QMenuBar, QMenu, QStatusBar { background: #eceef1; color: #202328; }"
        "QMenuBar::item:selected, QMenu::item:selected { background: #d8e4f5; }"
        "QLabel { color: #30343a; }"
        "QToolButton { color: #202328; background: transparent; border: none; border-radius: 4px; }"
        "QToolButton:hover { background: #dfe3e9; }"
        "QToolButton:disabled { color: #9a9da2; }"
        "QSlider::groove:horizontal { height: 4px; background: #c8ccd2; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: #307ddc; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #307ddc; width: 10px; margin: -4px 0; "
        "border-radius: 5px; }"
        "QMenu { background: #ffffff; color: #202328; border: 1px solid #c8ccd2; }"
        "QMenu::item:selected { color: #ffffff; background: #307ddc; }"
        "QToolTip { color: #202328; background: #ffffdc; border: 1px solid #c8ccd2; }");
}
} // namespace

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

    application.setPalette(theme == Theme::Dark ? darkPalette() : lightPalette());
    application.setStyleSheet(themeStyleSheet(theme));
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
