#include "TopViewPanel.h"
#include "TopViewWidget.h"
#include "ScanView3D.h"
#include <QStackedWidget>
#include <QPushButton>
#include <QButtonGroup>
#include <QMenu>
#include <QAction>
#include <QLocale>
#include <QFileInfo>
#include "Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QMouseEvent>
#include <QSizePolicy>

namespace {
QLabel *legendDot(QWidget *parent, const QColor &c) {
    auto *l = new QLabel(QChar(0x25CF), parent);
    l->setStyleSheet(QString("color:%1;font-size:9px;").arg(c.name()));
    return l;
}
QFrame *vDivider(QWidget *parent) {
    auto *f = new QFrame(parent);
    f->setFrameShape(QFrame::VLine);
    f->setStyleSheet(QString("background:%1;border:none;max-width:1px;min-width:1px;").arg(Theme::PanelHead.name()));
    return f;
}
QVBoxLayout *statCell(QWidget *parent, const QString &label, QLabel **value) {
    auto *box = new QVBoxLayout;
    box->setSpacing(3);
    auto *k = new QLabel(label, parent);
    k->setStyleSheet(Theme::mono(10) + QString("color:%1;letter-spacing:1px;").arg(Theme::TextFaint.name()));
    *value = new QLabel(parent);
    (*value)->setStyleSheet(Theme::mono(19, 700) + QString("color:%1;").arg(Theme::Text3.name()));
    box->addWidget(k);
    box->addWidget(*value);
    return box;
}
}

