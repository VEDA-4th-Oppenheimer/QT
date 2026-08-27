#pragma once
#include <QColor>
#include <QString>

// 관제실 테마 토큰 — 두 모드:
//   Developer     : 다크 테마 + 한화비전 브랜드 오렌지 액센트
//   User(기본)    : 라이트 테마(같은 화면 구성, 배경/텍스트/액센트만 교체)
// SETTINGS 탭에서 [블랙]/[화이트] 로 고른다. 시작값은 화이트(User)다.
// 색을 쓰는 쪽(Theme::Bg 등)은 전부 함수가 아니라 값처럼 그대로 읽으면 된다 —
// setMode() 가 아래 mutable 값들을 통째로 갈아끼운다. 다만 이미 setStyleSheet()
// 로 "구워진" 문자열은 재적용되지 않으므로, 모드를 바꾼 뒤에는 위젯을 다시
// 만들어야 한다(MainWindow::applyThemeMode 참고).
namespace Theme {

enum class Mode { Developer, User };

struct Palette {
    QColor Bg, Panel, PanelHead, BarBg, MapBg;
    QColor Border, BorderSoft, BorderRow;
    QColor Text, Text2, Text3, TextDim, TextDim2, TextFaint, TextFaint2, TextGhost, TextMuted;
    QColor Accent, AccentBright, AccentBg;
    QColor Ok, OkBright, OkBg;
    QColor Warn, WarnBg;
    QColor Danger, DangerText, DangerBg, DangerBorder, DangerTextDim;
    QColor NeutralBg, NeutralFg;
    QColor Wall, Grid, ScanHighlight;
};

// Developer — 기존 다크 UI 그대로, Accent 계열만 한화비전 브랜드 오렌지로.
inline const Palette kDeveloper = {
    /*Bg*/QColor("#0b0e11"), /*Panel*/QColor("#0e1216"), /*PanelHead*/QColor("#12181d"),
    /*BarBg*/QColor("#0e1317"), /*MapBg*/QColor("#080b0e"),
    /*Border*/QColor("#212a32"), /*BorderSoft*/QColor("#1c242b"), /*BorderRow*/QColor("#161d23"),
    /*Text*/QColor("#e4e9ee"), /*Text2*/QColor("#dbe2e8"), /*Text3*/QColor("#c7d1da"),
    /*TextDim*/QColor("#8e9aa5"), /*TextDim2*/QColor("#9aa6b1"), /*TextFaint*/QColor("#5f6c78"),
    /*TextFaint2*/QColor("#4a555f"), /*TextGhost*/QColor("#39434c"), /*TextMuted*/QColor("#7b8792"),
    /*Accent*/QColor("#ff6a1a"), /*AccentBright*/QColor("#ffa766"), /*AccentBg*/QColor("#2b1a10"),
    /*Ok*/QColor("#4bbd85"), /*OkBright*/QColor("#6fdcab"), /*OkBg*/QColor("#16241d"),
    /*Warn*/QColor("#e2a33c"), /*WarnBg*/QColor("#2e2413"),
    /*Danger*/QColor("#e0574a"), /*DangerText*/QColor("#ff8175"), /*DangerBg*/QColor("#3a1a16"),
    /*DangerBorder*/QColor("#6b2f28"), /*DangerTextDim*/QColor("#d9a9a3"),
    /*NeutralBg*/QColor("#1b2127"), /*NeutralFg*/QColor("#a9b4bd"),
    /*Wall*/QColor("#5a5f68"), /*Grid*/QColor("#151d23"), /*ScanHighlight*/QColor("#8a5a35"),
};

// User — 라이트 테마. 구조(패널/보더/텍스트)는 밝게, 액센트는 한화비전 오렌지
// (흰 배경 대비를 위해 조금 더 진하게).
inline const Palette kUser = {
    /*Bg*/QColor("#f4f5f7"), /*Panel*/QColor("#ffffff"), /*PanelHead*/QColor("#eef0f3"),
    /*BarBg*/QColor("#ffffff"), /*MapBg*/QColor("#eef1f3"),
    /*Border*/QColor("#d7dce1"), /*BorderSoft*/QColor("#e2e6ea"), /*BorderRow*/QColor("#eceff2"),
    /*Text*/QColor("#1b1e21"), /*Text2*/QColor("#262a2e"), /*Text3*/QColor("#3a3f44"),
    /*TextDim*/QColor("#676d74"), /*TextDim2*/QColor("#757b82"), /*TextFaint*/QColor("#989fa6"),
    /*TextFaint2*/QColor("#aeb4ba"), /*TextGhost*/QColor("#c9ced3"), /*TextMuted*/QColor("#6d747b"),
    /*Accent*/QColor("#e14e14"), /*AccentBright*/QColor("#ff7a3d"), /*AccentBg*/QColor("#ffede2"),
    /*Ok*/QColor("#218a50"), /*OkBright*/QColor("#2fa868"), /*OkBg*/QColor("#e4f5eb"),
    /*Warn*/QColor("#b9790f"), /*WarnBg*/QColor("#fbf0dc"),
    /*Danger*/QColor("#d6392c"), /*DangerText*/QColor("#c4291d"), /*DangerBg*/QColor("#fdeae7"),
    /*DangerBorder*/QColor("#f0b8af"), /*DangerTextDim*/QColor("#a85950"),
    /*NeutralBg*/QColor("#eef0f2"), /*NeutralFg*/QColor("#5d636a"),
    /*Wall*/QColor("#78808a"), /*Grid*/QColor("#e4e8ec"), /*ScanHighlight*/QColor("#c98a4f"),
};

inline Mode CurrentMode = Mode::User;

inline QColor Bg, Panel, PanelHead, BarBg, MapBg;
inline QColor Border, BorderSoft, BorderRow;
inline QColor Text, Text2, Text3, TextDim, TextDim2, TextFaint, TextFaint2, TextGhost, TextMuted;
inline QColor Accent, AccentBright, AccentBg;
inline QColor Ok, OkBright, OkBg;
inline QColor Warn, WarnBg;
inline QColor Danger, DangerText, DangerBg, DangerBorder, DangerTextDim;
inline QColor NeutralBg, NeutralFg;
inline QColor Wall, Grid, ScanHighlight;

inline void setMode(Mode mode) {
    CurrentMode = mode;
    const Palette &p = (mode == Mode::Developer) ? kDeveloper : kUser;
    Bg = p.Bg; Panel = p.Panel; PanelHead = p.PanelHead; BarBg = p.BarBg; MapBg = p.MapBg;
    Border = p.Border; BorderSoft = p.BorderSoft; BorderRow = p.BorderRow;
    Text = p.Text; Text2 = p.Text2; Text3 = p.Text3; TextDim = p.TextDim; TextDim2 = p.TextDim2;
    TextFaint = p.TextFaint; TextFaint2 = p.TextFaint2; TextGhost = p.TextGhost; TextMuted = p.TextMuted;
    Accent = p.Accent; AccentBright = p.AccentBright; AccentBg = p.AccentBg;
    Ok = p.Ok; OkBright = p.OkBright; OkBg = p.OkBg;
    Warn = p.Warn; WarnBg = p.WarnBg;
    Danger = p.Danger; DangerText = p.DangerText; DangerBg = p.DangerBg;
    DangerBorder = p.DangerBorder; DangerTextDim = p.DangerTextDim;
    NeutralBg = p.NeutralBg; NeutralFg = p.NeutralFg;
    Wall = p.Wall; Grid = p.Grid; ScanHighlight = p.ScanHighlight;
}

// 프로그램 시작 시 1회 기본값(User = 화이트)으로 초기화한다. 즉시실행식으로
// 채워둬야 Theme:: 값을 참조하는 다른 정적 초기화가 있어도 항상 유효하다 —
// 정적 초기화 순서에 기대지 않으려는 장치다.
inline bool _themeInit = [] { setMode(Mode::User); return true; }();

inline QString mono(int px, int weight = 400) {
    return QString("font-family:'JetBrains Mono','D2Coding',monospace;font-size:%1px;font-weight:%2;")
        .arg(px).arg(weight);
}

// 각 조각을 독립적으로 .arg() 해서 이어붙인다 — 자리표시자가 20개 넘게 한
// 문자열에 몰리면 번호를 세다 실수하기 쉬워서, 조각마다 %1.. 을 새로 쓴다.
inline QString appStyleSheet() {
    QString css;
    css += QString(R"(
QWidget            { background:%1; color:%2;
                     font-family:'Helvetica Neue',Helvetica,Arial,'Malgun Gothic',sans-serif; font-size:12px; }
QLabel             { background:transparent; }
QFrame#panel       { background:%3; border:1px solid %4; border-radius:5px; }
QFrame#panelHead   { background:%5; border:none; border-bottom:1px solid %6; }
QFrame#chip        { background:%3; border:1px solid %4; border-radius:4px; }
QFrame#card        { background:%5; border:1px solid %6; border-radius:4px; }
QLabel#mono        { font-family:'JetBrains Mono','D2Coding',monospace; font-size:11px; color:%7; }
QLabel#label       { font-family:'JetBrains Mono','D2Coding',monospace; font-size:10px;
                     letter-spacing:1px; color:%8; }
)").arg(Bg.name(), Text.name(), Panel.name(), Border.name())
   .arg(PanelHead.name(), BorderSoft.name(), Text3.name(), TextFaint.name());

