#include "MqttBridge.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QUuid>
#include <QSysInfo>
#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QMetaObject>
#include <cmath>

namespace {
constexpr int kQosCmd = 1;
constexpr int kQosBestEffort = 0;
constexpr int kKeepAliveSec = 30;
constexpr int kReconnectRetryMs = 5000;   // 최초 접속 실패 시 재시도 주기

// kit_id 세그먼트 없음 — RPi develop 브랜치 실구현 기준(Models.h 상단 주석 참고).
constexpr char kTopicCmdScan[]       = "adts/cmd/scan";
constexpr char kTopicCmdStop[]       = "adts/cmd/stop";
constexpr char kTopicCmdHome[]       = "adts/cmd/home";
constexpr char kTopicCmdDisarm[]     = "adts/cmd/disarm";
constexpr char kTopicCmdRearm[]      = "adts/cmd/rearm";
constexpr char kTopicStateWildcard[] = "adts/state/#";
constexpr char kTopicEventWildcard[] = "adts/event/#";
constexpr char kTopicStateDaemon[]   = "adts/state/daemon";
constexpr char kTopicStateScan[]     = "adts/state/scan";
constexpr char kTopicEventProgress[] = "adts/event/progress";
constexpr char kTopicEventError[]    = "adts/event/error";

QDateTime tsFromUnixSeconds(qint64 secs) {
    return secs > 0 ? QDateTime::fromSecsSinceEpoch(secs) : QDateTime::currentDateTime();
}
}

MqttBridge::MqttBridge(QObject *parent) : DataBridge(parent) {
    m_retryTimer.setInterval(kReconnectRetryMs);
    connect(&m_retryTimer, &QTimer::timeout, this, [this] {
        if (!m_wantConnected) { m_retryTimer.stop(); return; }
#ifdef USE_PAHO_MQTT
        if (m_client && m_client->is_connected()) { m_retryTimer.stop(); return; }
#endif
        attemptConnect();
    });
}

MqttBridge::~MqttBridge() { disconnectFromBroker(); }

void MqttBridge::start() { /* 라이브 브리지는 connectToBroker() 로 명시적으로 시작한다 */ }

void MqttBridge::stop() { disconnectFromBroker(); }

void MqttBridge::connectToBroker(const QString &host, quint16 port, const QString &certDir) {
    m_host = host;
    m_port = port;

#ifdef USE_PAHO_MQTT
    const QString caCert     = certDir.isEmpty() ? QString() : certDir + "/ca.crt";
    const QString clientCert = certDir.isEmpty() ? QString() : certDir + "/qt-console.crt";
    const QString clientKeyPlain = certDir.isEmpty() ? QString() : certDir + "/qt-console.key";
    const QString clientKeyTrad  = certDir.isEmpty() ? QString() : certDir + "/qt-console-trad.key";
    const QString clientKey = QFile::exists(clientKeyPlain) ? clientKeyPlain : clientKeyTrad;
    const bool haveCerts = !certDir.isEmpty()
        && QFile::exists(caCert) && QFile::exists(clientCert) && QFile::exists(clientKey);

    const std::string scheme = haveCerts ? "ssl://" : "tcp://";
    const std::string uri = scheme + QStringLiteral("%1:%2").arg(host).arg(port).toStdString();

    QString hostTag = QSysInfo::machineHostName();
    hostTag.remove(QRegularExpression(QStringLiteral("[^A-Za-z0-9-]")));
    if (hostTag.isEmpty()) hostTag = QStringLiteral("host");
    const QString clientId = QStringLiteral("qt-console-%1-%2")
        .arg(hostTag.left(12),
             QUuid::createUuid().toString(QUuid::Id128).left(4));

    m_client = std::make_unique<mqtt::async_client>(uri, clientId.toStdString());
    m_client->set_callback(*this);

    mqtt::connect_options opts;
    opts.set_clean_session(true);
    opts.set_keep_alive_interval(kKeepAliveSec);
    opts.set_automatic_reconnect(true);

    if (haveCerts) {
        mqtt::ssl_options ssl;
        ssl.set_trust_store(caCert.toStdString());
        ssl.set_key_store(clientCert.toStdString());
        ssl.set_private_key(clientKey.toStdString());
        ssl.set_ssl_version(3 /*MQTT_SSL_VERSION_TLS_1_2*/);
        opts.set_ssl(ssl);
        emit logLine("MQTT", QString("mTLS 인증서 로드됨 (%1) — ssl://%2:%3").arg(certDir, host).arg(port));
    } else {
        emit logLine("MQTT", QString("인증서 없음(%1) — 평문 tcp://%2:%3 로 degraded 접속 "
                                      "(계약 §6 인증서 배치 전 로컬 개발용)").arg(certDir, host).arg(port));
    }

    m_connOpts = opts;
    m_wantConnected = true;
    m_loggedFailure = false;
    attemptConnect();
    m_retryTimer.start();
#else
    emit logLine("MQTT", QString("MQTT 클라이언트 미내장(빌드 옵션) — host:%1 port:%2").arg(host).arg(port));
    emit brokerStateChanged(false);
#endif
}

