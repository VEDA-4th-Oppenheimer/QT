#include "TopViewPanel.h"
#include "TopViewWidget.h"
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
    f->setStyleSheet("background:#1a2229;border:none;max-width:1px;min-width:1px;");
    return f;
}
QVBoxLayout *statCell(QWidget *parent, const QString &label, QLabel **value) {
    auto *box = new QVBoxLayout;
    box->setSpacing(3);
    auto *k = new QLabel(label, parent);
    k->setStyleSheet(Theme::mono(10) + "color:#5f6c78;letter-spacing:1px;");
    *value = new QLabel(parent);
    (*value)->setStyleSheet(Theme::mono(19, 700) + "color:#c7d1da;");
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
    ht->setStyleSheet(Theme::mono(10, 700) + "color:#3fbfcc;letter-spacing:2px;");
    auto *hs = new QLabel(QString::fromUtf8("실내 3D 맵 조감 단면 · 천장 중앙 기준"), head);
    hs->setStyleSheet("color:#7b8792;font-size:11px;");
    auto *hr = new QLabel("1 grid = 1.0 m", head);
    hr->setStyleSheet(Theme::mono(10) + "color:#5f6c78;");
    hl->addWidget(ht); hl->addSpacing(8); hl->addWidget(hs); hl->addStretch(1); hl->addWidget(hr);

    // 캔버스
    m_map = new TopViewWidget(this);
    m_map->setRoomSize(10.0, 10.0);

    // 범례 바
    auto *legend = new QFrame(this);
    legend->setStyleSheet("background:#0e1317;border:none;border-top:1px solid #1e262d;");
    auto *ll = new QHBoxLayout(legend);
    ll->setContentsMargins(11, 8, 11, 8);
    ll->setSpacing(16);
    auto mono10 = [](QLabel *l) { l->setStyleSheet(Theme::mono(10) + "color:#7b8792;"); };

    ll->addWidget(legendDot(legend, Theme::Warn));
    auto *lp = new QLabel("PERSON", legend); mono10(lp); ll->addWidget(lp);

    auto *wallSwatch = new QLabel(legend);
    wallSwatch->setFixedSize(14, 2);
    wallSwatch->setStyleSheet("background:#46596a;");
    ll->addWidget(wallSwatch);
    auto *lw = new QLabel("WALL / EDGE", legend); mono10(lw); ll->addWidget(lw);

    auto *fovSwatch = new QLabel(legend);
    fovSwatch->setFixedSize(14, 10);
    fovSwatch->setStyleSheet("border:1px dashed #2a5f68;");
    ll->addWidget(fovSwatch);
    auto *lf = new QLabel(QString::fromUtf8("CH FOV ×4"), legend); mono10(lf); ll->addWidget(lf);

    ll->addStretch(1);
    m_coverage = new QLabel(legend);
    mono10(m_coverage);
    ll->addWidget(m_coverage);

    // 하단 통계 바
    auto *stats = new QFrame(this);
    stats->setStyleSheet("background:#0e1317;border:none;");
    auto *sv = new QVBoxLayout(stats);
    sv->setContentsMargins(11, 8, 11, 10);
    sv->setSpacing(8);

    auto *summary = new QHBoxLayout;
    auto *sTitle = new QLabel("SCAN", stats);
    sTitle->setStyleSheet(Theme::mono(11, 700) + "color:#3fbfcc;letter-spacing:1px;");
    m_scanSummary = new QLabel(stats);
    m_scanSummary->setStyleSheet(Theme::mono(11) + "color:#9aa6b1;");
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
    root->addWidget(m_map, 1);
    root->addWidget(legend);
    root->addWidget(stats);

    setImu({});
    setDaemonState({});
    setScanProgress({});
}

void TopViewPanel::setRoomSize(double w, double d) { m_map->setRoomSize(w, d); }
void TopViewPanel::setEdges(const QVector<MapEdge> &e) { m_map->setEdges(e); }
void TopViewPanel::setObjects(const QVector<SpatialObject> &o) { m_map->setObjects(o); }

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
