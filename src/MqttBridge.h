#pragma once
#include <QByteArray>
#include <QString>
#include <memory>
#include <mqtt/async_client.h>
#include "DataBridge.h"

// MQTT 브로커 <-> Qt UI 브리지 (Eclipse Paho MQTT C++).
// 브로커는 RPi 에 상주(Mosquitto, MQTT-over-TLS 8883)하며 Qt·카메라 단·통합 데몬이 모두
// 이 브로커의 클라이언트다 ("Device 파트 아키텍처 및 역할 분담 V2" 기준).
//
// 확정: scan/start·scan/stop (발행) — RPi 데몬 FSM 트리거 (protocol.h CMD_SCAN_START/STOP)
//       scan/status·scan/done (구독) — 스캔 진행률 / 완료(포인트클라우드 파일 경로)
// 미정(팀 협의 중, "MQTT 토픽 스키마 확정" 항목): 카메라 단(이영민) 캘리브 결과·실좌표
//       발행 토픽명. 아래 calib/result, calib/objects, imu/level 은 확정 전까지의
//       placeholder 이며, 실제 스키마가 정해지면 이 파일만 고치면 된다.
//
// 영상은 이 브릿지가 아니라 RtspSource 가 카메라에서 RTSP 로 직접 받는다
// (아키텍처 V2: PNM-C16083RVQ --RTSP/SUNAPI--> Pi/Qt, MQTT 경로 아님).
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
    void setKitPower(bool on) override;   // 실제 프로토콜엔 "전원" 개념이 없음 — 로그만 남김(TODO)
    void requestRescan() override;        // scan/start 발행 (CMD_SCAN_START)

private:
    // mqtt::callback — Paho 내부 네트워크 스레드에서 호출됨
    void connected(const std::string &cause) override;
    void connection_lost(const std::string &cause) override;
    void message_arrived(mqtt::const_message_ptr msg) override;
    void delivery_complete(mqtt::delivery_token_ptr token) override;

    void subscribeAll();
    void publish(const QString &topic, const QByteArray &payload);

    // GUI 스레드에서 안전하게 실행되는 실제 파싱 처리부
    void onRawMessage(const QString &topic, const QByteArray &payload);
    void handleScanStatus(const QByteArray &payload);
    void handleScanDone(const QByteArray &payload);
    void handleCalibResult(const QByteArray &payload);   // placeholder 토픽 (TODO)
    void handleImu(const QByteArray &payload);           // placeholder 토픽 (TODO)
    void handleObjects(const QByteArray &payload);        // placeholder 토픽 (TODO)

    std::unique_ptr<mqtt::async_client> m_client;
    QString  m_host;
    quint16  m_port = 1883;
    CalibState m_calib;   // scan/status, scan/done, calib/result 를 누적 반영
};
