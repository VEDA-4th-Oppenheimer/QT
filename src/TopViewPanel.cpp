#include "TopViewPanel.h"
#include "TopViewWidget.h"
#include "ScanView3D.h"
#include <QStackedWidget>
#include <QPushButton>
#include <QButtonGroup>
#include <QListWidget>
#include <QLocale>
#include <QFileInfo>
#include "Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QLocale>

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
    setFixedWidth(430);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // 헤더
    auto *head = new QFrame(this);
    head->setObjectName("panelHead");
    head->setFixedHeight(34);
    auto *hl = new QHBoxLayout(head);
    hl->setContentsMargins(11, 0, 11, 0);
    auto *ht = new QLabel("TOP-VIEW", head);
    ht->setStyleSheet(Theme::mono(10, 700) + QString("color:%1;letter-spacing:2px;").arg(Theme::Accent.name()));
    auto *hs = new QLabel(QString::fromUtf8("천장 중앙 기준"), head);
    hs->setStyleSheet(QString("color:%1;font-size:11px;").arg(Theme::TextMuted.name()));
    hl->addWidget(ht); hl->addSpacing(8); hl->addWidget(hs, 1); hl->addStretch(0);

    // 2D 조감 / 3D 점군 전환. 둘은 같은 데이터의 다른 단면이다 — 2D 는 카메라
    // FOV·감지객체와 같이 보는 배치도이고, 3D 는 벽 높이를 확인하는 용도다.
    // 바닥 투영만으로는 벽과 바닥이 같은 점으로 겹쳐서 구분이 안 된다.
    // 전역 QPushButton 규칙은 min-height:30px / padding:0 13px 이라 34px 헤더에
    // 그대로 넣으면 찌그러진다. 또 :checked 규칙이 전역에 없어서 어느 쪽이 켜져
    // 있는지 표시가 안 난다 — 이 두 개만 따로 정의한다.
    const QString segCss = QString(
        "QPushButton { background:%1; border:1px solid %2; border-radius:3px; color:%3;"
        " font-family:'JetBrains Mono','D2Coding',monospace; font-size:10px;"
        " letter-spacing:1px; padding:0 9px; min-height:0px; min-width:0px; }"
        "QPushButton:hover { color:%4; }"
        "QPushButton:checked { background:%5; color:%6; border-color:%7; }")
        .arg(Theme::PanelHead.name(), Theme::Border.name(), Theme::TextFaint.name())
        .arg(Theme::Text2.name())
        .arg(Theme::AccentBg.name(), Theme::AccentBright.name(), Theme::Accent.name());

    m_btn2d = new QPushButton("2D", head);
    m_btn3d = new QPushButton("3D", head);
    QPushButton *btn2d = m_btn2d, *btn3d = m_btn3d;
    auto *viewGroup = new QButtonGroup(this);
    viewGroup->setExclusive(true);   // 둘 중 하나만 켜지도록 Qt 가 관리한다
    for (QPushButton *b : {btn2d, btn3d}) {
        b->setCheckable(true);
        b->setFixedHeight(20);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(segCss);
        viewGroup->addButton(b);
        hl->addSpacing(5);
        hl->addWidget(b);
    }
    btn2d->setChecked(true);

    // 캔버스 — 2D 지도와 3D 뷰를 같은 자리에 겹쳐 두고 헤더 버튼으로 바꾼다.
    m_map = new TopViewWidget(this);
    m_map->setRoomSize(10.0, 10.0);
    m_view3d = new ScanView3D(this);
    // ── 3D 칸은 두 화면이다: 파일 목록 → 선택하면 뷰어, 뷰어에서 뒤로가면 목록.
    auto *listPage = new QWidget(this);
    auto *lpv = new QVBoxLayout(listPage);
    lpv->setContentsMargins(10, 10, 10, 8);
    lpv->setSpacing(7);
    auto *lpHead = new QHBoxLayout;
    auto *lpTitle = new QLabel(QString::fromUtf8("스캔 파일"), listPage);
    lpTitle->setStyleSheet(Theme::mono(11, 700) + QString("color:%1;letter-spacing:1px;").arg(Theme::Accent.name()));
    auto *btnRefresh = new QPushButton(QString::fromUtf8("새로고침"), listPage);
    btnRefresh->setFixedHeight(20);
    btnRefresh->setCursor(Qt::PointingHandCursor);
    btnRefresh->setStyleSheet(segCss);
    lpHead->addWidget(lpTitle);
    lpHead->addStretch(1);
    lpHead->addWidget(btnRefresh);
    lpv->addLayout(lpHead);

    m_listNote = new QLabel(QString::fromUtf8("목록을 불러오는 중…"), listPage);
    m_listNote->setStyleSheet(Theme::mono(10) + QString("color:%1;").arg(Theme::TextFaint.name()));
    lpv->addWidget(m_listNote);

    m_list = new QListWidget(listPage);
    m_list->setStyleSheet(QString(
        "QListWidget { background:%1; border:1px solid %2; border-radius:4px;"
        " font-family:'JetBrains Mono','D2Coding',monospace; font-size:10px; color:%3; }"
        "QListWidget::item { padding:6px 8px; border-bottom:1px solid %4; }"
        "QListWidget::item:selected { background:%5; color:%6; }"
        "QListWidget::item:hover { background:%4; }")
        .arg(Theme::MapBg.name(), Theme::Border.name(), Theme::Text3.name())
        .arg(Theme::BorderRow.name())
        .arg(Theme::AccentBg.name(), Theme::AccentBright.name()));
    lpv->addWidget(m_list, 1);

    auto *viewPage = new QWidget(this);
    auto *vpv = new QVBoxLayout(viewPage);
    vpv->setContentsMargins(0, 0, 0, 0);
    vpv->setSpacing(0);
    auto *backBar = new QFrame(viewPage);
    backBar->setFixedHeight(28);
    backBar->setStyleSheet(QString("background:%1;border:none;border-bottom:1px solid %2;")
        .arg(Theme::BarBg.name(), Theme::BorderSoft.name()));
    auto *bbl = new QHBoxLayout(backBar);
    bbl->setContentsMargins(8, 0, 8, 0);
    auto *btnBack = new QPushButton(QString::fromUtf8("← 목록"), backBar);
    btnBack->setFixedHeight(20);
    btnBack->setCursor(Qt::PointingHandCursor);
    btnBack->setStyleSheet(segCss);
    m_viewTitle = new QLabel(backBar);
    m_viewTitle->setStyleSheet(Theme::mono(10) + QString("color:%1;").arg(Theme::TextMuted.name()));
    bbl->addWidget(btnBack);
    bbl->addSpacing(8);
    bbl->addWidget(m_viewTitle, 1);
    vpv->addWidget(backBar);
    vpv->addWidget(m_view3d, 1);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_map);        // 0
    m_stack->addWidget(listPage);     // 1
    m_stack->addWidget(viewPage);     // 2

    connect(btnRefresh, &QPushButton::clicked, this, [this] {
        m_listNote->setText(QString::fromUtf8("목록을 불러오는 중…"));
        emit refreshRequested();
    });
    connect(btnBack, &QPushButton::clicked, this, [this] { showScanList(); });
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *it) {
        if (it == nullptr) return;
        emit scanChosen(it->data(Qt::UserRole).toString(), it->data(Qt::UserRole + 1).toString());
    });
    // 한 번 클릭으로 열리게 한다 — 목록이 짧고 파괴적 동작이 아니다.
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
        if (it == nullptr) return;
        emit scanChosen(it->data(Qt::UserRole).toString(), it->data(Qt::UserRole + 1).toString());
    });

    connect(btn2d, &QPushButton::clicked, this, [this] { m_stack->setCurrentIndex(0); });
    connect(btn3d, &QPushButton::clicked, this, [this] {
        // 이미 띄워둔 점군이 있으면 바로 뷰어로, 없으면 목록부터.
        if (m_view3d->hasCloud()) m_stack->setCurrentIndex(2);
        else showScanList();
    });

    // 범례 바
    auto *legend = new QFrame(this);
    legend->setStyleSheet(QString("background:%1;border:none;border-top:1px solid %2;")
        .arg(Theme::BarBg.name(), Theme::BorderSoft.name()));
    // 430px 한 줄에 스와치 4종 + 점군 요약까지 넣으면 잘린다. 2행으로 나눈다.
    auto *legendV = new QVBoxLayout(legend);
    legendV->setContentsMargins(11, 7, 11, 7);
    legendV->setSpacing(5);
    auto *ll = new QHBoxLayout;
    ll->setSpacing(10);
    legendV->addLayout(ll);
    auto mono10 = [](QLabel *l) { l->setStyleSheet(Theme::mono(10) + QString("color:%1;").arg(Theme::TextMuted.name())); };

    ll->addWidget(legendDot(legend, Theme::Warn));
    auto *lp = new QLabel("PERSON", legend); mono10(lp); ll->addWidget(lp);

    auto *wallSwatch = new QLabel(legend);
    wallSwatch->setFixedSize(14, 2);
    wallSwatch->setStyleSheet(QString("background:%1;").arg(Theme::Wall.name()));
    ll->addWidget(wallSwatch);
    auto *lw = new QLabel("WALL", legend); mono10(lw); ll->addWidget(lw);

    auto *fovSwatch = new QLabel(legend);
    fovSwatch->setFixedSize(14, 10);
    fovSwatch->setStyleSheet(QString("border:1px dashed %1;").arg(Theme::ScanHighlight.name()));
    ll->addWidget(fovSwatch);
    auto *lf = new QLabel("FOV", legend); mono10(lf); ll->addWidget(lf);

    auto *cloudSwatch = new QLabel(legend);
    cloudSwatch->setFixedSize(14, 10);
    // 높이 램프의 양 끝(낮음→높음)을 그대로 보여준다 — TopViewWidget::cloudRamp 와 같은 색.
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

    // 2행: 점군 요약(점 수·미반사·반경) — 길어서 한 줄을 통째로 쓴다.
    m_cloudInfo = new QLabel(legend);
    mono10(m_cloudInfo);
    legendV->addWidget(m_cloudInfo);

    // 하단 통계 바
    auto *stats = new QFrame(this);
    stats->setStyleSheet(QString("background:%1;border:none;").arg(Theme::BarBg.name()));
    auto *sv = new QVBoxLayout(stats);
    sv->setContentsMargins(11, 8, 11, 10);
    sv->setSpacing(8);

    auto *summary = new QHBoxLayout;
    auto *sTitle = new QLabel("SCAN", stats);
    sTitle->setStyleSheet(Theme::mono(11, 700) + QString("color:%1;letter-spacing:1px;").arg(Theme::Accent.name()));
    m_scanSummary = new QLabel(stats);
    m_scanSummary->setStyleSheet(Theme::mono(11) + QString("color:%1;").arg(Theme::TextDim2.name()));
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

    setImu({});
    setDaemonState({});
    setScanProgress({});
}

