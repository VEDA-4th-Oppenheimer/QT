#pragma once
#include <QWidget>
#include "Models.h"

class QPlainTextEdit;
class QLabel;
class QProgressBar;

// CALIBRATION 탭: 스캔 세션 상태 + 진행률 + 로그.
// MQTT_INTERFACE_CONTRACT.md v1.0 은 스캔 제어/상태만 규정한다 — 카메라 단 캘리브
// 결과(NCC/edge_rmse/extrinsic)는 아직 발행 토픽이 정해지지 않아(§9 미결) 여기서
// 다루지 않는다. 그 데이터가 오는 토픽이 정해지면 이 탭에 QUALITY 패널을 추가한다.
class CalibrationTab : public QWidget {
    Q_OBJECT
public:
    explicit CalibrationTab(QWidget *parent = nullptr);

    void setDaemonState(const DaemonState &s);
    void setScanProgress(const ScanProgress &p);
    void setScanResult(const ScanResult &r);
    void appendLog(const QString &tag, const QString &msg);

private:
    QLabel *m_stepBadge[4];
    QLabel *m_stepState[4];
    QPlainTextEdit *m_log;

    QLabel *m_stateValue;
    QProgressBar *m_progressBar;
    QLabel *m_pointsValue, *m_expectedValue;
    QLabel *m_sessionValue, *m_scanIdValue, *m_pcdValue, *m_jsonValue;
    QLabel *m_rowsColsValue, *m_durationValue;
};
