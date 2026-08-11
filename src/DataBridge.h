#pragma once
#include <QObject>
#include <QImage>
#include <QVector>
#include "Models.h"

// MQTT 브로커(MqttBridge)와 데모 시뮬레이터(DemoBridge)가 공유하는 신호 인터페이스.
// MainWindow 및 각 탭은 이 시그널 표면만 알면 되고, 실제로 어느 쪽이 연결되어 있는지는
// 신경 쓰지 않는다.
//
// 명령/상태 신호는 MQTT_INTERFACE_CONTRACT.md v1.0 을 기준으로 하되, 토픽은
// RPi develop 브랜치 실구현대로 kit_id 세그먼트 없는 "adts/..." 를 쓴다
// (Models.h 상단 주석 참고).
class DataBridge : public QObject {
    Q_OBJECT
public:
    explicit DataBridge(QObject *parent = nullptr) : QObject(parent) {}
    ~DataBridge() override = default;

    virtual void start() = 0;   // 데이터 발생 시작(데모: 타이머 시작 / 라이브: 아무 것도 안 함)
    virtual void stop() = 0;    // 데이터 발생 중단

public slots:
    // cmd/scan (계약 §3.1). 각도는 0.1도(deci-degree) 단위, 기구각.
    virtual void requestScan(int panStartDdeg, int panEndDdeg,
                              int tiltStartDdeg, int tiltEndDdeg,
                              int stepDdeg, int sensorHeightMm) = 0;
    virtual void requestStop() = 0;     // cmd/stop
    virtual void requestHome() = 0;     // cmd/home (데몬 쪽 TODO — 보통 불필요, §3.2)
    virtual void requestDisarm() = 0;   // cmd/disarm — 항상 발행 가능해야 한다(비상정지)
    virtual void requestRearm() = 0;    // cmd/rearm — DISARM -> IDLE 복구. 복구 가능 여부
                                         // (링크 생존 여부 등)는 데몬이 판정한다.

signals:
    void brokerStateChanged(bool connected);
    void frameReceived(int channel, const QImage &frame);
    void channelStatusChanged(int channel, bool online, double fps);
    void imuUpdated(const ImuState &imu);
    void objectsUpdated(const QVector<SpatialObject> &objects);
    void mapEdgesUpdated(const QVector<MapEdge> &edges);

    void daemonStateUpdated(const DaemonState &state);     // state/daemon (retained, LWT)
    void scanResultUpdated(const ScanResult &result);      // state/scan (retained)
    void scanProgressUpdated(const ScanProgress &progress);// event/progress (~2Hz)
    void kitErrorReceived(const KitError &error);          // event/error

    void logLine(const QString &tag, const QString &msg);
};
