#include "CalibrationTab.h"
#include "Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QLabel>
#include <QFrame>
#include <QProgressBar>
#include <QDateTime>
#include <QLocale>

namespace {
// 계약이 실제로 규정하는 FSM(§5)만 반영한다 — 추측성 알고리즘 단계는 넣지 않는다.
const char *kSteps[4][2] = {
    {"1 · IDLE",     "대기 — cmd/scan 수신 시 SCANNING 진입 (수평 게이트 통과 필요)"},
    {"2 · SCANNING", "STM32 연속 pan sweep, event/progress ~2Hz 수신"},
    {"3 · EXPORT",   "포인트클라우드 파일 마감, state/scan 발행"},
    {"4 · IDLE",     "세션 종료 — 다음 cmd/scan 대기"},
};

QLabel *statRow(QWidget *parent, QVBoxLayout *into, const QString &label) {
    auto *row = new QHBoxLayout;
    auto *k = new QLabel(label, parent);
    k->setStyleSheet(Theme::mono(10) + QString("color:%1;").arg(Theme::TextFaint.name()));
    auto *v = new QLabel(parent);
    v->setStyleSheet(Theme::mono(11) + QString("color:%1;").arg(Theme::Text3.name()));
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

    // ---- 좌측: 세션 흐름 + 로그 -----------------------------------------
    auto *left = new QVBoxLayout;
    left->setSpacing(7);

    auto *pipeHead = new QLabel("SCAN SESSION", this);
    pipeHead->setStyleSheet(Theme::mono(11, 700) + QString("color:%1;letter-spacing:1px;").arg(Theme::Accent.name()));
    auto *pipeSub = new QLabel(QString::fromUtf8("MQTT_INTERFACE_CONTRACT.md §5 상태 흐름"), this);
    pipeSub->setStyleSheet(QString("color:%1;font-size:11px;").arg(Theme::TextDim.name()));
    left->addWidget(pipeHead);
    left->addWidget(pipeSub);
    left->addSpacing(4);

    for (int i = 0; i < 4; ++i) {
        auto *row = new QFrame(this);
        row->setObjectName("panel");
        auto *rl = new QHBoxLayout(row);
        rl->setContentsMargins(11, 8, 11, 8);
        rl->setSpacing(11);

        m_stepBadge[i] = new QLabel(row);
        m_stepBadge[i]->setFixedSize(16, 16);
        m_stepBadge[i]->setAlignment(Qt::AlignCenter);

        auto *id = new QLabel(kSteps[i][0], row);
        id->setFixedWidth(120);
        id->setStyleSheet(Theme::mono(11, 500) + QString("color:%1;").arg(Theme::Text2.name()));
        auto *desc = new QLabel(kSteps[i][1], row);
        desc->setStyleSheet(QString("color:%1;").arg(Theme::TextDim.name()));

        m_stepState[i] = new QLabel("—", row);
        m_stepState[i]->setStyleSheet(Theme::mono(10, 700) + QString("color:%1;").arg(Theme::TextFaint.name()));

        rl->addWidget(m_stepBadge[i]);
        rl->addWidget(id);
        rl->addWidget(desc, 1);
        rl->addWidget(m_stepState[i]);
        left->addWidget(row);
    }

    auto *logHead = new QLabel("SESSION LOG", this);
    logHead->setStyleSheet(Theme::mono(11, 700) + QString("color:%1;letter-spacing:1px;margin-top:6px;").arg(Theme::Accent.name()));
    left->addWidget(logHead);
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    left->addWidget(m_log, 1);

    // ---- 우측: 진행률 + 마지막 결과 -----------------------------------------
    auto *right = new QVBoxLayout;
    right->setSpacing(10);

    auto *progPanel = new QFrame(this);
    progPanel->setObjectName("panel");
    auto *pl = new QVBoxLayout(progPanel);
    pl->setContentsMargins(11, 10, 11, 10);
    pl->setSpacing(6);
    auto *pTitle = new QLabel("PROGRESS", progPanel);
    pTitle->setStyleSheet(Theme::mono(11, 700) + QString("color:%1;letter-spacing:1px;").arg(Theme::Accent.name()));
    m_stateValue = new QLabel("OFFLINE", progPanel);
    m_stateValue->setStyleSheet(Theme::mono(24, 700) + QString("color:%1;").arg(Theme::TextFaint.name()));
    m_progressBar = new QProgressBar(progPanel);
    m_progressBar->setRange(0, 100);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(5);
    m_progressBar->setStyleSheet(
        QString("QProgressBar{background:%1;border:none;border-radius:2px;}"
                "QProgressBar::chunk{background:%2;border-radius:2px;}")
            .arg(Theme::BorderRow.name(), Theme::Accent.name()));
    pl->addWidget(pTitle);
    pl->addWidget(m_stateValue);
    pl->addWidget(m_progressBar);

    auto *progStats = new QVBoxLayout;
    progStats->setSpacing(4);
    m_pointsValue   = statRow(progPanel, progStats, "points");
    m_expectedValue = statRow(progPanel, progStats, "expected");
    pl->addLayout(progStats);

    auto *resultPanel = new QFrame(this);
    resultPanel->setObjectName("panel");
    auto *rl2 = new QVBoxLayout(resultPanel);
    rl2->setContentsMargins(11, 10, 11, 10);
    rl2->setSpacing(6);
    auto *rTitle = new QLabel("LAST SCAN RESULT  (state/scan)", resultPanel);
    rTitle->setStyleSheet(Theme::mono(11, 700) + QString("color:%1;letter-spacing:1px;").arg(Theme::Accent.name()));
    rl2->addWidget(rTitle);
    auto *resultStats = new QVBoxLayout;
    resultStats->setSpacing(4);
    // 실구현(develop 브랜치)이 실제로 보내는 필드부터: ok/points/stm_reported/pcd.
    m_okValue          = statRow(resultPanel, resultStats, "ok");
    m_stmReportedValue = statRow(resultPanel, resultStats, "stm_reported");
    m_pcdValue         = statRow(resultPanel, resultStats, "pcd");
    // 계약 §3.4엔 있지만 실구현이 아직 안 보내는 필드 — 오면 채워지고, 안 오면
    // setScanResult()가 "—"로 표시한다(0/빈 문자열을 실제 값처럼 보이지 않게).
    m_sessionValue  = statRow(resultPanel, resultStats, "session_id");
    m_scanIdValue   = statRow(resultPanel, resultStats, "scan_id");
    m_rowsColsValue = statRow(resultPanel, resultStats, "rows × columns");
    m_durationValue = statRow(resultPanel, resultStats, "duration_s");
    m_jsonValue     = statRow(resultPanel, resultStats, "json");
    rl2->addLayout(resultStats);

    right->addWidget(progPanel);
    right->addWidget(resultPanel);
    right->addStretch(1);

    auto *rightWrap = new QWidget(this);
    rightWrap->setLayout(right);
    rightWrap->setFixedWidth(360);

    auto *leftWrap = new QWidget(this);
    leftWrap->setLayout(left);

    root->addWidget(leftWrap, 1);
    root->addWidget(rightWrap);

    setDaemonState({});
    setScanProgress({});
}

void CalibrationTab::setDaemonState(const DaemonState &s) {
    m_stateValue->setText(s.state);
    const QString color =
        (s.state == "IDLE")     ? Theme::Ok.name() :
        (s.state == "SCANNING") ? Theme::AccentBright.name() :
        (s.state == "EXPORT")   ? Theme::Warn.name() :
        (s.state == "DISARM")   ? Theme::DangerText.name() : Theme::TextFaint.name();
    m_stateValue->setStyleSheet(Theme::mono(24, 700) + QString("color:%1;").arg(color));

    int active = -1;
    if (s.state == "IDLE") active = 0;
    else if (s.state == "SCANNING") active = 1;
    else if (s.state == "EXPORT") active = 2;

    for (int i = 0; i < 4; ++i) {
        if (i == active) {
            m_stepBadge[i]->setText(QString::fromUtf8("●"));
            m_stepBadge[i]->setStyleSheet(QString("border-radius:3px;background:%1;color:%2;")
                .arg(Theme::AccentBg.name(), Theme::AccentBright.name()) + Theme::mono(10, 700));
            m_stepState[i]->setText("ACTIVE");
            m_stepState[i]->setStyleSheet(Theme::mono(10, 700) + QString("color:%1;").arg(Theme::AccentBright.name()));
        } else if (active >= 0 && i < active) {
            m_stepBadge[i]->setText(QString::fromUtf8("✓"));
            m_stepBadge[i]->setStyleSheet(QString("border-radius:3px;background:%1;color:%2;")
                .arg(Theme::OkBg.name(), Theme::OkBright.name()) + Theme::mono(10, 700));
            m_stepState[i]->setText("DONE");
            m_stepState[i]->setStyleSheet(Theme::mono(10, 700) + QString("color:%1;").arg(Theme::OkBright.name()));
        } else {
            m_stepBadge[i]->setText("");
            m_stepBadge[i]->setStyleSheet(QString("border-radius:3px;background:%1;border:1px solid %2;")
                .arg(Theme::BorderRow.name(), Theme::Border.name()));
            m_stepState[i]->setText("—");
            m_stepState[i]->setStyleSheet(Theme::mono(10, 700) + QString("color:%1;").arg(Theme::TextFaint.name()));
        }
    }
}

void CalibrationTab::setScanProgress(const ScanProgress &p) {
    m_progressBar->setValue(p.percent);
    m_pointsValue->setText(QLocale().toString(p.points));
    m_expectedValue->setText(QLocale().toString(p.expected));
}

void CalibrationTab::setScanResult(const ScanResult &r) {
    m_progressBar->setValue(100);

    m_okValue->setText(r.ok ? "true" : "false");
    m_okValue->setStyleSheet(Theme::mono(11) + QString("color:%1;")
                                  .arg((r.ok ? Theme::Ok : Theme::DangerText).name()));
    m_stmReportedValue->setText(r.stmReported > 0 ? QLocale().toString(r.stmReported) : "—");
    m_pcdValue->setText(r.pcdPath.isEmpty() ? "—" : r.pcdPath);

    // 계약엔 있지만 실구현이 아직 안 보내는 필드 — 기본값(빈 문자열/0)이면 "—".
    m_sessionValue->setText(r.sessionId.isEmpty() ? "—" : r.sessionId);
    m_scanIdValue->setText(r.scanId.isEmpty() ? "—" : r.scanId);
    m_rowsColsValue->setText((r.rows > 0 && r.columns > 0)
                                  ? QString("%1 × %2").arg(r.rows).arg(r.columns) : "—");
    m_durationValue->setText(r.durationS > 0.0 ? QString("%1 s").arg(r.durationS, 0, 'f', 1) : "—");
    m_jsonValue->setText(r.jsonPath.isEmpty() ? "—" : r.jsonPath);
}

void CalibrationTab::appendLog(const QString &tag, const QString &msg) {
    const QString t = QDateTime::currentDateTime().toString("HH:mm:ss");
    m_log->appendPlainText(QString("%1  %2  %3").arg(t, tag.leftJustified(7), msg));
}
