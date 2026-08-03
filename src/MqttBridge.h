#pragma once
#include <QByteArray>
#include <QString>
#include <QTimer>
#include <memory>
#include <mqtt/async_client.h>
#include "DataBridge.h"

// MQTT 브로커 <-> Qt UI 브리지 (Eclipse Paho MQTT C++).
//  cctv/chN/h264   : 영상 프레임 (JPEG payload 가정 — H.264 면 디코더 연결 지점 표시)
//  wiseai/+/objects: 감지 BBox + class (JSON, 이미 실내 좌표로 변환되어 있다고 가정)
//  kit/lidar/scan  : Depth-Edge 평면 투영 결과 (JSON, {"edges":[{"a":{x,y},"b":{x,y}}, ...]})
//  kit/imu/level   : MPU6050 roll/pitch (JSON)
//  kit/calib/status: 캘리브레이션 진행 상태 (JSON)
//  kit/cmd/power   : 킷 전원 제어 (publish)
//  kit/cmd/rescan  : 자동/수동 재스캔 명령 (publish)
class MqttBridge : public DataBridge, public virtual mqtt::callback {
    Q_OBJECT
public:
    explicit MqttBridge(QObject *parent = nullptr);
    ~MqttBridge() override;

    void start() override;   // no-op — connectToBroker() 로 명시적으로 연결
    void stop() override;

    void connectToBroker(const QString &host, quint16 port);
    void disconnectFromBroker();

public slots:
    void setKitPower(bool on) override;
    void requestRescan() override;

private:
    // mqtt::callback — Paho 내부 네트워크 스레드에서 호출됨
    void connected(const std::string &cause) override;
    void connection_lost(const std::string &cause) override;
    void message_arrived(mqtt::const_message_ptr msg) override;
    void delivery_complete(mqtt::delivery_token_ptr token) override;

    void subscribeAll();
    void publish(const QString &topic, const QByteArray &payload);

    // GUI 스레드에서 안전하게 실행되는 실제 파싱/타이머 처리부
    void onRawMessage(const QString &topic, const QByteArray &payload);
    void handleFrame(int ch, const QByteArray &payload);
    void handleImu(const QByteArray &payload);
    void handleObjects(const QByteArray &payload);
    void handleLidarScan(const QByteArray &payload);
    void handleCalib(const QByteArray &payload);

    std::unique_ptr<mqtt::async_client> m_client;
    QString  m_host;
    quint16  m_port = 1883;
    QTimer  *m_timeout[4] = {nullptr, nullptr, nullptr, nullptr};
    qint64   m_lastFrameAtMs[4] = {0, 0, 0, 0};
};
