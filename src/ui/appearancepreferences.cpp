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
            "QWidget#playlistPane { background: #202329; border-left: 1px solid #343842; }"
            "QWidget#playlistHeader { border-bottom: 1px solid #343842; }"
            "QLabel#playlistTitle { color: #f4f7ff; font-size: 14px; font-weight: 600; }"
            "QLabel#playlistCount { color: #aeb5c4; background: #2d323c; border-radius: 8px; "
            "padding: 2px 8px; }"
            "QWidget#welcomePanel { background: #242830; border: 1px solid #3b414d; "
            "border-radius: 14px; }"
            "QLabel#welcomeBrand { color: #8ba9ef; font-size: 10px; font-weight: 700; "
            "letter-spacing: 1px; }"
            "QLabel#welcomeTitle { color: #f4f7ff; font-size: 26px; font-weight: 600; }"
            "QLabel#welcomeSubtitle { color: #c4c9d4; font-size: 14px; }"
            "QLabel#welcomeDropHint { color: #929aa9; font-size: 12px; }"
            "QLabel#welcomeRecentHeading { color: #aeb5c4; font-size: 12px; font-weight: 600; }"
            "QPushButton[welcomeRecentAction] { color: #e8eaee; text-align: left; "
            "background: #2b3039; border: 1px solid #3b414d; border-radius: 8px; "
            "padding: 8px 11px; }"
            "QPushButton[welcomeRecentAction]:hover { background: #343a45; "
            "border-color: #566174; }"
            "QPushButton#welcomeOpenButton, QPushButton#welcomeNetworkButton { min-height: 38px; "
            "border-radius: 10px; padding: 7px 14px; font-weight: 600; }"
            "QPushButton#welcomeOpenButton { color: #ffffff; background: #4b83df; "
            "border: 1px solid #6e9ce9; }"
            "QPushButton#welcomeOpenButton:hover { background: #5b91e8; }"
            "QPushButton#welcomeOpenButton:pressed { background: #3e71c5; }"
            "QPushButton#welcomeNetworkButton { color: #e8eaee; background: #30353f; "
            "border: 1px solid #4a5260; }"
            "QPushButton#welcomeNetworkButton:hover { background: #3a414d; }"
            "QPushButton#welcomeNetworkButton:pressed { background: #454d5b; }"
            "QMenuBar, QMenu, QStatusBar { background: #262930; color: #e8eaee; }"
            "QMenuBar::item { padding: 5px 9px; border-radius: 5px; }"
            "QMenuBar::item:selected, QMenu::item:selected { background: #414655; }"
            "QMenu::item { padding: 6px 28px 6px 10px; border-radius: 4px; }"
            "QLabel { color: #bfc3cf; }"
            "QToolButton { color: #e8eaee; background: transparent; border: 1px solid "
            "transparent; border-radius: 7px; padding: 4px; }"
            "QToolButton:hover { background: #343842; border-color: #414652; }"
            "QToolButton:pressed { background: #454b58; border-color: #5b6372; }"
            "QToolButton:checked { color: #a9c4ff; background: #303b51; }"
            "QToolButton:disabled { color: #777b85; background: transparent; }"
            "QToolButton:focus, QPushButton:focus, QComboBox:focus, QLineEdit:focus { "
            "border: 1px solid #8ba9ef; }"
            "QPushButton { border: 1px solid #454953; border-radius: 7px; padding: 6px 12px; "
            "color: #e8eaee; background: #2b2f37; }"
            "QPushButton:hover { background: #343842; }"
            "QPushButton:pressed { background: #454b58; }"
            "QPushButton:disabled { color: #777b85; background: #262930; border-color: #343842; }"
            "QSlider::groove:horizontal { height: 4px; background: #454953; border-radius: 2px; }"
            "QSlider::sub-page:horizontal { background: #8ba9ef; border-radius: 2px; }"
            "QSlider::handle:horizontal { background: #dce5fa; width: 10px; margin: -5px 0; "
            "border: 2px solid #1f2228; border-radius: 6px; }"
            "QSlider::handle:horizontal:hover { background: #ffffff; width: 12px; margin: -6px "
            "-1px; border-radius: 7px; }"
            "QSlider::handle:horizontal:pressed { background: #ffffff; width: 14px; margin: -6px "
            "-2px; border-radius: 7px; }"
            "QSlider::handle:horizontal:disabled { background: #777b85; border-color: #262930; }"
            "QSlider#timelineSlider::groove:horizontal { height: 5px; }"
            "QSlider#timelineSlider::handle:horizontal { width: 12px; }"
            "QSlider#timelineSlider::handle:horizontal:hover { width: 14px; }"
            "QSlider:focus { outline: 1px solid #8ba9ef; outline-offset: 3px; }"
            "QSlider:disabled { opacity: 0.55; }"
            "QListView#playlistView { background: transparent; border: none; outline: none; "
            "padding: 4px; }"
            "QListView#playlistView::item { min-height: 28px; padding: 4px 8px; border-radius: "
            "5px; }"
            "QListView#playlistView::item:hover { background: #2d313a; }"
            "QListView#playlistView::item:selected { color: #f4f7ff; background: #35445f; }"
            "QListView#playlistView::item:selected:!active { color: #dce5fa; background: #303641; }"
            "QListView#playlistView:focus { border: 1px solid #59647a; border-radius: 6px; }"
            "QFocusFrame { border: 1px solid #8ba9ef; }"
            "QMenu { background: #262930; color: #e8eaee; border: 1px solid #454a54; }"
            "QMenu::item:selected { background: #307ddc; }"
            "QMenu::item:disabled { color: #777b85; }"
            "QToolTip { color: #f5f6f8; background: #262930; border: 1px solid #555b66; }");
    }
    return QStringLiteral(
        "QMainWindow, QWidget#transportBar { background: #f5f6f8; color: #202328; }"
        "QWidget#playlistPane { background: #f0f1f4; border-left: 1px solid #dfe2e7; }"
        "QWidget#playlistHeader { border-bottom: 1px solid #dfe2e7; }"
        "QLabel#playlistTitle { color: #202328; font-size: 14px; font-weight: 600; }"
        "QLabel#playlistCount { color: #626872; background: #e5e8ed; border-radius: 8px; "
        "padding: 2px 8px; }"
        "QWidget#welcomePanel { background: #ffffff; border: 1px solid #dfe2e7; "
        "border-radius: 14px; }"
        "QLabel#welcomeBrand { color: #307ddc; font-size: 10px; font-weight: 700; "
        "letter-spacing: 1px; }"
        "QLabel#welcomeTitle { color: #202328; font-size: 26px; font-weight: 600; }"
        "QLabel#welcomeSubtitle { color: #555b64; font-size: 14px; }"
        "QLabel#welcomeDropHint { color: #858b95; font-size: 12px; }"
        "QLabel#welcomeRecentHeading { color: #626872; font-size: 12px; font-weight: 600; }"
        "QPushButton[welcomeRecentAction] { color: #30343a; text-align: left; "
        "background: #f4f5f7; border: 1px solid #e2e5e9; border-radius: 8px; "
        "padding: 8px 11px; }"
        "QPushButton[welcomeRecentAction]:hover { background: #e9edf2; "
        "border-color: #c7d0dc; }"
        "QPushButton#welcomeOpenButton, QPushButton#welcomeNetworkButton { min-height: 38px; "
        "border-radius: 10px; padding: 7px 14px; font-weight: 600; }"
        "QPushButton#welcomeOpenButton { color: #ffffff; background: #307ddc; "
        "border: 1px solid #5b98e6; }"
        "QPushButton#welcomeOpenButton:hover { background: #3e8be7; }"
        "QPushButton#welcomeOpenButton:pressed { background: #286bc0; }"
        "QPushButton#welcomeNetworkButton { color: #202328; background: #f4f5f7; "
        "border: 1px solid #c8ccd2; }"
        "QPushButton#welcomeNetworkButton:hover { background: #e9ebef; }"
        "QPushButton#welcomeNetworkButton:pressed { background: #dde1e7; }"
        "QMenuBar, QMenu, QStatusBar { background: #eceef1; color: #202328; }"
        "QMenuBar::item { padding: 5px 9px; border-radius: 5px; }"
        "QMenuBar::item:selected, QMenu::item:selected { background: #d8e4f5; }"
        "QMenu::item { padding: 6px 28px 6px 10px; border-radius: 4px; }"
        "QLabel { color: #30343a; }"
        "QToolButton { color: #202328; background: transparent; border: 1px solid transparent; "
        "border-radius: 7px; padding: 4px; }"
        "QToolButton:hover { background: #e6e9ee; border-color: #d9dde4; }"
        "QToolButton:pressed { background: #d7dce4; border-color: #c4cad4; }"
        "QToolButton:checked { color: #185abc; background: #dce8fb; }"
        "QToolButton:disabled { color: #9a9da2; background: transparent; }"
        "QToolButton:focus, QPushButton:focus, QComboBox:focus, QLineEdit:focus { border: 1px "
        "solid #307ddc; }"
        "QPushButton { border: 1px solid #c8ccd2; border-radius: 7px; padding: 6px 12px; "
        "color: #202328; background: #ffffff; }"
        "QPushButton:hover { background: #f0f2f5; }"
        "QPushButton:pressed { background: #dfe3e9; }"
        "QPushButton:disabled { color: #9a9da2; background: #eceef1; border-color: #dfe2e7; }"
        "QSlider::groove:horizontal { height: 4px; background: #c8ccd2; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: #307ddc; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #307ddc; width: 10px; margin: -5px 0; border: "
        "2px solid #f5f6f8; border-radius: 6px; }"
        "QSlider::handle:horizontal:hover { background: #185abc; width: 12px; margin: -6px -1px; "
        "border-radius: 7px; }"
        "QSlider::handle:horizontal:pressed { background: #124a9c; width: 14px; margin: -6px -2px; "
        "border-radius: 7px; }"
        "QSlider::handle:horizontal:disabled { background: #aeb3bc; border-color: #eceef1; }"
        "QSlider#timelineSlider::groove:horizontal { height: 5px; }"
        "QSlider#timelineSlider::handle:horizontal { width: 12px; }"
        "QSlider#timelineSlider::handle:horizontal:hover { width: 14px; }"
        "QSlider:focus { outline: 1px solid #307ddc; outline-offset: 3px; }"
        "QSlider:disabled { opacity: 0.55; }"
        "QListView#playlistView { background: transparent; border: none; outline: none; padding: "
        "4px; }"
        "QListView#playlistView::item { min-height: 28px; padding: 4px 8px; border-radius: 5px; }"
        "QListView#playlistView::item:hover { background: #e7eaf0; }"
        "QListView#playlistView::item:selected { color: #173b70; background: #d7e5fb; }"
        "QListView#playlistView::item:selected:!active { color: #30343a; background: #e1e4e9; }"
        "QListView#playlistView:focus { border: 1px solid #9db9e8; border-radius: 6px; }"
        "QFocusFrame { border: 1px solid #307ddc; }"
        "QMenu { background: #ffffff; color: #202328; border: 1px solid #c8ccd2; }"
        "QMenu::item:selected { color: #ffffff; background: #307ddc; }"
        "QMenu::item:disabled { color: #9a9da2; }"
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