void MqttBridge::attemptConnect() {
#ifdef USE_PAHO_MQTT
    if (!m_client || m_client->is_connected()) return;
    try {
        m_client->connect(m_connOpts, nullptr, *this);
    } catch (const mqtt::exception &exc) {
        emit brokerStateChanged(false);
        if (!m_loggedFailure) {
            m_loggedFailure = true;
            qWarning() << "MQTT connect failed:" << exc.what();
            emit logLine("MQTT", QString("연결 실패: %1 — %2초마다 재시도")
                                      .arg(exc.what()).arg(kReconnectRetryMs / 1000));
        }
    }
#endif
}

#ifdef USE_PAHO_MQTT
void MqttBridge::on_failure(const mqtt::token &tok) {
    const int rc = tok.get_return_code();
    QMetaObject::invokeMethod(this, [this, rc] {
        emit brokerStateChanged(false);
        if (!m_loggedFailure) {
            m_loggedFailure = true;
            emit logLine("MQTT", QString("연결 실패 (rc=%1) — %2초마다 재시도")
                                      .arg(rc).arg(kReconnectRetryMs / 1000));
        }
    }, Qt::QueuedConnection);
}

void MqttBridge::on_success(const mqtt::token &) {
}
#endif

void MqttBridge::disconnectFromBroker() {
    m_wantConnected = false;
    m_retryTimer.stop();
#ifdef USE_PAHO_MQTT
    if (!m_client || !m_client->is_connected()) return;
    try {
        m_client->disconnect()->wait();
    } catch (const mqtt::exception &exc) {
        qWarning() << "MQTT disconnect failed:" << exc.what();
    }
#endif
}

QString MqttBridge::newReqId() {
    m_lastReqId = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    return m_lastReqId;
}

bool MqttBridge::acceptsReqId(const QString &incoming) const {
    return incoming.isEmpty() || m_lastReqId.isEmpty() || incoming == m_lastReqId;
}

void MqttBridge::publishCommand(const QString &topic, const QJsonObject &fields) {
#ifdef USE_PAHO_MQTT
    if (!m_client || !m_client->is_connected()) {
        emit logLine("MQTT", QString("브로커 미연결 — %1 발행 실패").arg(topic));
        return;
    }
    const QByteArray payload = QJsonDocument(fields).toJson(QJsonDocument::Compact);
    try {
        m_client->publish(topic.toStdString(), payload.constData(), payload.size(),
                           kQosCmd, /*retained=*/false);
    } catch (const mqtt::exception &exc) {
        qWarning() << "MQTT publish failed on" << topic << ":" << exc.what();
    }
#else
    emit logLine("MQTT", QString("명령 발행(stub) — %1").arg(topic));
#endif
}