void TopViewPanel::setRoomSize(double w, double d) { m_map->setRoomSize(w, d); }
void TopViewPanel::setEdges(const QVector<MapEdge> &e) { m_map->setEdges(e); }
void TopViewPanel::setObjects(const QVector<SpatialObject> &o) { m_map->setObjects(o); }

void TopViewPanel::setScanCloud(const ScanCloud &c) {
    m_map->setScanCloud(c);
    m_view3d->setScanCloud(c);
    m_viewTitle->setText(QFileInfo(c.sourcePath).fileName());
    // 목록에서 고른 직후라면 뷰어를 보여준다. 2D 를 보고 있었다면 방해하지 않는다.
    if (m_stack->currentIndex() == 1) m_stack->setCurrentIndex(2);
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
    const QString color =
        (s.state == "IDLE")     ? Theme::Ok.name() :
        (s.state == "SCANNING") ? Theme::AccentBright.name() :
        (s.state == "DISARM")   ? Theme::DangerText.name() : Theme::TextDim2.name();
    m_scanSummary->setText(QString("%1  ·  link %2  ·  homed %3  ·  %4")
                                .arg(s.state)
                                .arg(s.linkAlive ? "OK" : "DOWN")
                                .arg(s.homed ? "Y" : "N")
                                .arg(s.ts.isValid() ? s.ts.toString("HH:mm:ss") : QString("--:--:--")));
    m_scanSummary->setStyleSheet(Theme::mono(11) + QString("color:%1;").arg(color));
}

