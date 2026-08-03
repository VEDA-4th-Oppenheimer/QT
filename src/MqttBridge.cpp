#include "MqttBridge.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QUuid>
#include <QDebug>
#include <QMetaObject>
#include <cmath>

namespace {
constexpr int kQos = 1;
constexpr int kKeepAliveSec = 30;
}

MqttBridge::MqttBridge(QObject *parent) : DataBridge(parent) {}

MqttBridge::~MqttBridge() { disconnectFromBroker(); }

void MqttBridge::start() { /* 라이브 브리지는 connectToBroker() 로 명시적으로 시작한다 */ }

void MqttBridge::stop() { disconnectFromBroker(); }

void MqttBridge::connectToBroker(const QString &host, quint16 port) {
    m_host = host;
    m_port = port;
    const std::string uri = QStringLiteral("tcp://%1:%2").arg(host).arg(port).toStdString();
    const std::string clientId = QStringLiteral("spatial-vms-%1")
                                      .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8))
                                      .toStdString();
    m_client = std::make_unique<mqtt::async_client>(uri, clientId);
    m_client->set_callback(*this);

    mqtt::connect_options opts;
    opts.set_clean_session(true);
    opts.set_keep_alive_interval(kKeepAliveSec);
    opts.set_automatic_reconnect(true);

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

void MqttBridge::setKitPower(bool on) {
    // 실제 STM32/RPi 프로토콜엔 "전원 ON/OFF" 커맨드가 없다(CMD_HOME/SCAN_*/DISARM 뿐).
    // 확정되면 이 자리에서 실제 토픽으로 publish 한다 — 지금은 로그만 남긴다.
    emit logLine("POWER", on ? QStringLiteral("전원 ON 요청 (실제 프로토콜 미정 — 로그만 기록)")
                              : QStringLiteral("전원 OFF 요청 (실제 프로토콜 미정 — 로그만 기록)"));
}

void MqttBridge::requestRescan() {
    // CMD_SCAN_START. Qt 에 아직 pan/tilt 범위 입력 UI가 없어 기본 풀스윕 파라미터로 발행한다.
    const QByteArray payload = R"({"pan_start_ddeg":0,"pan_end_ddeg":1800,)"
                                R"("tilt_start_ddeg":0,"tilt_end_ddeg":0,)"
                                R"("step_ddeg":10,"z_offset_mm":0})";
    publish("scan/start", payload);
    emit logLine("SCAN", QStringLiteral("scan/start 발행 (pan 0..180°, step 1.0°)"));
}

void MqttBridge::publish(const QString &topic, const QByteArray &payload) {
    if (!m_client || !m_client->is_connected()) return;
    try {
        m_client->publish(topic.toStdString(), payload.constData(), payload.size(), kQos, false);
    } catch (const mqtt::exception &exc) {
        qWarning() << "MQTT publish failed on" << topic << ":" << exc.what();
    }
}

void MqttBridge::subscribeAll() {
    if (!m_client) return;
    static const char *topics[] = {
        "scan/status", "scan/done",           // 확정 (daemon_module.h)
        "calib/result", "calib/objects",      // TODO: 카메라 단 발행 토픽명 협의 중
        "imu/level",                          // TODO: 실시간 IMU 브로드캐스트 여부 미정
    };
    for (auto *t : topics) {
        try {
            m_client->subscribe(t, kQos);
        } catch (const mqtt::exception &exc) {
            qWarning() << "MQTT subscribe failed on" << t << ":" << exc.what();
        }
    }
}

void MqttBridge::connected(const std::string & /*cause*/) {
    emit brokerStateChanged(true);
    subscribeAll();
    emit logLine("MQTT", QString("broker connected (%1:%2), 5 topics subscribed").arg(m_host).arg(m_port));
}

void MqttBridge::connection_lost(const std::string &cause) {
    emit brokerStateChanged(false);
    emit logLine("MQTT", QString("connection lost: %1").arg(QString::fromStdString(cause)));
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
    if (topic == "scan/status")     { handleScanStatus(payload); return; }
    if (topic == "scan/done")       { handleScanDone(payload);   return; }
    if (topic == "calib/result")    { handleCalibResult(payload); return; }
    if (topic == "calib/objects")   { handleObjects(payload);    return; }
    if (topic == "imu/level")       { handleImu(payload);        return; }
}