void MqttBridge::requestScan(int panStartDdeg, int panEndDdeg,
                              int tiltStartDdeg, int tiltEndDdeg,
                              int stepDdeg, int sensorHeightMm) {
    const QString reqId = newReqId();
    QJsonObject o;
    o["req_id"] = reqId;
    o["pan_ddeg"] = QJsonArray{panStartDdeg, panEndDdeg};
    o["tilt_ddeg"] = QJsonArray{tiltStartDdeg, tiltEndDdeg};
    o["step_ddeg"] = stepDdeg;
    o["sensor_height_mm"] = sensorHeightMm;
    publishCommand(kTopicCmdScan, o);
    emit logLine("SCAN", QString("cmd/scan 발행 (req_id=%1) pan[%2..%3] tilt[%4..%5] step=%6")
                              .arg(reqId).arg(panStartDdeg).arg(panEndDdeg)
                              .arg(tiltStartDdeg).arg(tiltEndDdeg).arg(stepDdeg));
}

void MqttBridge::requestStop() {
    const QString reqId = newReqId();
    publishCommand(kTopicCmdStop, {{"req_id", reqId}});
    emit logLine("SCAN", QString("cmd/stop 발행 (req_id=%1)").arg(reqId));
}

void MqttBridge::requestHome() {
    const QString reqId = newReqId();
    publishCommand(kTopicCmdHome, {{"req_id", reqId}});
    emit logLine("SCAN", QString("cmd/home 발행 (req_id=%1)").arg(reqId));
}

void MqttBridge::requestDisarm() {
    const QString reqId = newReqId();
    publishCommand(kTopicCmdDisarm, {{"req_id", reqId}});
    emit logLine("POWER", QString("cmd/disarm 발행 (req_id=%1) — 비상정지").arg(reqId));
}

void MqttBridge::requestRearm() {
    const QString reqId = newReqId();
    publishCommand(kTopicCmdRearm, {{"req_id", reqId}});
    emit logLine("POWER", QString("cmd/rearm 발행 (req_id=%1) — DISARM 해제 요청").arg(reqId));
}

void MqttBridge::subscribeAll() {
#ifdef USE_PAHO_MQTT
    if (!m_client) return;
    for (const char *t : {kTopicStateWildcard, kTopicEventWildcard}) {
        try {
            m_client->subscribe(t, kQosCmd);
        } catch (const mqtt::exception &exc) {
            qWarning() << "MQTT subscribe failed on" << t << ":" << exc.what();
        }
    }
    emit logLine("MQTT", "토픽 구독 완료 (adts/state/#, adts/event/#)");
#endif
}

#ifdef USE_PAHO_MQTT
void MqttBridge::connected(const std::string &cause) {
    QMetaObject::invokeMethod(this, [this, cause] {
        m_retryTimer.stop();
        m_loggedFailure = false;
        emit brokerStateChanged(true);
        const QString reason = cause.empty() ? QString() : QString::fromUtf8(cause.c_str());
        emit logLine("MQTT", reason.isEmpty() ? "브로커 연결됨" : QString("브로커 재연결됨 (%1)").arg(reason));
        subscribeAll();
    }, Qt::QueuedConnection);
}

void MqttBridge::connection_lost(const std::string &cause) {
    QMetaObject::invokeMethod(this, [this, cause] {
        emit brokerStateChanged(false);
        const QString reason = cause.empty() ? QString() : QString::fromUtf8(cause.c_str());
        emit logLine("MQTT", reason.isEmpty() ? "브로커 연결 끊김" : QString("브로커 연결 끊김: %1").arg(reason));
        if (m_wantConnected && !m_retryTimer.isActive()) {
            m_retryTimer.start();
        }
    }, Qt::QueuedConnection);
}

void MqttBridge::message_arrived(mqtt::const_message_ptr msg) {
    if (!msg) return;
    const QString topic = QString::fromStdString(msg->get_topic());
    /* 주의: get_payload() 는 const binary&(= std::string) 를 돌려준다. 예전
     *   paho-mqtt-cpp 의 (const void*, get_payload_len()) 조합은 지금 헤더에
     *   없어서 컴파일이 깨진다. data()/size() 로 받는다. */
    const std::string &raw = msg->get_payload();
    const QByteArray payload(raw.data(), static_cast<int>(raw.size()));
    QMetaObject::invokeMethod(this, [this, topic, payload] {
        onRawMessage(topic, payload);
    }, Qt::QueuedConnection);
}

