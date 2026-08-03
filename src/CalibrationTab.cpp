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
const char *kSteps[8][2] = {
    {"1 · 360° SCAN",    "천장 중앙 Pan-Tilt 전방위 그리드 스캔 (0.45°/step)"},
    {"2 · FOV CLIP",     "4채널 카메라 FOV(사분면)별 포인트 분할·클리핑"},
    {"3 · DEPTH EDGE",   "실내 벽/기둥 모서리 Score 가중 3D 라인 피팅"},
    {"4 · LSD + NFA",    "채널별 영상 구조선 검출 및 거짓 선분 제거"},
    {"5 · LINE MATCH",   "Plücker 기반 Line-to-Line 대응 + RANSAC (CH1-CH4)"},
    {"6 · LM OPTIMIZE",  "직선 재투영 오차 비선형 최소화"},
    {"7 · 3D MAP BUILD", "4채널 RT 통합 -> 실내 3D 포인트 맵 생성"},
    {"8 · SELF-CHECK",   "NCC 자가 검증 및 자동 재시도 판정"},
};

// 실측 RT 로드(TODO) 전까지 쓰는 레퍼런스 정지값 — design_handoff 시안과 동일.
const char *kRt[12] = {
    "0.9986", "-0.0271", "0.0451", "0.1002",
    "0.0284", "0.9992", "-0.0281", "0.0503",
    "-0.0443", "0.0294", "0.9986", "0.0021",
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
}

CalibrationTab::CalibrationTab(QWidget *parent) : QWidget(parent) {
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(10);

    // ---- 좌측: 파이프라인 + 로그 -----------------------------------------
    auto *left = new QVBoxLayout;
    left->setSpacing(7);

    auto *pipeHead = new QLabel("AUTO-CALIBRATION PIPELINE", this);
    pipeHead->setStyleSheet(Theme::mono(11, 700) + "color:#3fbfcc;letter-spacing:1px;");
    auto *pipeSub = new QLabel(QString::fromUtf8("마커리스 Line-to-Line 정합 진행 상태"), this);
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

    auto *logHead = new QLabel("CALIB LOG", this);
    logHead->setStyleSheet(Theme::mono(11, 700) + "color:#3fbfcc;letter-spacing:1px;margin-top:6px;");
    left->addWidget(logHead);
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    left->addWidget(m_log, 1);

    // ---- 우측: EXTRINSIC RT + QUALITY -------------------------------------
    auto *right = new QVBoxLayout;
    right->setSpacing(10);

    auto *rtPanel = new QFrame(this);
    rtPanel->setObjectName("panel");
    auto *rtl = new QVBoxLayout(rtPanel);
    rtl->setContentsMargins(11, 10, 11, 10);
    rtl->setSpacing(8);
    auto *rtTitle = new QLabel("EXTRINSIC RT [R|T]", rtPanel);
    rtTitle->setStyleSheet(Theme::mono(11, 700) + "color:#3fbfcc;letter-spacing:1px;");
    rtl->addWidget(rtTitle);

    auto *grid = new QGridLayout;
    grid->setSpacing(4);
    for (int i = 0; i < 12; ++i) {
        auto *cell = new QLabel(kRt[i], rtPanel);
        cell->setAlignment(Qt::AlignCenter);
        cell->setStyleSheet(Theme::mono(11) + "color:#c7d1da;background:#101519;border-radius:3px;padding:6px 2px;");
        grid->addWidget(cell, i / 3, i % 3);
    }
    rtl->addLayout(grid);

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
    m_nccValue = new QLabel("0.000", qualityPanel);
    m_nccValue->setStyleSheet(Theme::mono(34, 700) + "color:#4bbd85;");
    m_nccBar = new QProgressBar(qualityPanel);
    m_nccBar->setRange(0, 100);
    m_nccBar->setTextVisible(false);
    m_nccBar->setFixedHeight(5);
    m_nccBar->setStyleSheet(
        "QProgressBar{background:#161d23;border:none;border-radius:2px;}"
        "QProgressBar::chunk{background:#4bbd85;border-radius:2px;}");

    ql->addWidget(qTitle);
    ql->addWidget(m_nccValue);
    ql->addWidget(m_nccBar);

    auto *statBox = new QVBoxLayout;
    statBox->setSpacing(4);
    m_reprojValue = statRow(qualityPanel, statBox, "reproj");
    m_inlierValue = statRow(qualityPanel, statBox, "inlier");
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
    for (int i = 0; i < 8; ++i) {
        const bool isLast = (i == 7);
        if (i < completed) {
            const bool pass = isLast && c.progress >= 100;
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

    m_nccValue->setText(QString::number(c.ncc, 'f', 3));
    m_nccValue->setStyleSheet(Theme::mono(34, 700) +
        QString("color:%1;").arg(c.ncc >= 0.72 ? "#4bbd85" : "#ff8175"));
    m_nccBar->setValue(int(c.ncc * 100));
    m_reprojValue->setText(QString("%1 px").arg(c.reprojPx, 0, 'f', 2));
    m_inlierValue->setText(QString("%1 / %2").arg(c.inliers).arg(c.candidateLines));
    m_retryValue->setText(QString("%1 / %2").arg(c.retry).arg(c.maxRetry));
}

void CalibrationTab::appendLog(const QString &tag, const QString &msg) {
    const QString t = QDateTime::currentDateTime().toString("HH:mm:ss");
    m_log->appendPlainText(QString("%1  %2  %3").arg(t, tag.leftJustified(7), msg));
}
