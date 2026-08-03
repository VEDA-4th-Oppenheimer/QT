#include "CalibrationTab.h"
#include "Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPlainTextEdit>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QProgressBar>
#include <QDateTime>
#include <cmath>

namespace {
// "02. Point Cloud 이후 Camera Automatic Calibration" §3 아키텍처 flowchart를 그대로 8단계로 나눔.
const char *kSteps[8][2] = {
    {"1 · SCAN",           "STM32 연속 pan sweep + encoder 동기 취득 (protocol.h v5)"},
    {"2 · POINT CLOUD",    "(pan,tilt,d) -> (x,y,z) 변환, organized grid + point cloud QA"},
    {"3 · INPUT GATE",     "PointCloudPackage schema/topology/intrinsic 검증 (G0)"},
    {"4 · SCENE FEATURES", "Camera edge distance transform + LiDAR depth-edge/LSD 라인 추출"},
    {"5 · COARSE SEARCH",  "기구 실측 bound 안에서 multi-start coarse pose 후보 생성"},
    {"6 · FINE SE(3)",     "6-DoF nonlinear optimize (edge + line residual, robust loss)"},
    {"7 · MULTI-SCENE",    "Joint refinement + hold-out scene 검증"},
    {"8 · QUALITY GATE",   "edge_rmse/inlier/관측성 복합 판정 — PASS 시 active 승격"},
};

QLabel *statRow(QWidget *parent, QVBoxLayout *into, const QString &label) {
    auto *row = new QHBoxLayout;
    auto *k = new QLabel(label, parent);
    k->setStyleSheet(Theme::mono(10) + "color:#5f6c78;");
    auto *v = new QLabel(parent);
    v->setStyleSheet(Theme::mono(11) + "color:#c7d1da;");
    row->addWidget(k);
    row->addStretch(1);
    row->addWidget(v);
    into->addLayout(row);
    return v;
}

QLabel *extrinsicCell(QWidget *parent, QGridLayout *grid, int row, int col) {
    auto *cell = new QLabel("0.000", parent);
    cell->setAlignment(Qt::AlignCenter);
    cell->setStyleSheet(Theme::mono(11) + "color:#c7d1da;background:#101519;border-radius:3px;padding:6px 2px;");
    grid->addWidget(cell, row, col);
    return cell;
}
}