void MqttBridge::delivery_complete(mqtt::delivery_token_ptr token) {
    Q_UNUSED(token);
}
#endif

void MqttBridge::onRawMessage(const QString &topic, const QByteArray &payload) {
    if (topic == kTopicStateDaemon)        handleStateDaemon(payload);
    else if (topic == kTopicStateScan)     handleStateScan(payload);
    else if (topic == kTopicEventProgress) handleEventProgress(payload);
    else if (topic == kTopicEventError)    handleEventError(payload);
}

void MqttBridge::handleStateDaemon(const QByteArray &payload) {
    const auto obj = QJsonDocument::fromJson(payload).object();
    DaemonState s;
    s.state       = obj.value("state").toString("OFFLINE");
    s.online      = obj.value("online").toBool(false);
    s.linkAlive   = obj.value("link_alive").toBool(false);
    s.homed       = obj.value("homed").toBool(false);
    s.scanning    = obj.value("scanning").toBool(false);
    s.curPanDdeg  = obj.value("cur_pan_ddeg").toInt(0);
    s.curTiltDdeg = obj.value("cur_tilt_ddeg").toInt(0);
    s.lastErr     = obj.value("last_err").toInt(0);
    s.ts          = tsFromUnixSeconds(static_cast<qint64>(obj.value("ts").toDouble()));

    const auto rollVal = obj.value("imu_roll");
    const auto pitchVal = obj.value("imu_pitch");
    if (!rollVal.isUndefined() && !pitchVal.isUndefined()) {
        s.level.roll  = rollVal.toDouble();
        s.level.pitch = pitchVal.toDouble();
        s.level.valid = true;
    }

    emit daemonStateUpdated(s);
}

void MqttBridge::handleStateScan(const QByteArray &payload) {
    const auto obj = QJsonDocument::fromJson(payload).object();
    const QString reqId = obj.value("req_id").toString();
    if (!acceptsReqId(reqId)) return;

    ScanResult r;
    r.reqId       = reqId;
    r.ok          = obj.value("ok").toBool(false);
    r.sessionId   = obj.value("session_id").toString();
    r.scanId      = obj.value("scan_id").toString();
    r.pcdPath     = obj.value("pcd").toString();
    r.jsonPath    = obj.value("json").toString();
    r.rows        = obj.value("rows").toInt(0);
    r.columns     = obj.value("columns").toInt(0);
    r.points      = static_cast<quint32>(obj.value("points").toInt(0));
    r.expected    = static_cast<quint32>(obj.value("expected").toInt(0));
    r.stmReported = static_cast<quint32>(obj.value("stm_reported").toInt(0));
    r.durationS   = obj.value("duration_s").toDouble(0.0);
    r.ts          = tsFromUnixSeconds(static_cast<qint64>(obj.value("ts").toDouble()));

    emit scanResultUpdated(r);
}

void MqttBridge::handleEventProgress(const QByteArray &payload) {
    const auto obj = QJsonDocument::fromJson(payload).object();
    const QString reqId = obj.value("req_id").toString();
    if (!acceptsReqId(reqId)) return;

    ScanProgress p;
    p.reqId    = reqId;
    p.points   = static_cast<quint32>(obj.value("points").toInt(0));
    p.expected = static_cast<quint32>(obj.value("expected").toInt(0));
    p.percent  = obj.value("percent").toInt(0);
    p.ts       = tsFromUnixSeconds(static_cast<qint64>(obj.value("ts").toDouble()));

    emit scanProgressUpdated(p);
}

void MqttBridge::handleEventError(const QByteArray &payload) {
    const auto obj = QJsonDocument::fromJson(payload).object();
    const QString reqId = obj.value("req_id").toString();
    if (!acceptsReqId(reqId)) return;

    KitError e;
    e.reqId = reqId;
    e.code  = obj.value("code").toInt(0);
    e.name  = obj.value("name").toString();
    e.msg   = obj.value("msg").toString();
    e.fatal = obj.value("fatal").toBool(false);
    e.ts    = tsFromUnixSeconds(static_cast<qint64>(obj.value("ts").toDouble()));

    emit kitErrorReceived(e);
}
