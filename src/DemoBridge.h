#pragma once
#include <QTimer>
#include "DataBridge.h"

// 실제 브로커/펌웨어 없이도 대시보드를 그대로 확인할 수 있도록 가짜 4채널 상태 +
// IMU 드리프트 + WiseAI 객체 + 8단계 캘리브레이션 파이프라인 로그를 재생하는 데모 소스.
// design_handoff_spatial_vms 의 레퍼런스 로그/수치를 그대로 재현한다.
class DemoBridge : public DataBridge {
    Q_OBJECT
public:
    explicit DemoBridge(QObject *parent = nullptr);

    void start() override;
    void stop() override;

public slots:
    void setKitPower(bool on) override;
    void requestRescan() override;

private slots:
    void tickImu();
    void tickObjects();

private:
    void emitChannelDefaults();
    void emitEdges();
    void runCalibScript();

    QTimer m_imuTimer;
    QTimer m_objTimer;
    double m_imuPhase = 0.0;
    double m_objPhase = 0.0;
    int    m_calibStep = 0;
    bool   m_powered = true;
};