CalibrationTab::CalibrationTab(QWidget *parent) : QWidget(parent) {
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(10);

    // ---- 좌측: 파이프라인 + 로그 -----------------------------------------
    auto *left = new QVBoxLayout;
    left->setSpacing(7);

    auto *pipeHead = new QLabel("TARGETLESS CALIBRATION PIPELINE", this);
    pipeHead->setStyleSheet(Theme::mono(11, 700) + "color:#3fbfcc;letter-spacing:1px;");
    auto *pipeSub = new QLabel(QString::fromUtf8("Scan → Point Cloud → Coarse-to-Fine SE(3) → Quality Gate"), this);
    pipeSub->setStyleSheet("color:#8e9aa5;font-size:11px;");
    left->addWidget(pipeHead);
    left->addWidget(pipeSub);
    left->addSpacing(4);

    for (int i = 0; i < 8; ++i) {
        auto *row = new QFrame(this);
        row->setObjectName("panel");
        auto *rl = new QHBoxLayout(row);
        rl->setContentsMargins(11, 8, 11, 8);
        rl->setSpacing(11);

        m_stepBadge[i] = new QLabel(row);
        m_stepBadge[i]->setFixedSize(16, 16);
        m_stepBadge[i]->setAlignment(Qt::AlignCenter);

        auto *id = new QLabel(kSteps[i][0], row);
        id->setFixedWidth(170);
        id->setStyleSheet(Theme::mono(11, 500) + "color:#dbe2e8;");
        auto *desc = new QLabel(kSteps[i][1], row);
        desc->setStyleSheet("color:#8e9aa5;");

        m_stepState[i] = new QLabel("PENDING", row);
        m_stepState[i]->setStyleSheet(Theme::mono(10, 700) + "color:#5f6c78;");

        rl->addWidget(m_stepBadge[i]);
        rl->addWidget(id);
        rl->addWidget(desc, 1);
        rl->addWidget(m_stepState[i]);
        left->addWidget(row);
    }

    auto *logHead = new QLabel("SESSION LOG", this);
    logHead->setStyleSheet(Theme::mono(11, 700) + "color:#3fbfcc;letter-spacing:1px;margin-top:6px;");
    left->addWidget(logHead);
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    left->addWidget(m_log, 1);

    // ---- 우측: EXTRINSIC + QUALITY -----------------------------------------
    auto *right = new QVBoxLayout;
    right->setSpacing(10);

    auto *rtPanel = new QFrame(this);
    rtPanel->setObjectName("panel");
    auto *rtl = new QVBoxLayout(rtPanel);
    rtl->setContentsMargins(11, 10, 11, 10);
    rtl->setSpacing(8);
    auto *rtTitle = new QLabel("EXTRINSIC  T_camera_ch1_lidar_scan", rtPanel);
    rtTitle->setStyleSheet(Theme::mono(11, 700) + "color:#3fbfcc;letter-spacing:1px;");
    rtl->addWidget(rtTitle);

    auto *tLabel = new QLabel("translation_m  [x, y, z]", rtPanel);
    tLabel->setStyleSheet(Theme::mono(10) + "color:#5f6c78;");
    rtl->addWidget(tLabel);
    auto *tGrid = new QGridLayout;
    tGrid->setSpacing(4);
    for (int i = 0; i < 3; ++i) m_translationCell[i] = extrinsicCell(rtPanel, tGrid, 0, i);
    rtl->addLayout(tGrid);

    auto *qLabel = new QLabel("quaternion_xyzw", rtPanel);
    qLabel->setStyleSheet(Theme::mono(10) + "color:#5f6c78;margin-top:4px;");
    rtl->addWidget(qLabel);
    auto *qGrid = new QGridLayout;
    qGrid->setSpacing(4);
    for (int i = 0; i < 4; ++i) m_quatCell[i] = extrinsicCell(rtPanel, qGrid, 0, i);
    rtl->addLayout(qGrid);

    auto *exportRow = new QHBoxLayout;
    auto *exportJson = new QPushButton("EXPORT JSON", rtPanel);
    exportJson->setObjectName("accent");
    auto *exportYaml = new QPushButton("EXPORT YAML", rtPanel);
    connect(exportJson, &QPushButton::clicked, this, [this] { emit exportRequested("json"); });
    connect(exportYaml, &QPushButton::clicked, this, [this] { emit exportRequested("yaml"); });
    exportRow->addWidget(exportJson);
    exportRow->addWidget(exportYaml);
    rtl->addLayout(exportRow);

    auto *qualityPanel = new QFrame(this);
    qualityPanel->setObjectName("panel");
    auto *ql = new QVBoxLayout(qualityPanel);
    ql->setContentsMargins(11, 10, 11, 10);
    ql->setSpacing(6);
    auto *qTitle = new QLabel("QUALITY", qualityPanel);
    qTitle->setStyleSheet(Theme::mono(11, 700) + "color:#3fbfcc;letter-spacing:1px;");
    m_edgeRmseValue = new QLabel("0.00 px", qualityPanel);
    m_edgeRmseValue->setStyleSheet(Theme::mono(30, 700) + "color:#4bbd85;");
    auto *edgeCaption = new QLabel("edge RMSE (gate: ≤ 3 px)", qualityPanel);
    edgeCaption->setStyleSheet(Theme::mono(10) + "color:#5f6c78;");
    m_inlierBar = new QProgressBar(qualityPanel);
    m_inlierBar->setRange(0, 100);
    m_inlierBar->setTextVisible(false);
    m_inlierBar->setFixedHeight(5);
    m_inlierBar->setStyleSheet(
        "QProgressBar{background:#161d23;border:none;border-radius:2px;}"
        "QProgressBar::chunk{background:#4bbd85;border-radius:2px;}");

    ql->addWidget(qTitle);
    ql->addWidget(m_edgeRmseValue);
    ql->addWidget(edgeCaption);
    ql->addWidget(m_inlierBar);

    auto *statBox = new QVBoxLayout;
    statBox->setSpacing(4);
    m_statusValue = statRow(qualityPanel, statBox, "status");
    m_inlierValue = statRow(qualityPanel, statBox, "inlier_ratio");
    m_nccValue    = statRow(qualityPanel, statBox, "ncc (진단용)");
    m_retryValue  = statRow(qualityPanel, statBox, "retry");
    ql->addLayout(statBox);

    right->addWidget(rtPanel);
    right->addWidget(qualityPanel);
    right->addStretch(1);

    auto *rightWrap = new QWidget(this);
    rightWrap->setLayout(right);
    rightWrap->setFixedWidth(360);

    auto *leftWrap = new QWidget(this);
    leftWrap->setLayout(left);

    root->addWidget(leftWrap, 1);
    root->addWidget(rightWrap);

    setCalib({});
}

