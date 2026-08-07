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
        if (m_client && m_client->is_connected()) { m_retryTimer.stop(); return; }
        attemptConnect();
    });
}

MqttBridge::~MqttBridge() { disconnectFromBroker(); }

void MqttBridge::start() { /* 라이브 브리지는 connectToBroker() 로 명시적으로 시작한다 */ }

void MqttBridge::stop() { disconnectFromBroker(); }

void MqttBridge::connectToBroker(const QString &host, quint16 port, const QString &certDir) {
    m_host = host;
    m_port = port;

    const QString caCert     = certDir.isEmpty() ? QString() : certDir + "/ca.crt";
    const QString clientCert = certDir.isEmpty() ? QString() : certDir + "/qt-console.crt";
    // broker/gen-certs.sh(RPi 저장소, 이광진)는 기본적으로 PKCS#8 qt-console.key 와,
    // QSslKey 용으로 변환한 qt-console-trad.key 를 같이 만들어 준다. Paho(OpenSSL
    // 직접 사용)는 PKCS#8 을 그대로 읽으므로 원래 이름을 우선 찾고, 혹시 trad 버전만
    // 복사해왔어도 그대로 동작하도록 폴백한다.
    const QString clientKeyPlain = certDir.isEmpty() ? QString() : certDir + "/qt-console.key";
    const QString clientKeyTrad  = certDir.isEmpty() ? QString() : certDir + "/qt-console-trad.key";
    const QString clientKey = QFile::exists(clientKeyPlain) ? clientKeyPlain : clientKeyTrad;
    const bool haveCerts = !certDir.isEmpty()
        && QFile::exists(caCert) && QFile::exists(clientCert) && QFile::exists(clientKey);

    const std::string scheme = haveCerts ? "ssl://" : "tcp://";
    const std::string uri = scheme + QStringLiteral("%1:%2").arg(host).arg(port).toStdString();

    // Client ID 는 인스턴스마다 달라야 한다. MQTT 는 같은 ID 로 두 번째가 붙으면
    // 브로커가 첫 번째를 끊으므로, 고정 ID 로 배포하면 두 명이 콘솔을 켜는 순간
    // 서로 계속 끊어내는 상태가 된다.
    //   계약서 §1 은 "고정 qt-console" 이라고 적고 있으나 다중 콘솔에서 성립하지
    //   않는다. 권한은 브로커가 인증서 CN 으로 판정하므로(mosquitto.conf 의
    //   use_identity_as_username true → ACL 의 user qt-console), Client ID 를
    //   바꿔도 인증서와 ACL 은 그대로 쓸 수 있다.
    // 호스트명으로 "누구 콘솔인지" 브로커 로그에서 알아볼 수 있게 하고, 같은 PC
    // 에서 두 개를 띄워도 겹치지 않도록 난수를 붙인다.
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
        // 데몬 쪽(mqtt_module.c)도 mosquitto_tls_opts_set(..., "tlsv1.2", ...)로
        // 고정한다 — 브로커 협상 폭을 양쪽에서 동일하게 맞춘다.
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
    m_retryTimer.start();   // 붙을 때까지 주기적으로 재시도(붙으면 스스로 멈춘다)
}

void MqttBridge::attemptConnect() {
    if (!m_client || m_client->is_connected()) return;
    try {
        // 리스너를 넘겨야 실패를 알 수 있다. connect() 자체는 비동기라 호스트가
        // 꺼져 있어도 여기서 예외가 나지 않는다 — 아래 catch 는 "이미 접속 시도
        // 중" 같은 즉시 오류만 잡는다.
        m_client->connect(m_connOpts, nullptr, *this);
    } catch (const mqtt::exception &exc) {
        // 접속 시도가 이미 진행 중이어도 여기로 온다 — 재시도 타이머가 계속
        // 돌고 있으므로 조용히 넘긴다. 로그는 첫 실패 때만 남긴다.
        emit brokerStateChanged(false);
        if (!m_loggedFailure) {
            m_loggedFailure = true;
            qWarning() << "MQTT connect failed:" << exc.what();
            emit logLine("MQTT", QString("연결 실패: %1 — %2초마다 재시도")
                                      .arg(exc.what()).arg(kReconnectRetryMs / 1000));
        }
    }
}

void MqttBridge::on_failure(const mqtt::token &tok) {
    const int rc = tok.get_return_code();
    // Paho 스레드에서 불린다 — UI 갱신은 GUI 스레드로 넘긴다.
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
    // 실제 "붙었다" 처리는 connected() 콜백에서 한다 — 구독까지 거기서 걸기 때문에
    // 여기서 중복으로 알리지 않는다.
}

void MqttBridge::disconnectFromBroker() {
    m_wantConnected = false;   // 의도적 종료 — 재시도 타이머가 다시 붙이지 않게
    m_retryTimer.stop();
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
    // Paho 네트워크 스레드에서 불린다. QTimer 는 자기 스레드에서만 멈출 수 있어서
    // (여기서 stop() 하면 "Timers cannot be stopped from another thread") 본문을
    // GUI 스레드로 넘긴다 — message_arrived 와 같은 방식이다.
    QMetaObject::invokeMethod(this, [this] {
        m_retryTimer.stop();   // 붙었으니 재시도 중단 (이후 끊김은 Paho 자동 재접속이 담당)
        m_loggedFailure = false;
        emit brokerStateChanged(true);
        subscribeAll();
        emit logLine("MQTT", QString("broker connected (%1:%2), state/#·event/# 구독").arg(m_host).arg(m_port));
    }, Qt::QueuedConnection);
}

void MqttBridge::connection_lost(const std::string &cause) {
    QMetaObject::invokeMethod(this, [this, cause] {
    emit brokerStateChanged(false);
    emit logLine("MQTT", QString("connection lost: %1").arg(QString::fromStdString(cause)));
    // LWT 는 브로커가 대신 발행해 주지만(계약 §5.2), 로컬 UI 도 즉시 OFFLINE 으로
    // 내려서 keepalive 대기 없이 반영한다.
    DaemonState offline;
    offline.state = "OFFLINE";
    offline.online = false;
    emit daemonStateUpdated(offline);
    }, Qt::QueuedConnection);
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

    // 실구현(develop 브랜치)이 실제로 보내는 필드: req_id/ok/pcd/points/
    // stm_reported/ts 뿐이다. session_id/scan_id/json/rows/columns/expected/
    // duration_s 는 계약 §3.4에 있지만 아직 안 보낸다 — QJsonObject::value 가
    // 없는 키에 기본값(빈 문자열/0)을 주므로 파싱 자체는 안전하다.
    ScanResult r;
    r.reqId       = reqId;
    r.ok          = o.value("ok").toBool();
    r.sessionId   = o.value("session_id").toString();
    r.scanId      = o.value("scan_id").toString();
    r.pcdPath     = o.value("pcd").toString();
    r.jsonPath    = o.value("json").toString();
    r.rows        = o.value("rows").toInt();
    r.columns     = o.value("columns").toInt();
    r.points      = o.value("points").toVariant().toUInt();
    r.expected    = o.value("expected").toVariant().toUInt();
    r.stmReported = o.value("stm_reported").toVariant().toUInt();
    r.durationS   = o.value("duration_s").toDouble();
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
