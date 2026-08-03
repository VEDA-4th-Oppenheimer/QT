#include "MqttBridge.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QUuid>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QMetaObject>
#include <cmath>

namespace {
constexpr int kQosCmd = 1;
constexpr int kQosBestEffort = 0;
constexpr int kKeepAliveSec = 30;

constexpr char kTopicCmdScan[]       = "adts/kit1/cmd/scan";
constexpr char kTopicCmdStop[]       = "adts/kit1/cmd/stop";
constexpr char kTopicCmdHome[]       = "adts/kit1/cmd/home";
constexpr char kTopicCmdDisarm[]     = "adts/kit1/cmd/disarm";
constexpr char kTopicStateWildcard[] = "adts/kit1/state/#";
constexpr char kTopicEventWildcard[] = "adts/kit1/event/#";
constexpr char kTopicStateDaemon[]   = "adts/kit1/state/daemon";
constexpr char kTopicStateScan[]     = "adts/kit1/state/scan";
constexpr char kTopicEventProgress[] = "adts/kit1/event/progress";
constexpr char kTopicEventError[]    = "adts/kit1/event/error";

QDateTime tsFromUnixSeconds(qint64 secs) {
    return secs > 0 ? QDateTime::fromSecsSinceEpoch(secs) : QDateTime::currentDateTime();
}
}

MqttBridge::MqttBridge(QObject *parent) : DataBridge(parent) {}

MqttBridge::~MqttBridge() { disconnectFromBroker(); }

void MqttBridge::start() { /* 라이브 브리지는 connectToBroker() 로 명시적으로 시작한다 */ }

void MqttBridge::stop() { disconnectFromBroker(); }

void MqttBridge::connectToBroker(const QString &host, quint16 port, const QString &certDir) {
    m_host = host;
    m_port = port;

    const QString caCert     = certDir.isEmpty() ? QString() : certDir + "/ca.crt";
    const QString clientCert = certDir.isEmpty() ? QString() : certDir + "/qt-console.crt";
    const QString clientKey  = certDir.isEmpty() ? QString() : certDir + "/qt-console.key";
    const bool haveCerts = !certDir.isEmpty()
        && QFile::exists(caCert) && QFile::exists(clientCert) && QFile::exists(clientKey);

    const std::string scheme = haveCerts ? "ssl://" : "tcp://";
    const std::string uri = scheme + QStringLiteral("%1:%2").arg(host).arg(port).toStdString();

    // 계약 §1: Client ID 고정 "qt-console" — 중복 접속하면 서로 끊긴다.
    m_client = std::make_unique<mqtt::async_client>(uri, "qt-console");
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
        opts.set_ssl(ssl);
        emit logLine("MQTT", QString("mTLS 인증서 로드됨 (%1) — ssl://%2:%3").arg(certDir, host).arg(port));
    } else {
        emit logLine("MQTT", QString("인증서 없음(%1) — 평문 tcp://%2:%3 로 degraded 접속 "
                                      "(계약 §6 인증서 배치 전 로컬 개발용)").arg(certDir, host).arg(port));
    }

    try {
        m_client->connect(opts);
    } catch (const mqtt::exception &exc) {
        qWarning() << "MQTT connect failed:" << exc.what();
        emit brokerStateChanged(false);
        emit logLine("MQTT", QString("연결 실패: %1").arg(exc.what()));
    }
}

void MqttBridge::disconnectFromBroker() {
    if (!m_client || !m_client->is_connected()) return;
    try {
        m_client->disconnect()->wait();
    } catch (const mqtt::exception &exc) {
        qWarning() << "MQTT disconnect failed:" << exc.what();
    }
}

QString MqttBridge::newReqId() {
    m_lastReqId = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    return m_lastReqId;
}

bool MqttBridge::acceptsReqId(const QString &incoming) const {
    // 계약 §4: 내가 보낸 req_id 가 아닌 응답은 무시(다른 콘솔이 붙어 있을 수 있다).
    // req_id 가 아예 없는 페이로드(state/daemon 등)는 항상 통과.
    return incoming.isEmpty() || m_lastReqId.isEmpty() || incoming == m_lastReqId;
}

void MqttBridge::publishCommand(const QString &topic, const QJsonObject &fields) {
    if (!m_client || !m_client->is_connected()) {
        emit logLine("MQTT", QString("브로커 미연결 — %1 발행 실패").arg(topic));
        return;
    }
    const QByteArray payload = QJsonDocument(fields).toJson(QJsonDocument::Compact);
    try {
        // 계약 §2 ⚠️: cmd 토픽에 retain 을 걸면 안 된다(재접속마다 재실행되는 안전 사고).
        m_client->publish(topic.toStdString(), payload.constData(), payload.size(),
                           kQosCmd, /*retained=*/false);
    } catch (const mqtt::exception &exc) {
        qWarning() << "MQTT publish failed on" << topic << ":" << exc.what();
    }
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
    // 계약에 DISARM -> IDLE 복구 토픽이 없다(코어에 rearm 트리거 API 미구현, TODO).
    // 실제 킷에서는 하드웨어를 물리적으로 재무장해야 할 수 있다 — 여기선 로그만 남긴다.
    emit logLine("POWER", QStringLiteral("REARM 요청 — 계약에 해당 토픽 없음(TODO, 이현우 협의 필요)"));
}