void TopViewPanel::setScanProgress(const ScanProgress &p) {
    m_coverage->setText(QString("SCAN PROGRESS %1%").arg(p.percent));
    m_scanPts->setText(QLocale().toString(p.points));
    m_expected->setText(QLocale().toString(p.expected));
}

void TopViewPanel::setScanResult(const ScanResult &r) {
    // state/scan(develop 브랜치 실구현)은 expected 를 안 보낸다 — event/progress
    // 에서 마지막으로 받은 값을 그대로 둔다(덮어써서 0으로 리셋하지 않는다).
    m_coverage->setText(QStringLiteral("SCAN PROGRESS 100%"));
    m_scanPts->setText(QLocale().toString(r.points));
}

void TopViewPanel::showScanList() {
    m_btn3d->setChecked(true);
    m_stack->setCurrentIndex(1);
    m_listNote->setText(QString::fromUtf8("목록을 불러오는 중…"));
    emit refreshRequested();
}

void TopViewPanel::setScanList(const QVector<ScanEntry> &entries, const QString &note) {
    m_list->clear();
    const QLocale loc(QLocale::English);
    for (const ScanEntry &e : entries) {
        // 파일명은 calib-YYYYMMDD-HHMMSS_sweep-NNNNNN.pcd 라 그대로 두면 길다.
        // 뒤에 크기와 출처를 붙여 한 줄에 판단할 수 있게 한다.
        const QString size = e.size > 0 ? QStringLiteral("%1 KB").arg(e.size / 1024) : QStringLiteral("—");
        const QString when = e.mtime.isValid() ? e.mtime.toString("MM-dd HH:mm") : QStringLiteral("");
        auto *it = new QListWidgetItem(QStringLiteral("%1\n%2 · %3 · %4")
                                           .arg(e.name, size, when,
                                                e.isLocal() ? QString::fromUtf8("로컬")
                                                            : QString::fromUtf8("서버")));
        it->setData(Qt::UserRole, e.name);
        it->setData(Qt::UserRole + 1, e.localPath);
        m_list->addItem(it);
    }
    if (entries.isEmpty()) {
        m_listNote->setText(QString::fromUtf8("스캔 파일이 없다 — %1").arg(note));
    } else {
        m_listNote->setText(note);
    }
    Q_UNUSED(loc);
}