TopViewPanel::TopViewPanel(QWidget *parent) : QFrame(parent) {
    setObjectName("panel");
    setMinimumWidth(kMinWidth);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── 상단 헤더 바
    auto *head = new QFrame(this);
    head->setObjectName("panelHead");
    head->setFixedHeight(36);
    auto *hl = new QHBoxLayout(head);
    hl->setContentsMargins(11, 0, 11, 0);
    hl->setSpacing(6);

    auto *ht = new QLabel("TOP-VIEW", head);
    ht->setStyleSheet(Theme::mono(10, 700) + QString("color:%1;letter-spacing:2px;").arg(Theme::Accent.name()));
    // 부제("천장 중앙 기준")는 뺐다 — 킷이 천장 중앙에 있다는 건 지도의 원점
    // 마커가 이미 말해주고, 헤더 폭만 잡아먹어 패널을 그만큼 못 줄이게 했다.
    hl->addWidget(ht);
    hl->addStretch(1);

    const QString btnCss = QString(
        "QPushButton { background:%1; border:1px solid %2; border-radius:3px; color:%3;"
        " font-family:'JetBrains Mono','D2Coding',monospace; font-size:10px;"
        " letter-spacing:0.5px; padding:0 8px; min-height:22px; max-height:22px; }"
        "QPushButton:hover { background:%4; color:%5; border-color:%5; }"
        "QPushButton:checked { background:%6; color:%7; border-color:%8; font-weight:bold; }")
        .arg(Theme::PanelHead.name(), Theme::Border.name(), Theme::Text2.name())
        .arg(Theme::AccentBg.name(), Theme::AccentBright.name())
        .arg(Theme::AccentBg.name(), Theme::AccentBright.name(), Theme::Accent.name());

    // 1. [📁 스캔 파일] 버튼 (로컬 파일 열기 / 서버 스캔 목록 선택 메뉴)
    // 라벨 뒤에 붙이던 드롭다운 표시 "▾" 는 뺐다 — 10px 에서는 삼각형이 안 보이고
    // 그냥 점 하나가 붙은 것처럼 읽혔다. 누르면 메뉴가 뜨는 건 어차피 알 수 있다.
    m_btnScan = new QPushButton(QString::fromUtf8("📁 스캔 파일"), head);
    m_btnScan->setFixedHeight(22);
    m_btnScan->setCursor(Qt::PointingHandCursor);
    m_btnScan->setStyleSheet(btnCss);
    m_btnScan->setToolTip(QString::fromUtf8("스캔 파일 (PCD 포인트클라우드) 불러오기"));

    auto *scanMenu = new QMenu(m_btnScan);
    scanMenu->setStyleSheet(QString(
        "QMenu { background:%1; border:1px solid %2; padding:4px; }"
        "QMenu::item { padding:6px 14px; font-family:'JetBrains Mono','D2Coding',monospace; font-size:11px; color:%3; border-radius:3px; }"
        "QMenu::item:selected { background:%4; color:%5; }")
        .arg(Theme::Panel.name(), Theme::Border.name(), Theme::Text2.name(), Theme::AccentBg.name(), Theme::AccentBright.name()));

    auto *actOpenLocal = scanMenu->addAction(QString::fromUtf8("📁 로컬 PCD 파일 열기..."));
    auto *actOpenServer = scanMenu->addAction(QString::fromUtf8("🌐 서버 스캔 목록 선택..."));
    connect(actOpenLocal, &QAction::triggered, this, &TopViewPanel::openScanFileRequested);
    connect(actOpenServer, &QAction::triggered, this, &TopViewPanel::showScanListDialogRequested);
    m_btnScan->setMenu(scanMenu);
    hl->addWidget(m_btnScan);

    // 2. [📐 캘리브레이션 RT] 버튼 (로컬 파일 열기 / CCTV 카메라에서 가져오기 메뉴)
    m_btnCalib = new QPushButton(QString::fromUtf8("📐 캘리브레이션 RT"), head);
    m_btnCalib->setFixedHeight(22);
    m_btnCalib->setCursor(Qt::PointingHandCursor);
    m_btnCalib->setStyleSheet(btnCss);
    m_btnCalib->setToolTip(QString::fromUtf8("캘리브레이션 결과 (RT) 불러오기 (로컬 / CCTV 카메라)"));

    auto *calibMenu = new QMenu(m_btnCalib);
    calibMenu->setStyleSheet(QString(
        "QMenu { background:%1; border:1px solid %2; padding:4px; }"
        "QMenu::item { padding:6px 14px; font-family:'JetBrains Mono','D2Coding',monospace; font-size:11px; color:%3; border-radius:3px; }"
        "QMenu::item:selected { background:%4; color:%5; }")
        .arg(Theme::Panel.name(), Theme::Border.name(), Theme::Text2.name(), Theme::AccentBg.name(), Theme::AccentBright.name()));

    auto *actCalibLocal = calibMenu->addAction(QString::fromUtf8("📁 로컬 RT 결과 파일 열기 (result.json)..."));
    auto *actCalibCamera = calibMenu->addAction(QString::fromUtf8("🌐 CCTV 카메라에서 최신 RT 가져오기 (OpenSDK)..."));
    connect(actCalibLocal, &QAction::triggered, this, &TopViewPanel::openCalibResultRequested);
    connect(actCalibCamera, &QAction::triggered, this, &TopViewPanel::fetchCalibResultFromCameraRequested);
    m_btnCalib->setMenu(calibMenu);
    hl->addWidget(m_btnCalib);

    hl->addSpacing(4);
    hl->addWidget(vDivider(head));
    hl->addSpacing(4);

    // 3. [2D] / [3D] 화면 전환 토글 버튼
    m_btn2d = new QPushButton("2D", head);
    m_btn3d = new QPushButton("3D", head);
    auto *viewGroup = new QButtonGroup(this);
    viewGroup->setExclusive(true);
    for (QPushButton *b : {m_btn2d, m_btn3d}) {
        b->setCheckable(true);
        b->setFixedHeight(22);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(btnCss);
        viewGroup->addButton(b);
        hl->addWidget(b);
    }
    m_btn2d->setChecked(true);

    // 4. [⛶ 전체화면] 토글 버튼
    m_btnFull = new QPushButton(QString::fromUtf8("⛶"), head);
    m_btnFull->setFixedHeight(22);
    // 고정폭을 주면 detached 상태 라벨("⛶ 복원")이 그대로 잘린다. 하한만 두고
    // 글자 길이에 따라 늘어나게 한다.
    m_btnFull->setMinimumWidth(28);
    m_btnFull->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_btnFull->setCursor(Qt::PointingHandCursor);
    m_btnFull->setStyleSheet(btnCss);
    m_btnFull->setToolTip(QString::fromUtf8("별도 창에서 전체화면으로 보기 (지도 더블클릭도 같은 동작)"));
    connect(m_btnFull, &QPushButton::clicked, this, &TopViewPanel::fullScreenToggleRequested);
    hl->addWidget(m_btnFull);

    // ── 메인 캔버스 스택: 0 = 2D TopViewWidget, 1 = 3D ScanView3D
    m_map = new TopViewWidget(this);
    m_map->setRoomSize(10.0, 10.0);
    m_view3d = new ScanView3D(this);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_map);      // 0: 2D 맵
    m_stack->addWidget(m_view3d);   // 1: 3D 뷰어

    connect(m_btn2d, &QPushButton::clicked, this, [this] { m_stack->setCurrentIndex(0); });
    connect(m_btn3d, &QPushButton::clicked, this, [this] { m_stack->setCurrentIndex(1); });

    // ── 범례 바
    auto *legend = new QFrame(this);
    legend->setStyleSheet(QString("background:%1;border:none;border-top:1px solid %2;")
        .arg(Theme::BarBg.name(), Theme::BorderSoft.name()));
    auto *legendV = new QVBoxLayout(legend);
    legendV->setContentsMargins(11, 7, 11, 7);
    legendV->setSpacing(5);
    auto *ll = new QHBoxLayout;
    ll->setSpacing(10);
    legendV->addLayout(ll);
    auto mono10 = [](QLabel *l) { l->setStyleSheet(Theme::mono(10) + QString("color:%1;").arg(Theme::TextMuted.name())); };

    ll->addWidget(legendDot(legend, Theme::Warn));
    auto *lp = new QLabel("PERSON · RTSP", legend); mono10(lp); ll->addWidget(lp);

    auto *wallSwatch = new QLabel(legend);
    wallSwatch->setFixedSize(14, 2);
    wallSwatch->setStyleSheet(QString("background:%1;").arg(Theme::Wall.name()));
    ll->addWidget(wallSwatch);
    auto *lw = new QLabel("WALL", legend); mono10(lw); ll->addWidget(lw);

    auto *cloudSwatch = new QLabel(legend);
    cloudSwatch->setFixedSize(14, 10);
    cloudSwatch->setStyleSheet(Theme::CurrentMode == Theme::Mode::Developer
        ? QStringLiteral("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                         "stop:0 #1a3b73, stop:0.5 #57c8b8, stop:1 #fff0c2);")
        : QStringLiteral("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                         "stop:0 #172a6b, stop:0.5 #159187, stop:1 #d9a021);"));
    ll->addWidget(cloudSwatch);
    auto *lc = new QLabel(QString::fromUtf8("높이"), legend); mono10(lc); ll->addWidget(lc);

    ll->addStretch(1);
    m_coverage = new QLabel(legend);
    mono10(m_coverage);
    ll->addWidget(m_coverage);

    // 2행: 점군 요약 (점 수, 미반사, 반경)
    m_cloudInfo = new QLabel(legend);
    mono10(m_cloudInfo);
    m_cloudInfo->setWordWrap(true);
    legendV->addWidget(m_cloudInfo);

    m_objectInfo = new QLabel(legend);
    mono10(m_objectInfo);
    m_objectInfo->setWordWrap(true);
    legendV->addWidget(m_objectInfo);

    // ── 하단 통계 바
    auto *stats = new QFrame(this);
    stats->setStyleSheet(QString("background:%1;border:none;").arg(Theme::BarBg.name()));
    stats->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *sv = new QVBoxLayout(stats);
    sv->setContentsMargins(11, 8, 11, 10);
    sv->setSpacing(8);

    auto *summary = new QHBoxLayout;
    auto *sTitle = new QLabel("SCAN", stats);
    sTitle->setStyleSheet(Theme::mono(11, 700) + QString("color:%1;letter-spacing:1px;").arg(Theme::Accent.name()));
    m_scanSummary = new QLabel(stats);
    m_scanSummary->setStyleSheet(Theme::mono(11) + QString("color:%1;").arg(Theme::TextDim2.name()));
    m_scanSummary->setWordWrap(true);
    summary->addWidget(sTitle);
    summary->addWidget(m_scanSummary, 1);
    sv->addLayout(summary);

    auto *row = new QHBoxLayout;
    row->setSpacing(12);
    row->addLayout(statCell(stats, "ROLL", &m_roll));
    row->addWidget(vDivider(stats));
    row->addLayout(statCell(stats, "PITCH", &m_pitch));
    row->addWidget(vDivider(stats));
    row->addLayout(statCell(stats, "SCAN PTS", &m_scanPts));
    row->addWidget(vDivider(stats));
    row->addLayout(statCell(stats, "EXPECTED", &m_expected));
    sv->addLayout(row);

    root->addWidget(head);
    root->addWidget(m_stack, 1);
    root->addWidget(legend);
    root->addWidget(stats);

    for (QWidget *w : {static_cast<QWidget *>(this), static_cast<QWidget *>(head),
                       static_cast<QWidget *>(m_map), static_cast<QWidget *>(legend),
                       static_cast<QWidget *>(stats)}) {
        w->installEventFilter(this);
    }

    setImu({});
    setDaemonState({});
    setScanProgress({});
    setObjects({});

    // 헤더 라벨은 폭에 따라 바뀌지 않는다 — 스플리터를 끌 때마다 버튼 글자가
    // 바뀌면 어디를 누르려던 건지 매번 다시 읽어야 한다. 대신 헤더가 요구하는
    // 최소 폭을 그대로 하한으로 쓴다(폰트/플랫폼마다 글자 폭이 달라서 상수로
    // 박으면 어딘가에서는 결국 잘린다). sizeHint 가 아니라 minimumSizeHint 라,
    // 위에서 Ignored 로 둔 부제 폭만큼은 더 줄일 수 있다. 버튼은 여기에 자기
    // 전체 라벨 폭을 그대로 싣기 때문에 어떤 폭에서도 잘리지 않는다.
    setMinimumWidth(qMax(kMinWidth, head->minimumSizeHint().width()));
}

bool TopViewPanel::eventFilter(QObject *watched, QEvent *ev) {
    if (ev->type() == QEvent::MouseButtonDblClick
        && static_cast<QMouseEvent *>(ev)->button() == Qt::LeftButton) {
        emit fullScreenToggleRequested();
        return true;
    }
    return QFrame::eventFilter(watched, ev);
}

void TopViewPanel::setDetached(bool detached) {
    m_btnFull->setText(detached ? QString::fromUtf8("⛶ 복원")
                                : QString::fromUtf8("⛶"));
    m_btnFull->setToolTip(detached
        ? QString::fromUtf8("대시보드로 되돌린다 (Esc · 지도 더블클릭도 같은 동작)")
        : QString::fromUtf8("별도 창에서 전체화면으로 본다 (지도 더블클릭도 같은 동작)"));
}

void TopViewPanel::setRoomSize(double w, double d) { m_map->setRoomSize(w, d); }
void TopViewPanel::setEdges(const QVector<MapEdge> &e) { m_map->setEdges(e); }
void TopViewPanel::setObjects(const QVector<SpatialObject> &o) {
    m_map->setObjects(o);
    m_view3d->setObjects(o);
    int topViewCount = 0;
    int personCount = 0;
    int unprojectedCount = 0;
    for (const SpatialObject &object : o) {
        if (!object.hasTopViewPosition()) {
            ++unprojectedCount;
            continue;
        }
        ++topViewCount;
        if (object.cls == QStringLiteral("PERSON")) ++personCount;
    }
    QString status = QStringLiteral("OBJECTS %1 · PERSON %2")
                         .arg(topViewCount).arg(personCount);
    if (unprojectedCount > 0) {
        status += QStringLiteral(" · UNPROJECTED %1").arg(unprojectedCount);
    }
    m_objectInfo->setText(status);
}

void TopViewPanel::setScanCloud(const ScanCloud &c) {
    m_map->setScanCloud(c);
    m_view3d->setScanCloud(c);
    setCloudStatus(QString::fromUtf8("CLOUD %1 pts · 미반사 %2 · 반경 %3 m")
                       .arg(QLocale(QLocale::English).toString(c.count()))
                       .arg(c.invalid)
                       .arg(c.radiusM(), 0, 'f', 1),
                   false);
}

void TopViewPanel::setCloudStatus(const QString &msg, bool isError) {
    m_cloudInfo->setText(msg);
    m_cloudInfo->setStyleSheet(Theme::mono(10) + QString("color:%1;")
        .arg((isError ? Theme::DangerText : Theme::TextMuted).name()));
}

void TopViewPanel::setImu(const ImuState &imu) {
    if (!imu.valid) {
        m_roll->setText("N/A");
        m_pitch->setText("N/A");
        m_roll->setStyleSheet(Theme::mono(19, 700) + QString("color:%1;").arg(Theme::TextFaint.name()));
        m_pitch->setStyleSheet(Theme::mono(19, 700) + QString("color:%1;").arg(Theme::TextFaint.name()));
        return;
    }
    const QString rollColor  = imu.level() ? Theme::Ok.name() : Theme::DangerText.name();
    const QString pitchColor = imu.level() ? Theme::Ok.name() : Theme::DangerText.name();
    m_roll->setText(QString("%1°").arg(imu.roll, 0, 'f', 1));
    m_roll->setStyleSheet(Theme::mono(19, 700) + QString("color:%1;").arg(rollColor));
    m_pitch->setText(QString("%1°").arg(imu.pitch, 0, 'f', 1));
    m_pitch->setStyleSheet(Theme::mono(19, 700) + QString("color:%1;").arg(pitchColor));
}

void TopViewPanel::setDaemonState(const DaemonState &s) {
    m_scanSummary->setText(QStringLiteral("%1 · %2 · homed %3 · %4")
                               .arg(s.state,
                                    s.linkAlive ? "link UP" : "link DOWN",
                                    s.homed ? "Y" : "N",
                                    s.ts.isValid() ? s.ts.toString("HH:mm:ss") : "--:--:--"));
}

void TopViewPanel::setScanProgress(const ScanProgress &p) {
    m_scanPts->setText(QString::number(p.points));
    m_expected->setText(QString::number(p.expected));
}

void TopViewPanel::setScanResult(const ScanResult &r) {
    m_scanPts->setText(QString::number(r.points));
    m_expected->setText(QString::number(r.expected));
}

void TopViewPanel::showMap2D() {
    m_btn2d->setChecked(true);
    m_stack->setCurrentIndex(0);
}

void TopViewPanel::showCloud3D() {
    m_btn3d->setChecked(true);
    m_stack->setCurrentIndex(1);
}