// scan/status: RPi 데몬이 SCANNING 중 발행 (struct scan_progress 기반 JSON 가정)
//   {"percent":42,"points":7823,"expected":18432,"state":"SCANNING"}
// 수평 게이트 실패 시 (아키텍처 V2 시퀀스): {"state":"tilt_ng"}
void MqttBridge::handleScanStatus(const QByteArray &payload) {
    const auto o = QJsonDocument::fromJson(payload).object();
    const QString state = o.value("state").toString();
    if (state == "tilt_ng") {
        emit logLine("LEVEL", QStringLiteral("수평 게이트 실패 — 스캔 거부 (재설치 필요)"));
        return;
    }
    m_calib.status         = "SCANNING";
    m_calib.progress       = o.value("percent").toInt();
    m_calib.scanPoints     = o.value("points").toInt();
    m_calib.expectedPoints = o.value("expected").toInt();
    m_calib.stamp          = QDateTime::currentDateTime();
    emit calibUpdated(m_calib);
}

// scan/done: {"path":"...","point_count":18432,"stm_reported":18432}
// 포인트클라우드 파일이 준비됐다는 뜻이지, 캘리브 결과(PASS/FAIL)는 아직 아니다.
void MqttBridge::handleScanDone(const QByteArray &payload) {
    const auto o = QJsonDocument::fromJson(payload).object();
    m_calib.status     = "EXPORT";
    m_calib.scanPoints = o.value("point_count").toInt();
    m_calib.stamp      = QDateTime::currentDateTime();
    emit calibUpdated(m_calib);
    emit logLine("EXPORT", QString("포인트클라우드 완료: %1 (%2점) — 카메라 단 전달 대기")
                                .arg(o.value("path").toString()).arg(m_calib.scanPoints));
}

// calib/result [TODO 토픽]: "02" 문서 §14.2 extrinsic/quality 스키마를 그대로 따른다고 가정.
void MqttBridge::handleCalibResult(const QByteArray &payload) {
    const auto root = QJsonDocument::fromJson(payload).object();
    const auto quality = root.value("quality").toObject();
    const auto extrinsic = root.value("extrinsic").toObject();
    const auto translation = extrinsic.value("translation_m").toArray();
    const auto quat = extrinsic.value("quaternion_xyzw").toArray();

    m_calib.status      = quality.value("status").toString("FAIL");
    m_calib.edgeRmsePx   = quality.value("edge_rmse_px").toDouble();
    m_calib.inlierRatio  = quality.value("inlier_ratio").toDouble();
    m_calib.retry        = quality.value("retry_count").toInt();
    m_calib.failureReasons.clear();
    for (const auto &v : quality.value("failure_reasons").toArray())
        m_calib.failureReasons << v.toString();

    for (int i = 0; i < 3 && i < translation.size(); ++i) m_calib.translationM[i] = translation[i].toDouble();
    for (int i = 0; i < 4 && i < quat.size(); ++i) m_calib.quaternionXyzw[i] = quat[i].toDouble();

    m_calib.stamp = QDateTime::currentDateTime();
    emit calibUpdated(m_calib);
    emit logLine("CHECK", QString("캘리브 결과 %1 — edge_rmse %2px, inlier %3%")
                               .arg(m_calib.status)
                               .arg(m_calib.edgeRmsePx, 0, 'f', 2)
                               .arg(int(m_calib.inlierRatio * 100)));
}

void MqttBridge::handleImu(const QByteArray &payload) {
    const auto o = QJsonDocument::fromJson(payload).object();
    ImuState imu;
    imu.roll  = o.contains("roll_deg")  ? o.value("roll_deg").toDouble()  : o.value("roll").toDouble();
    imu.pitch = o.contains("pitch_deg") ? o.value("pitch_deg").toDouble() : o.value("pitch").toDouble();
    emit imuUpdated(imu);
}

// calib/objects [TODO 토픽]: WiseAI(Wisenet 네이티브 사람/차량 클래스) bbox를 카메라 단이
// 캘리브 extrinsic으로 실좌표 변환해 발행한다고 가정 (아키텍처 V2 데이터 흐름).
void MqttBridge::handleObjects(const QByteArray &payload) {
    QVector<SpatialObject> out;
    for (const auto &v : QJsonDocument::fromJson(payload).object().value("objects").toArray()) {
        const auto o = v.toObject();
        SpatialObject s;
        s.cls     = o.value("class").toString();
        s.posM    = QPointF(o.value("x").toDouble(), o.value("y").toDouble());
        s.distM   = o.contains("dist") ? o.value("dist").toDouble() : std::hypot(s.posM.x(), s.posM.y());
        s.channel = o.value("ch").toInt(1);
        out.push_back(s);
    }
    emit objectsUpdated(out);
}
