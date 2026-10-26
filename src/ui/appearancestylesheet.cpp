#include "appearancestylesheet.h"

void initAppearanceResources() {
    static const bool initialized = [] {
        Q_INIT_RESOURCE(appearance_resources);
        return true;
    }();
    Q_UNUSED(initialized)
}

namespace {
QString color(const QColor &value) { return value.name(QColor::HexRgb); }

QString translucent(const QColor &value, int alpha) {
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(value.red())
        .arg(value.green())
        .arg(value.blue())
        .arg(alpha);
}

QString panelFill(const QColor &value, bool glassEnabled, int opacity, int blur) {
    if (!glassEnabled) {
        return color(value);
    }
    const int baseAlpha = qRound(normalizedGlassOpacity(opacity) * 2.55);
    // More blur means softer, lower-contrast rims; the fill remains a layered
    // gradient because Qt Widgets cannot sample the content behind a window.
    const int rimBoost = qRound((glassBlurMaximum - normalizedGlassBlur(blur)) * 0.04);
    const int edgeAlpha = qMin(255, baseAlpha + rimBoost);
    const int centerAlpha = qMax(0, baseAlpha - qRound(normalizedGlassBlur(blur) * 0.06));
    const int lighten = 106 + normalizedGlassBlur(blur) / 16;
    const int darken = 104 - normalizedGlassBlur(blur) / 16;
    return QStringLiteral("qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %1,stop:0.35 %2,stop:0.78 "
                          "%2,stop:1 %3)")
        .arg(translucent(value.lighter(lighten), edgeAlpha), translucent(value, centerAlpha),
             translucent(value.darker(darken), edgeAlpha));
}

QString windowBackdrop(const AppearanceTokens &tokens, bool glassEnabled) {
    // The main window keeps a translucent surface so compositors that support
    // background blur can show the desktop through the glass chrome. The video
    // widget remains opaque; when glass is off, the canvas is opaque as before.
    return glassEnabled ? QStringLiteral("transparent") : color(tokens.canvas);
}
} // namespace

