#pragma once
#include <QTimer>
#include "DataBridge.h"

// 실제 브로커/킷 없이도 대시보드를 그대로 확인할 수 있도록 MQTT_INTERFACE_CONTRACT.md
// v1.0 의 실제 세션 흐름(IDLE→SCANNING→EXPORT→IDLE, state/daemon·state/scan·
// event/progress·event/error)을 그대로 재생하는 데모 소스.
class DemoBridge : public DataBridge {
    Q_OBJECT
public:
    explicit DemoBridge(QObject *parent = nullptr);

    void start() override;
    void stop() override;

public slots:
    void requestScan(int panStartDdeg, int panEndDdeg,
                      int tiltStartDdeg, int tiltEndDdeg,
                      int stepDdeg, int sensorHeightMm) override;
    void requestStop() override;
    void requestHome() override;
    void requestDisarm() override;
    void requestRearm() override;

private slots:
    void tickImu();
    void tickObjects();

private:
    void emitChannelDefaults();
    void emitEdges();
    void runScanScript();
    void publishDaemonState(const QString &state);
    QString newReqId();

    QTimer m_imuTimer;
    QTimer m_objTimer;
    double m_imuPhase = 0.0;
    double m_objPhase = 0.0;
    double m_lastRoll = 0.0, m_lastPitch = 0.0;
    int    m_scanStep = 0;
    QString m_reqId;
    QString m_daemonState = "IDLE";
    bool   m_scanning = false;
    // Live 로 전환된 뒤에도 예약된 singleShot 이 남아 가짜 상태를 쏘는 것을 막는다.
    // stop() 은 QTimer 멤버만 멈출 수 있고 singleShot 은 취소할 수 없기 때문이다.
    bool   m_running = false;
};
