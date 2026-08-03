#pragma once
#include <QColor>
#include <QString>

// 관제실 다크 테마 토큰 (디자인 시안과 1:1)
namespace Theme {
inline const QColor Bg          = QColor("#0b0e11");
inline const QColor Panel       = QColor("#0e1216");
inline const QColor PanelHead   = QColor("#12181d");
inline const QColor BarBg       = QColor("#0e1317");
inline const QColor MapBg       = QColor("#080b0e");
inline const QColor Border      = QColor("#212a32");
inline const QColor BorderSoft  = QColor("#1c242b");
inline const QColor BorderRow   = QColor("#161d23");
inline const QColor Text        = QColor("#e4e9ee");
inline const QColor Text2       = QColor("#dbe2e8");
inline const QColor Text3       = QColor("#c7d1da");
inline const QColor TextDim     = QColor("#8e9aa5");
inline const QColor TextDim2    = QColor("#9aa6b1");
inline const QColor TextFaint   = QColor("#5f6c78");
inline const QColor TextFaint2  = QColor("#4a555f");
inline const QColor TextGhost   = QColor("#39434c");
inline const QColor Accent      = QColor("#3fbfcc");
inline const QColor AccentBright= QColor("#8fd9e2");
inline const QColor Ok          = QColor("#4bbd85");
inline const QColor OkBright    = QColor("#6fdcab");
inline const QColor Warn        = QColor("#e2a33c");
inline const QColor Danger      = QColor("#e0574a");
inline const QColor DangerText  = QColor("#ff8175");
inline const QColor Wall        = QColor("#46596a");

inline QString mono(int px, int weight = 400) {
    return QString("font-family:'JetBrains Mono','D2Coding',monospace;font-size:%1px;font-weight:%2;")
        .arg(px).arg(weight);
}

inline QString appStyleSheet() {
    return R"(
QWidget            { background:#0b0e11; color:#e4e9ee;
                     font-family:'Helvetica Neue',Helvetica,Arial,'Malgun Gothic',sans-serif; font-size:12px; }
QFrame#panel       { background:#0e1216; border:1px solid #212a32; border-radius:5px; }
QFrame#panelHead   { background:#12181d; border:none; border-bottom:1px solid #1e262d; }
QFrame#chip        { background:#0d1216; border:1px solid #222c34; border-radius:4px; }
QFrame#card        { background:#101519; border:1px solid #1c242b; border-radius:4px; }
QLabel#mono        { font-family:'JetBrains Mono','D2Coding',monospace; font-size:11px; color:#8e9aa5; }
QLabel#label       { font-family:'JetBrains Mono','D2Coding',monospace; font-size:10px;
                     letter-spacing:1px; color:#5f6c78; }

QPushButton        { background:#161b20; border:1px solid #2a343d; border-radius:4px; color:#9aa6b1;
                     font-family:'JetBrains Mono','D2Coding',monospace; font-size:11px;
                     letter-spacing:1px; padding:0 13px; min-height:30px; }
QPushButton:hover  { background:#1d242a; color:#d5dde4; }
QPushButton#accent { background:#152229; border-color:#2c4750; color:#8fd9e2; }
QPushButton#accent:hover { background:#1b2d36; color:#b6ecf3; }
QPushButton#powerOn  { background:#16241d; border-color:#2f5a45; color:#6fdcab; font-weight:700; }
QPushButton#powerOff { background:#20161a; border-color:#5a2f34; color:#ff9a92; font-weight:700; }

QTabWidget::pane   { border:none; background:#0b0e11; }
QTabBar            { background:#0e1317; }
QTabBar::tab       { background:transparent; color:#6d7983; padding:10px 16px;
                     border-bottom:2px solid transparent;
                     font-family:'JetBrains Mono','D2Coding',monospace; font-size:11px; letter-spacing:1px; }
QTabBar::tab:selected { background:#151c22; color:#e4e9ee; border-bottom:2px solid #3fbfcc; }

QTableWidget       { background:#0e1216; gridline-color:#161d23; border:none;
                     font-family:'JetBrains Mono','D2Coding',monospace; font-size:11px; color:#9aa6b1; }
QHeaderView::section { background:#12181d; color:#5f6c78; border:none;
                     border-bottom:1px solid #1e262d; padding:6px 8px;
                     font-family:'JetBrains Mono','D2Coding',monospace; font-size:10px; letter-spacing:1px; }
QPlainTextEdit     { background:#0e1216; border:none; color:#9aa6b1;
                     font-family:'JetBrains Mono','D2Coding',monospace; font-size:11px; }
QScrollBar:vertical   { background:transparent; width:8px; }
QScrollBar::handle:vertical { background:#2a333c; border-radius:4px; }
QScrollBar::add-line, QScrollBar::sub-line { height:0; width:0; }
QMenuBar           { background:#0e1317; color:#9aa6b1; border-bottom:1px solid #212a32;
                     font-family:'JetBrains Mono','D2Coding',monospace; font-size:11px; }
QMenuBar::item:selected { background:#1d242a; }
QMenu              { background:#12181d; border:1px solid #212a32; color:#c7d1da; }
QMenu::item:selected { background:#1d242a; }
)";
}
} // namespace Theme