void MqttBridge::subscribeAll() {
    if (!m_client) return;
    // 계약 §2: Qt 가 구독할 것은 이 두 줄이면 끝.
    for (const char *t : {kTopicStateWildcard, kTopicEventWildcard}) {
        try {
            m_client->subscribe(t, kQosCmd);
        } catch (const mqtt::exception &exc) {
            qWarning() << "MQTT subscribe failed on" << t << ":" << exc.what();
        }
    }
}

void MqttBridge::connected(const std::string & /*cause*/) {
    emit brokerStateChanged(true);
    subscribeAll();
    emit logLine("MQTT", QString("broker connected (%1:%2), state/#·event/# 구독").arg(m_host).arg(m_port));
}

void MqttBridge::connection_lost(const std::string &cause) {
    emit brokerStateChanged(false);
    emit logLine("MQTT", QString("connection lost: %1").arg(QString::fromStdString(cause)));
    // LWT 는 브로커가 대신 발행해 주지만(계약 §5.2), 로컬 UI 도 즉시 OFFLINE 으로
    // 내려서 keepalive 대기 없이 반영한다.
    DaemonState offline;
    offline.state = "OFFLINE";
    offline.online = false;
    emit daemonStateUpdated(offline);
}

void MqttBridge::delivery_complete(mqtt::delivery_token_ptr /*token*/) {}

void MqttBridge::message_arrived(mqtt::const_message_ptr msg) {
    // Paho 콜백은 자체 네트워크 스레드에서 실행된다. 파싱은 GUI 스레드로 넘겨 처리한다.
    const QString topic = QString::fromStdString(msg->get_topic());
    const QByteArray payload(msg->get_payload().data(), static_cast<int>(msg->get_payload().size()));
    QMetaObject::invokeMethod(this, [this, topic, payload] {
        onRawMessage(topic, payload);
    }, Qt::QueuedConnection);
}

void MqttBridge::onRawMessage(const QString &topic, const QByteArray &payload) {
    if (topic == kTopicStateDaemon)   { handleStateDaemon(payload);   return; }
    if (topic == kTopicStateScan)     { handleStateScan(payload);     return; }
    if (topic == kTopicEventProgress) { handleEventProgress(payload); return; }
    if (topic == kTopicEventError)    { handleEventError(payload);    return; }
}

void MqttBridge::handleStateDaemon(const QByteArray &payload) {
    const auto o = QJsonDocument::fromJson(payload).object();
    DaemonState s;
    s.state      = o.value("state").toString("OFFLINE");
    s.online     = o.value("online").toBool();
    s.linkAlive  = o.value("link_alive").toBool();
    s.homed      = o.value("homed").toBool();
    s.scanning   = o.value("scanning").toBool();
    s.curPanDdeg  = o.value("cur_pan_ddeg").toInt();
    s.curTiltDdeg = o.value("cur_tilt_ddeg").toInt();
    s.lastErr    = o.value("last_err").toInt();
    const auto lvl = o.value("level").toObject();
    s.level.valid = lvl.value("valid").toBool();
    s.level.roll  = lvl.value("roll_deg").toDouble();
    s.level.pitch = lvl.value("pitch_deg").toDouble();
    s.ts = tsFromUnixSeconds(o.value("ts").toVariant().toLongLong());

    emit daemonStateUpdated(s);
    emit imuUpdated(s.level);   // state/daemon.level 이 계약상 유일한 IMU 출처 (별도 imu/level 토픽 없음)
}

void MqttBridge::handleStateScan(const QByteArray &payload) {
    const auto o = QJsonDocument::fromJson(payload).object();
    const QString reqId = o.value("req_id").toString();
    if (!acceptsReqId(reqId)) return;

    ScanResult r;
    r.reqId      = reqId;
    r.ok         = o.value("ok").toBool();
    r.sessionId  = o.value("session_id").toString();
    r.scanId     = o.value("scan_id").toString();
    r.pcdPath    = o.value("pcd").toString();
    r.jsonPath   = o.value("json").toString();
    r.rows       = o.value("rows").toInt();
    r.columns    = o.value("columns").toInt();
    r.points     = o.value("points").toVariant().toUInt();
    r.expected   = o.value("expected").toVariant().toUInt();
    r.durationS  = o.value("duration_s").toDouble();
    r.ts = tsFromUnixSeconds(o.value("ts").toVariant().toLongLong());
    emit scanResultUpdated(r);
}

void MqttBridge::handleEventProgress(const QByteArray &payload) {
    const auto o = QJsonDocument::fromJson(payload).object();
    const QString reqId = o.value("req_id").toString();
    if (!acceptsReqId(reqId)) return;

    ScanProgress p;
    p.reqId    = reqId;
    p.points   = o.value("points").toVariant().toUInt();
    p.expected = o.value("expected").toVariant().toUInt();
    p.percent  = o.value("percent").toInt();
    p.ts = tsFromUnixSeconds(o.value("ts").toVariant().toLongLong());
    emit scanProgressUpdated(p);
}

void MqttBridge::handleEventError(const QByteArray &payload) {
    const auto o = QJsonDocument::fromJson(payload).object();
    const QString reqId = o.value("req_id").toString();
    if (!acceptsReqId(reqId)) return;

    KitError e;
    e.reqId = reqId;
    e.code  = o.value("code").toInt();
    e.name  = o.value("name").toString();
    e.msg   = o.value("msg").toString();
    e.fatal = o.value("fatal").toBool();
    e.ts = tsFromUnixSeconds(o.value("ts").toVariant().toLongLong());
    emit kitErrorReceived(e);
}
