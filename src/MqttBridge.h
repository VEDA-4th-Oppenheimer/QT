#pragma once
#include <QByteArray>
#include <QString>
#include <QJsonObject>
#include <memory>
#include <mqtt/async_client.h>
#include "DataBridge.h"

// MQTT 브로커 <-> Qt UI 브리지 (Eclipse Paho MQTT C++).
// 계약: docs 없이도 여기 주석만으로 알 수 있게 — MQTT_INTERFACE_CONTRACT.md v1.0
// (RPi 저장소 docs/, 데몬=이현우 / Qt=송영빈 / 브로커·인증서=이광진 서명)을 그대로 구현한다.
//
//   토픽(전부 adts/kit1/... 접두): cmd/{scan,stop,home,disarm} 발행,
//                                  state/{daemon,scan}, event/{progress,error} 구독.
//   포트 8883 + mTLS(클라이언트 인증서). 인증서가 아직 없으면(§6, 발급 전) tcp:// 평문
//   1883 으로 degraded 접속 — 로컬 개발/데모용. 실제 인증서가 배치되면 자동으로 ssl:// 를 쓴다.
//   Client ID 는 계약서 §1 대로 고정 "qt-console" (중복 접속하면 서로 끊긴다).
//
// 영상은 이 브릿지가 아니라 RtspSource 가 카메라에서 RTSP 로 직접 받는다(MQTT 경로 아님).
class MqttBridge : public DataBridge, public virtual mqtt::callback {
    Q_OBJECT
public:
    explicit MqttBridge(QObject *parent = nullptr);
    ~MqttBridge() override;

    void start() override;   // no-op — connectToBroker() 로 명시적으로 연결
    void stop() override;

    // certDir 에 ca.crt/qt-console.crt/qt-console.key 가 모두 있으면 ssl://(mTLS)로,
    // 없으면 tcp:// 평문으로 degraded 접속한다(로컬 개발용, 계약서 §6 인증서 배치 전).
    void connectToBroker(const QString &host, quint16 port, const QString &certDir = QString());
    void disconnectFromBroker();

public slots:
    void requestScan(int panStartDdeg, int panEndDdeg,
                      int tiltStartDdeg, int tiltEndDdeg,
                      int stepDdeg, int sensorHeightMm) override;
    void requestStop() override;
    void requestHome() override;
    void requestDisarm() override;
    void requestRearm() override;

private:
    // mqtt::callback — Paho 내부 네트워크 스레드에서 호출됨
    void connected(const std::string &cause) override;
    void connection_lost(const std::string &cause) override;
    void message_arrived(mqtt::const_message_ptr msg) override;
    void delivery_complete(mqtt::delivery_token_ptr token) override;

    void subscribeAll();
    void publishCommand(const QString &topic, const QJsonObject &fields);
    QString newReqId();

    // GUI 스레드에서 안전하게 실행되는 실제 파싱 처리부
    void onRawMessage(const QString &topic, const QByteArray &payload);
    void handleStateDaemon(const QByteArray &payload);
    void handleStateScan(const QByteArray &payload);
    void handleEventProgress(const QByteArray &payload);
    void handleEventError(const QByteArray &payload);
    bool acceptsReqId(const QString &incoming) const;   // 내가 보낸 req_id 아니면 무시(계약 §4)

    std::unique_ptr<mqtt::async_client> m_client;
    QString  m_host;
    quint16  m_port = 1883;
    QString  m_lastReqId;   // 가장 최근에 Qt 가 발행한 req_id
};