QString appearanceStyleSheet(const AppearanceTokens &t, bool glassEnabled, int glassOpacity,
                             int glassBlur) {
    initAppearanceResources();
    const auto unit = t.spaceUnit;
    auto style =
        QStringLiteral(
            "QMainWindow{background:%28;color:%2;}"
            "QWidget#transportBar{background:%27;color:%2;border-top:1px solid %3;}"
            "QFrame#transportDivider{background:%3;color:%3;}"
            "QWidget#playlistPane{background:%27;border-left:1px solid %3;}"
            "QWidget#playlistHeader{background:%27;border-bottom:1px solid %3;}"
            "QLineEdit#playlistSearchEdit{color:%2;background:%27;border:1px solid "
            "%3;border-radius:%13px;padding:%8px %14px;min-height:%29px;}"
            "QLineEdit#playlistSearchEdit:focus{border:1px solid %18;}"
            "QWidget#welcomePanel{background:transparent;border:none;border-radius:%6px;}"
            "QLabel{color:%7;} "
            "QLabel#timeLabel{color:%7;font-family:monospace;font-size:12px;padding:0 %8px;}"
            "QLabel#playlistTitle,QLabel#welcomeTitle{color:%2;font-weight:600;}"
            "QLabel#playlistCount{color:%7;background:%27;border-radius:%9px;padding:%10px "
            "%8px;}"
            "QLabel#welcomeBrand{color:%11;font-weight:700;} QLabel#welcomeSubtitle{color:%7;}"
            "QLabel#welcomeDropHint{color:%12;} QLabel#welcomeRecentHeading{color:%7;}"
            "QLabel#welcomeRecentChevron{color:%12;} "
            "QLabel[welcomeRecentTitle]{color:%2;font-weight:600;}"
            "QLabel[welcomeRecentLocation]{color:%12;} "
            "QLabel#welcomeContinueHeading{color:%7;font-weight:600;} "
            "QProgressBar#welcomeContinueProgress1,QProgressBar#welcomeContinueProgress2,"
            "QProgressBar#welcomeContinueProgress3{color:%7;background:%27;border:0;"
            "border-radius:%9px;text-align:center;} "
            "QProgressBar::chunk{background:%11;border-radius:%9px;} "
            "QLabel[welcomeMediaIcon]{background:%27;border-radius:%6px;}"
            "QPushButton{color:%2;background:%27;border:1px solid "
            "%3;border-radius:%13px;padding:%8px %14px;}"
            "QPushButton:hover{background:%15;} QPushButton:pressed{background:%16;}"
            "QPushButton:disabled{color:%17;background:%4;border-color:%3;}"
            "QPushButton:focus{border:1px solid %18;}"
            "QPushButton#welcomeOpenButton{color:%19;background:%11;border:1px solid %20;}"
            "QPushButton#welcomeOpenButton:hover{background:%21;} "
            "QPushButton#welcomeOpenButton:pressed{background:%22;}"
            "QPushButton#welcomeNetworkButton{background:%27;}"
            "QPushButton[welcomeRecentAction]{text-align:left;background:%27;border:1px solid "
            "%3;border-radius:%6px;}"
            "QPushButton[welcomeRecentAction]:hover{background:%15;} "
            "QPushButton[welcomeRecentAction]:pressed{background:%16;}"
            "QPushButton[welcomeRecentAction]:focus{border:1px solid %18;}"
            "QPushButton[welcomeContinueAction]{text-align:left;background:%27;border:1px solid "
            "%3;border-radius:%6px;}"
            "QPushButton[welcomeContinueAction]:hover{background:%15;} "
            "QPushButton[welcomeContinueAction]:pressed{background:%16;}"
            "QMenuBar{background:%27;color:%2;border-bottom:1px solid %3;}"
            "QStatusBar{background:%27;color:%2;border-top:1px solid %3;border-bottom:none;}"
            "QStatusBar QLabel{background:transparent;color:%7;}"
            "QStatusBar::item{border:none;}"
            "QMenu{background:%32;color:%2;border:1px solid %3;border-radius:%13px;padding:6px;}"
            "QDialog{background:%32;color:%2;}"
            "QMenuBar{padding:2px 6px;} QMenuBar::item{padding:6px 10px;border-radius:%13px;}"
            "QMenuBar::item:selected,QMenu::item:selected{background:%24;}"
            "QMenu::item{padding:7px %25px 7px 12px;margin:1px 4px;border-radius:%13px;"
            "min-height:%31px;}"
            "QMenu::item:disabled{color:%17;}"
            "QToolButton{color:%2;background:transparent;border:1px solid "
            "transparent;border-radius:%13px;padding:%8px;min-height:%29px;}"
            "QToolButton:hover{background:%15;border-color:%3;} "
            "QToolButton:pressed{background:%16;border-color:%3;}"
            "QToolButton:checked{color:%11;background:%24;} QToolButton:disabled{color:%17;}"
            "QToolButton:focus,QComboBox:focus,QLineEdit:focus{border:1px solid %18;}"
            "QToolButton#transportPrimaryButton{background:%24;border:1px solid "
            "%18;border-radius:%6px;}"
            "QToolButton#transportPrimaryButton:hover{background:%21;} "
            "QToolButton#transportPrimaryButton:pressed{background:%22;}"
            "QToolButton#transportPrimaryButton:checked{color:%19;background:%11;border-color:%"
            "20;}"
            "QSlider::groove:horizontal{height:4px;background:%3;border-radius:%13px;}"
            "QSlider::sub-page:horizontal{background:%11;border-radius:%13px;}"
            "QSlider::handle:horizontal{background:%11;width:10px;margin:-5px 0;border:2px "
            "solid %1;border-radius:%13px;}"
            "QSlider::handle:horizontal:hover{background:%21;width:12px;}"
            "QSlider::handle:horizontal:pressed{background:%22;width:14px;}"
            "QSlider::handle:horizontal:disabled{background:%17;border-color:%4;}"
            "QSlider#timelineSlider::groove:horizontal{height:5px;} "
            "QSlider#timelineSlider::handle:horizontal{width:12px;}"
            "QSlider#timelineSlider::handle:horizontal:hover{width:14px;}"
            "QSlider#volumeSlider::groove:horizontal{height:4px;} "
            "QSlider#volumeSlider::sub-page:horizontal{background:%11;}"
            "QSlider#volumeSlider::handle:horizontal{background:%11;width:10px;margin:-5px "
            "0;border:2px solid %1;border-radius:%13px;}"
            "QSlider#volumeSlider::handle:horizontal:hover{background:%21;width:12px;}"
            "QSlider::handle:horizontal:focus{background:%5;border:2px solid %18;width:14px;"
            "margin:-7px 0;} "
            "QSlider:disabled{opacity:0.55;}"
            "QListView#playlistView{background:transparent;border:none;outline:none;padding:%"
            "8px;}"
            "QListView#playlistView::item{min-height:%30px;padding:%8px "
            "%14px;border-radius:%13px;}"
            "QListView#playlistView::item:hover{background:%15;}"
            "QListView#playlistView::item:selected{color:%26;background:%24;}"
            "QListView#playlistView::item:selected:!active{color:%7;background:%4;}"
            "QListView#playlistView:focus{border:1px solid %18;border-radius:%13px;}"
            "QFocusFrame{border:1px solid %18;} "
            "QComboBox,QFontComboBox,QLineEdit,QSpinBox,QDoubleSpinBox{background:%27;color:%2;"
            "border:1px solid %3;border-radius:%13px;padding:%23px %14px;"
            "selection-background-color:%11;selection-color:%19;}"
            "QComboBox:hover,QFontComboBox:hover,QLineEdit:hover,QSpinBox:hover,QDoubleSpinBox:"
            "hover{border-color:%7;}"
            "QComboBox:disabled,QFontComboBox:disabled,QLineEdit:disabled,QSpinBox:disabled,"
            "QDoubleSpinBox:disabled{color:%17;background:%4;border-color:%3;}"
            "QComboBox:focus,QFontComboBox:focus,QLineEdit:focus,QSpinBox:focus,QDoubleSpinBox:"
            "focus{border:1px solid %18;}"
            "QComboBox::drop-down{border:none;width:24px;}"
            "QComboBox::down-arrow{image:url(:/appearance/data/"
            "chevron.png);width:10px;height:10px;}"
            "QSpinBox,QDoubleSpinBox{padding-right:24px;}"
            "QSpinBox::up-button,QDoubleSpinBox::up-button{image:url(:/appearance/data/"
            "chevron-up.png);"
            "subcontrol-origin:border;subcontrol-position:top right;width:20px;height:50%;"
            "background:transparent;border:none;}"
            "QSpinBox::down-button,QDoubleSpinBox::down-button{image:url(:/appearance/data/"
            "chevron-down.png);"
            "subcontrol-origin:border;subcontrol-position:bottom right;width:20px;height:50%;"
            "background:transparent;border:none;}"
            "QComboBox QAbstractItemView{background:%32;color:%2;border:1px solid %3;"
            "border-radius:%6px;padding:%23px;selection-background-color:%24;"
            "selection-color:%26;outline:none;}"
            "QTabWidget::pane{border:1px solid %3;border-radius:%6px;background:%27;"
            "top:-1px;}"
            "QTabBar::tab{background:transparent;color:%7;padding:%23px %14px;margin:2px;"
            "min-height:%29px;"
            "border:none;border-radius:%13px;}"
            "QTabBar::tab:selected{background:%27;color:%2;font-weight:600;border:1px solid %3;}"
            "QTabBar::tab:hover:!selected{background:%15;}"
            "QScrollBar:vertical{background:transparent;width:10px;margin:2px;}"
            "QScrollBar::handle:vertical{background:%3;min-height:32px;border-radius:5px;}"
            "QScrollBar::handle:vertical:hover{background:%7;}"
            "QScrollBar:horizontal{background:transparent;height:10px;margin:2px;}"
            "QScrollBar::handle:horizontal{background:%3;min-width:32px;border-radius:5px;}"
            "QScrollBar::handle:horizontal:hover{background:%7;}"
            "QScrollBar::add-line,QScrollBar::sub-line{width:0;height:0;}"
            "QScrollBar::add-page,QScrollBar::sub-page{background:transparent;}"
            "QMenu::separator{height:1px;background:%3;margin:6px 8px;}"
            "QCheckBox{spacing:%23px;}"
            "QCheckBox::indicator{width:16px;height:16px;border:1px solid %3;"
            "border-radius:5px;background:%5;}"
            "QCheckBox:focus::indicator{border:2px solid %18;}"
            "QCheckBox::indicator:hover{border-color:%7;}"
            "QCheckBox::indicator:checked{background:%11;border-color:%11;image:url(:/"
            "appearance/data/checkmark.png);}"
            "QCheckBox::indicator:checked:hover{background:%21;border-color:%21;}"
            "QCheckBox::indicator:disabled{background:%4;border-color:%3;}"
            "QCheckBox:disabled{color:%17;}"
            "QCheckBox::indicator:checked:disabled{background:%4;border-color:%3;image:url(:/"
            "appearance/data/checkmark.png);}"
            "QDialogButtonBox QPushButton{min-width:76px;}"
            "QStatusBar{font-size:11px;}"
            "QLabel#appearanceGlassOpacityValue,QLabel#appearanceGlassBlurValue{color:%7;}"
            "QToolTip{color:%2;background:%32;border:1px solid %3;border-radius:%13px;"
            "padding:%23px %14px;}")
            .arg(color(t.canvas), color(t.primaryText), color(t.border), color(t.raisedSurface),
                 color(t.surface), QString::number(t.surfaceRadius), color(t.secondaryText),
                 QString::number(unit), QString::number(t.controlRadius), QString::number(unit / 2),
                 color(t.accent), color(t.mutedText), QString::number(t.controlRadius),
                 QString::number(unit * 3), color(t.hoverSurface), color(t.pressedSurface),
                 color(t.disabledText), color(t.focus), color(t.accentText), color(t.accentHover),
                 color(t.accentHover), color(t.accentPressed), QString::number(unit + 1),
                 color(t.selection), QString::number(unit * 7), color(t.selectedText),
                 panelFill(t.surface, glassEnabled, glassOpacity, glassBlur),
                 windowBackdrop(t, glassEnabled), QString::number(t.controlHeight),
                 QString::number(t.rowHeight), QString::number(t.menuItemHeight),
                 panelFill(t.raisedSurface, glassEnabled, glassOpacity, glassBlur));
    style +=
        QStringLiteral(
            "QLabel#playbackInformationOverlay{color:%1;background:%2;border-radius:6px;"
            "padding:12px;font-family:monospace;font-size:14px;}"
            "QFrame#seekProgressOverlay{background:%3;border-radius:6px;}"
            "QFrame#seekProgressOverlay QLabel{color:%1;background:transparent;font-size:16px;}"
            "QProgressBar#seekProgressBar{background:%4;border:none;border-radius:4px;}"
            "QProgressBar#seekProgressBar::chunk{background:%5;border-radius:4px;}")
            .arg(color(t.primaryText),
                 panelFill(t.raisedSurface, glassEnabled, glassOpacity, glassBlur),
                 panelFill(t.raisedSurface, glassEnabled, glassOpacity, glassBlur),
                 color(t.separator), color(t.accent));
    return style;
}
