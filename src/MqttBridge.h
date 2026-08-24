#pragma once
#include <QByteArray>
#include <QString>
#include <QJsonObject>
#include <QTimer>
#include <memory>
#include "DataBridge.h"

#if defined(USE_PAHO_MQTT) && !defined(Q_MOC_RUN)
#include <mqtt/async_client.h>
#endif

// MQTT 브로커 <-> Qt UI 브리지 (Eclipse Paho MQTT C++).
class MqttBridge : public DataBridge
#if defined(USE_PAHO_MQTT) && !defined(Q_MOC_RUN)
                 , public virtual mqtt::callback
                 , public virtual mqtt::iaction_listener
#endif
{
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
#ifdef USE_PAHO_MQTT
    // mqtt::callback — Paho 내부 네트워크 스레드에서 호출됨
    void connected(const std::string &cause) override;
    void connection_lost(const std::string &cause) override;

    // iaction_listener — connect() 토큰 결과. Paho 네트워크 스레드에서 불린다.
    void on_failure(const mqtt::token &tok) override;
    void on_success(const mqtt::token &tok) override;
    void message_arrived(mqtt::const_message_ptr msg) override;
    void delivery_complete(mqtt::delivery_token_ptr token) override;

    std::unique_ptr<mqtt::async_client> m_client;
    mqtt::connect_options m_connOpts;
#endif

    void subscribeAll();
    void publishCommand(const QString &topic, const QJsonObject &fields);
    QString newReqId();
    void attemptConnect();   // 최초 접속 실패 시 재시도용

    // GUI 스레드에서 안전하게 실행되는 실제 파싱 처리부
    void onRawMessage(const QString &topic, const QByteArray &payload);
    void handleStateDaemon(const QByteArray &payload);
    void handleStateScan(const QByteArray &payload);
    void handleEventProgress(const QByteArray &payload);
    void handleEventError(const QByteArray &payload);
    bool acceptsReqId(const QString &incoming) const;

    QString  m_host;
    quint16  m_port = 1883;
    QString  m_lastReqId;
    QTimer   m_retryTimer;
    bool     m_wantConnected = false;
    bool     m_loggedFailure = false;
};
