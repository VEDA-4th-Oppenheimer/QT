#pragma once
#include <QByteArray>
#include <QString>
#include <QJsonObject>
#include <QTimer>
#include <memory>
#include <mqtt/async_client.h>
#include "DataBridge.h"

// MQTT 브로커 <-> Qt UI 브리지 (Eclipse Paho MQTT C++).
// 계약: MQTT_INTERFACE_CONTRACT.md v1.0 문서 기준이되, 실제로는 RPi 저장소
// develop 브랜치의 진짜 구현(daemon/modules/mqtt/mqtt_module.c, 이현우)에 맞췄다.
// 문서와 실구현이 갈리는 지점(토픽에 kit_id 없음, state/scan 필드 축소)은
// Models.h 상단 주석 참고 — 재확정되면 이 파일과 함께 고칠 것.
//
//   토픽(전부 adts/... 접두, kit_id 세그먼트 없음): cmd/{scan,stop,home,disarm} 발행,
//                                  state/{daemon,scan}, event/{progress,error} 구독.
//   포트 8883 + mTLS(클라이언트 인증서, TLS 1.2 — 데몬 쪽과 동일하게 고정).
//   인증서가 아직 없으면(§6, 발급 전) tcp:// 평문 1883 으로 degraded 접속 — 로컬
//   개발/데모용. 실제 인증서가 배치되면 자동으로 ssl:// 를 쓴다.
//   Client ID 는 계약서 §1 대로 고정 "qt-console" (중복 접속하면 서로 끊긴다).
//   데몬 쪽 Client ID 는 "adts-daemon" — 서로 다르므로 충돌 없음.
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

signals:
    // 브로커가 retained 로 내려주는 카메라 설정. 카메라는 사용자별 자산이 아니라
    // 킷의 일부라, 관리자가 한 곳에서 바꾸면 접속 중인 콘솔 전부에 반영된다.
    // (등록 시점에 받은 config/cameras.json 은 브로커 연결 전 초기값으로 남는다)
    void cameraConfigReceived(const QJsonObject &channels);

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
    void attemptConnect();   // 최초 접속 실패 시 재시도용 (아래 m_retryTimer 참고)

    // GUI 스레드에서 안전하게 실행되는 실제 파싱 처리부
    void onRawMessage(const QString &topic, const QByteArray &payload);
    void handleStateDaemon(const QByteArray &payload);
    void handleStateScan(const QByteArray &payload);
    void handleEventProgress(const QByteArray &payload);
    void handleEventError(const QByteArray &payload);
    void handleConfigCameras(const QByteArray &payload);
    bool acceptsReqId(const QString &incoming) const;   // 내가 보낸 req_id 아니면 무시(계약 §4)

    std::unique_ptr<mqtt::async_client> m_client;
    QString  m_host;
    quint16  m_port = 1883;
    QString  m_lastReqId;   // 가장 최근에 Qt 가 발행한 req_id

    // Paho 의 set_automatic_reconnect(true) 는 "한 번 붙었다가 끊긴" 경우만
    // 다시 붙여준다. 최초 connect() 자체가 실패하면(앱을 먼저 켜고 RPi 를
    // 나중에 켠 경우) 재시도도 없고 connection_lost() 콜백도 안 온다 —
    // 그래서 브로커가 나중에 올라와도 영원히 미연결로 남았다. 아래 타이머가
    // 연결될 때까지 주기적으로 다시 시도한다.
    QTimer   m_retryTimer;
    mqtt::connect_options m_connOpts;
    bool     m_wantConnected = false;
    bool     m_loggedFailure = false;   // 재시도 로그 도배 방지
};