void CalibrationTab::setCalib(const CalibState &c) {
    const int completed = qBound(0, int(std::round(c.progress / 12.5)), 8);
    const bool passed = (c.status == "PASS");
    for (int i = 0; i < 8; ++i) {
        const bool isLast = (i == 7);
        if (i < completed) {
            const bool pass = isLast && passed;
            m_stepBadge[i]->setText(QString::fromUtf8("✓"));
            m_stepBadge[i]->setStyleSheet(QString("border-radius:3px;background:%1;color:%2;")
                .arg(pass ? "#16241d" : "#152229").arg(pass ? "#6fdcab" : "#8fd9e2") + Theme::mono(10, 700));
            m_stepState[i]->setText(pass ? "PASS" : "DONE");
            m_stepState[i]->setStyleSheet(Theme::mono(10, 700) +
                QString("color:%1;").arg(pass ? "#6fdcab" : "#8fd9e2"));
        } else {
            m_stepBadge[i]->setText("");
            m_stepBadge[i]->setStyleSheet("border-radius:3px;background:#161d23;border:1px solid #222c34;");
            m_stepState[i]->setText("PENDING");
            m_stepState[i]->setStyleSheet(Theme::mono(10, 700) + "color:#5f6c78;");
        }
    }

    const bool rmseOk = c.edgeRmsePx > 0.0 && c.edgeRmsePx <= 3.0;
    m_edgeRmseValue->setText(QString("%1 px").arg(c.edgeRmsePx, 0, 'f', 2));
    m_edgeRmseValue->setStyleSheet(Theme::mono(30, 700) +
        QString("color:%1;").arg(rmseOk ? "#4bbd85" : "#ff8175"));
    m_inlierBar->setValue(int(c.inlierRatio * 100));

    m_statusValue->setText(c.status);
    m_statusValue->setStyleSheet(Theme::mono(11) +
        QString("color:%1;").arg(passed ? "#6fdcab" : (c.status == "FAIL" ? "#ff8175" : "#c7d1da")));
    m_inlierValue->setText(QString("%1%").arg(int(c.inlierRatio * 100)));
    m_nccValue->setText(QString::number(c.ncc, 'f', 3));
    m_retryValue->setText(QString("%1 / %2").arg(c.retry).arg(c.maxRetry));

    for (int i = 0; i < 3; ++i) m_translationCell[i]->setText(QString::number(c.translationM[i], 'f', 3));
    for (int i = 0; i < 4; ++i) m_quatCell[i]->setText(QString::number(c.quaternionXyzw[i], 'f', 3));
}

void CalibrationTab::appendLog(const QString &tag, const QString &msg) {
    const QString t = QDateTime::currentDateTime().toString("HH:mm:ss");
    m_log->appendPlainText(QString("%1  %2  %3").arg(t, tag.leftJustified(7), msg));
}