    css += QString(R"(
QPushButton        { background:%1; border:1px solid %2; border-radius:4px; color:%3;
                     font-family:'JetBrains Mono','D2Coding',monospace; font-size:11px;
                     letter-spacing:1px; padding:0 13px; min-height:30px; }
QPushButton:hover  { background:%4; color:%5; }
QPushButton#accent { background:%6; color:%7; font-weight:700; }
QPushButton#accent:hover { background:%6; color:%5; }
)").arg(PanelHead.name(), Border.name(), TextDim2.name())
   .arg(BorderSoft.name(), Text2.name())
   .arg(AccentBg.name(), AccentBright.name());

    // border-color 를 따로 주면 Qt 스타일(Fusion/macOS 둘 다)이 버튼 아래쪽
    // 변에 자체 베벨/그림자를 겹쳐 그려서 밑변만 다른 색으로 보이는 문제가
    // 있었다. border 를 아예 안 건드리고 기본 QPushButton 규칙의 회색
    // border(HOME/STOP/REARM 과 동일)를 그대로 물려받게 두면 네 변이 항상
    // 일관되게 그려진다 — 배경색 + 굵은 글씨만으로 강조한다.
    css += QString(R"(
QPushButton#powerOn  { background:%1; color:%2; font-weight:700; }
QPushButton#powerOff { background:%3; color:%4; font-weight:700; }
)").arg(OkBg.name(), OkBright.name())
   .arg(DangerBg.name(), DangerText.name());

