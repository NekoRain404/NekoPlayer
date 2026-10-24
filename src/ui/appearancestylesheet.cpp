#include "appearancestylesheet.h"

namespace {
QString color(const QColor &value) { return value.name(QColor::HexRgb); }

QString translucent(const QColor &value, int alpha) {
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(value.red())
        .arg(value.green())
        .arg(value.blue())
        .arg(alpha);
}

QString panelFill(const QColor &value, bool glassEnabled) {
    if (!glassEnabled) {
        return color(value);
    }
    // A shaded lower rim supplies inexpensive depth without an offscreen graphics effect.
    return QStringLiteral("qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %1,stop:0.96 %2,stop:1 %3)")
        .arg(translucent(value.lighter(108), 238), translucent(value, 218),
             translucent(value.darker(108), 238));
}
} // namespace

QString appearanceStyleSheet(const AppearanceTokens &t, bool glassEnabled) {
    const auto unit = t.spaceUnit;
    return QStringLiteral(
               "QMainWindow{background:%1;color:%2;}"
               "QWidget#transportBar{background:%27;color:%2;border-top:1px solid %3;}"
               "QFrame#transportDivider{background:%3;color:%3;}"
               "QWidget#playlistPane{background:%28;border-left:1px solid %3;}"
               "QWidget#playlistHeader{background:%29;border-bottom:1px solid %3;}"
               "QWidget#welcomePanel{background:%29;border:1px solid %3;border-radius:%6px;}"
               "QLabel{color:%7;} "
               "QLabel#timeLabel{color:%7;font-family:monospace;font-size:12px;padding:0 %8px;}"
               "QLabel#playlistTitle,QLabel#welcomeTitle{color:%2;font-weight:600;}"
               "QLabel#playlistCount{color:%7;background:%4;border-radius:%9px;padding:%10px %8px;}"
               "QLabel#welcomeBrand{color:%11;font-weight:700;} QLabel#welcomeSubtitle{color:%7;}"
               "QLabel#welcomeDropHint{color:%12;} QLabel#welcomeRecentHeading{color:%7;}"
               "QLabel#welcomeRecentChevron{color:%12;} "
               "QLabel[welcomeRecentTitle]{color:%2;font-weight:600;}"
               "QLabel[welcomeRecentLocation]{color:%12;} "
               "QLabel#welcomeContinueHeading{color:%7;font-weight:600;} "
               "QProgressBar#welcomeContinueProgress1,QProgressBar#welcomeContinueProgress2,"
               "QProgressBar#welcomeContinueProgress3{color:%7;background:%4;border:0;"
               "border-radius:%9px;text-align:center;} "
               "QProgressBar::chunk{background:%11;border-radius:%9px;} "
               "QLabel[welcomeMediaIcon]{background:%4;border-radius:%6px;}"
               "QPushButton{color:%2;background:%5;border:1px solid "
               "%3;border-radius:%13px;padding:%8px %14px;}"
               "QPushButton:hover{background:%15;} QPushButton:pressed{background:%16;}"
               "QPushButton:disabled{color:%17;background:%4;border-color:%3;}"
               "QPushButton:focus{border:1px solid %18;}"
               "QPushButton#welcomeOpenButton{color:%19;background:%11;border:1px solid %20;}"
               "QPushButton#welcomeOpenButton:hover{background:%21;} "
               "QPushButton#welcomeOpenButton:pressed{background:%22;}"
               "QPushButton#welcomeNetworkButton{background:%5;}"
               "QPushButton[welcomeRecentAction]{text-align:left;background:%29;border:1px solid "
               "%3;border-radius:%6px;}"
               "QPushButton[welcomeRecentAction]:hover{background:%15;} "
               "QPushButton[welcomeRecentAction]:pressed{background:%16;}"
               "QPushButton[welcomeRecentAction]:focus{border:1px solid %18;}"
               "QPushButton[welcomeContinueAction]{text-align:left;background:%29;border:1px solid "
               "%3;border-radius:%6px;}"
               "QPushButton[welcomeContinueAction]:hover{background:%15;} "
               "QPushButton[welcomeContinueAction]:pressed{background:%16;}"
               "QMenuBar,QStatusBar{background:%4;color:%2;}"
               "QMenu{background:%28;color:%2;border:1px solid %3;border-radius:%13px;}"
               "QDialog{background:%28;color:%2;}"
               "QMenuBar::item{padding:%8px %23px;border-radius:%13px;}"
               "QMenuBar::item:selected,QMenu::item:selected{background:%24;}"
               "QMenu::item{padding:%23px %25px %23px %14px;border-radius:%13px;}"
               "QMenu::item:disabled{color:%17;}"
               "QToolButton{color:%2;background:transparent;border:1px solid "
               "transparent;border-radius:%13px;padding:%8px;}"
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
               "QSlider:focus{outline:1px solid %18;outline-offset:3px;} "
               "QSlider:disabled{opacity:0.55;}"
               "QListView#playlistView{background:transparent;border:none;outline:none;padding:%"
               "8px;}"
               "QListView#playlistView::item{min-height:28px;padding:%8px "
               "%14px;border-radius:%13px;}"
               "QListView#playlistView::item:hover{background:%15;}"
               "QListView#playlistView::item:selected{color:%26;background:%24;}"
               "QListView#playlistView::item:selected:!active{color:%7;background:%4;}"
               "QListView#playlistView:focus{border:1px solid %18;border-radius:%13px;}"
               "QFocusFrame{border:1px solid %18;} QToolTip{color:%2;background:%4;border:1px "
               "solid %3;}")
        .arg(color(t.canvas), color(t.primaryText), color(t.separator), color(t.raisedSurface),
             color(t.surface), QString::number(t.surfaceRadius), color(t.secondaryText),
             QString::number(unit), QString::number(t.controlRadius), QString::number(unit / 2),
             color(t.accent), color(t.mutedText), QString::number(t.controlRadius),
             QString::number(unit * 3), color(t.hoverSurface), color(t.pressedSurface),
             color(t.disabledText), color(t.focus), color(t.accentText), color(t.accentHover),
             color(t.accentHover), color(t.accentPressed), QString::number(unit + 1),
             color(t.selection), QString::number(unit * 7), color(t.selectedText),
             panelFill(t.canvas, glassEnabled), panelFill(t.raisedSurface, glassEnabled),
             panelFill(t.surface, glassEnabled));
}