    // 체크박스는 기본 스타일에 맡기면 표시기(네모)가 배경색과 거의 같은 회색으로
    // 그려져서 체크 여부는커녕 있는지도 안 보였다(다크에서 특히). 테두리를 주고,
    // 켜지면 액센트로 꽉 채운다 — 체크 표시 이미지 없이 "채워짐 = 켜짐"으로 읽힌다.
    css += QString(R"(
QCheckBox          { background:transparent; color:%1; font-size:12px; spacing:8px; padding:2px 0; }
QCheckBox:disabled { color:%2; }
QCheckBox::indicator { width:15px; height:15px; border-radius:3px;
                     border:1px solid %3; background:%4; }
QCheckBox::indicator:hover   { border:1px solid %5; }
QCheckBox::indicator:checked { background:%5; border:1px solid %5; }
QCheckBox::indicator:checked:hover { background:%6; border:1px solid %6; }
)").arg(Text2.name(), TextFaint.name(), Border.name())
   .arg(Bg.name(), Accent.name(), AccentBright.name());

    css += QString(R"(
QTabWidget::pane   { border:none; background:%1; }
QTabWidget::tab-bar { left: 10px; }
QTabBar            { background:%2; }
QTabBar::tab       { background:transparent; color:%3; padding:10px 16px;
                     border-bottom:2px solid transparent;
                     border-top-left-radius:5px; border-top-right-radius:5px;
                     font-family:'JetBrains Mono','D2Coding',monospace; font-size:11px; letter-spacing:1px; }
QTabBar::tab:selected { background:%4; color:%5; border-bottom:2px solid %6; }
QTabBar::tab:!selected:hover { background:%7; }
)").arg(Bg.name(), BarBg.name(), TextFaint2.name())
   .arg(PanelHead.name(), Text.name(), Accent.name())
   .arg(BorderSoft.name());

    css += QString(R"(
QTableWidget       { background:%1; gridline-color:%2; border:none;
                     font-family:'JetBrains Mono','D2Coding',monospace; font-size:11px; color:%3; }
QHeaderView::section { background:%4; color:%5; border:none;
                     border-bottom:1px solid %2; padding:6px 8px;
                     font-family:'JetBrains Mono','D2Coding',monospace; font-size:10px; letter-spacing:1px; }
QPlainTextEdit     { background:%1; border:none; color:%3;
                     font-family:'JetBrains Mono','D2Coding',monospace; font-size:11px; }
)").arg(Panel.name(), BorderSoft.name(), TextDim2.name())
   .arg(PanelHead.name(), TextFaint.name());

    css += QString(R"(
QScrollBar:vertical   { background:transparent; width:8px; }
QScrollBar::handle:vertical { background:%1; border-radius:4px; }
QScrollBar::add-line, QScrollBar::sub-line { height:0; width:0; }
QMenuBar           { background:%2; color:%3; border-bottom:1px solid %1;
                     font-family:'JetBrains Mono','D2Coding',monospace; font-size:11px; }
QMenuBar::item:selected { background:%4; }
QMenu              { background:%5; border:1px solid %1; color:%6; }
QMenu::item:selected { background:%4; }
)").arg(Border.name(), BarBg.name(), TextDim2.name())
   .arg(BorderSoft.name(), PanelHead.name(), Text3.name());

    return css;
}
} // namespace Theme
