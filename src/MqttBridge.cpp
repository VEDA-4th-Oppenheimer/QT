#include "MqttBridge.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QDateTime>
#include <QUuid>
#include <QDebug>
#include <QMetaObject>
#include <cmath>

namespace {
constexpr int kQos = 1;
constexpr int kKeepAliveSec = 30;
constexpr int kChannelTimeoutMs = 3000;
}

MqttBridge::MqttBridge(QObject *parent) : DataBridge(parent) {
    for (int i = 0; i < 4; ++i) {
        m_timeout[i] = new QTimer(this);
        m_timeout[i]->setSingleShot(true);
        m_timeout[i]->setInterval(kChannelTimeoutMs);
        const int ch = i + 1;
        connect(m_timeout[i], &QTimer::timeout, this, [this, ch] {
            emit channelStatusChanged(ch, false, 0.0);
            emit logLine("MQTT", QString("cctv/ch%1/h264 구독 끊김 — keepalive timeout").arg(ch));
        });
    }
}

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
    publish("kit/cmd/power", on ? "{\"power\":1}" : "{\"power\":0}");
    emit logLine("POWER", on ? QStringLiteral("kit power ON 명령 전송") : QStringLiteral("kit power OFF 명령 전송"));
}

void MqttBridge::requestRescan() {
    publish("kit/cmd/rescan", "{\"rescan\":1}");
    emit logLine("SCAN", QStringLiteral("re-scan 명령 전송"));
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
        "cctv/ch1/h264", "cctv/ch2/h264", "cctv/ch3/h264", "cctv/ch4/h264",
        "wiseai/+/objects", "kit/lidar/scan", "kit/imu/level", "kit/calib/status",
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
    emit logLine("MQTT", QString("broker connected (%1:%2), 8 topics subscribed").arg(m_host).arg(m_port));
}

void MqttBridge::connection_lost(const std::string &cause) {
    emit brokerStateChanged(false);
    emit logLine("MQTT", QString("connection lost: %1").arg(QString::fromStdString(cause)));
}

void MqttBridge::delivery_complete(mqtt::delivery_token_ptr /*token*/) {}

void MqttBridge::message_arrived(mqtt::const_message_ptr msg) {
    // Paho 콜백은 자체 네트워크 스레드에서 실행된다. QTimer 등 GUI 스레드 전용 객체를
    // 건드리는 실제 처리는 큐드 연결로 GUI 스레드에 넘긴 뒤 onRawMessage 에서 수행한다.
    const QString topic = QString::fromStdString(msg->get_topic());
    const QByteArray payload(msg->get_payload().data(), static_cast<int>(msg->get_payload().size()));
    QMetaObject::invokeMethod(this, [this, topic, payload] {
        onRawMessage(topic, payload);
    }, Qt::QueuedConnection);
}

void MqttBridge::onRawMessage(const QString &topic, const QByteArray &payload) {
    static const QRegularExpression chRe("^cctv/ch(\\d)/h264$");
    const auto m = chRe.match(topic);
    if (m.hasMatch())            { handleFrame(m.captured(1).toInt(), payload); return; }
    if (topic == "kit/imu/level")    { handleImu(payload); return; }
    if (topic == "kit/lidar/scan")   { handleLidarScan(payload); return; }
    if (topic.endsWith("/objects"))  { handleObjects(payload); return; }
    if (topic == "kit/calib/status") { handleCalib(payload); return; }
}

void MqttBridge::handleFrame(int ch, const QByteArray &payload) {
    if (ch < 1 || ch > 4) return;
    // TODO: payload 가 H.264 NAL 이면 여기서 FFmpeg/QtMultimedia 디코더를 통과시킨다.
    // 지금은 publisher 가 JPEG 프레임을 그대로 싣는다고 가정한다.
    QImage img;
    const bool ok = img.loadFromData(payload, "JPG");

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    double fps = 0.0;
    if (m_lastFrameAtMs[ch - 1] > 0) {
        const qint64 dt = now - m_lastFrameAtMs[ch - 1];
        if (dt > 0) fps = 1000.0 / dt;
    }
    m_lastFrameAtMs[ch - 1] = now;

    m_timeout[ch - 1]->start();   // 3초 이내 다음 프레임이 없으면 DISCONNECTED 처리
    emit channelStatusChanged(ch, true, fps);
    if (ok) emit frameReceived(ch, img);
}

void MqttBridge::handleImu(const QByteArray &payload) {
    const auto o = QJsonDocument::fromJson(payload).object();
    ImuState imu;
    imu.roll  = o.contains("roll_deg")  ? o.value("roll_deg").toDouble()  : o.value("roll").toDouble();
    imu.pitch = o.contains("pitch_deg") ? o.value("pitch_deg").toDouble() : o.value("pitch").toDouble();
    emit imuUpdated(imu);
}

void MqttBridge::handleObjects(const QByteArray &payload) {
    // Module 4: 2D BBox 바닥점 -> H^-1 / RT^-1 변환된 실내 좌표(x,y[m])가 이미 실려온다고 가정.
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

void MqttBridge::handleLidarScan(const QByteArray &payload) {
    // 가정: RPi 파이프라인이 Depth-Edge -> 평면 투영을 마친 라인 목록을 올린다.
    //   {"edges":[{"a":{"x":..,"y":..},"b":{"x":..,"y":..}}, ...]}
    // 원시 포인트 클라우드를 그대로 올리는 펌웨어라면 여기서 라인 피팅이 선행되어야 한다.
    QVector<MapEdge> edges;
    for (const auto &v : QJsonDocument::fromJson(payload).object().value("edges").toArray()) {
        const auto o = v.toObject();
        const auto a = o.value("a").toObject();
        const auto b = o.value("b").toObject();
        edges.push_back({QPointF(a.value("x").toDouble(), a.value("y").toDouble()),
                          QPointF(b.value("x").toDouble(), b.value("y").toDouble())});
    }
    if (!edges.isEmpty()) emit mapEdgesUpdated(edges);
}

void MqttBridge::handleCalib(const QByteArray &payload) {
    const auto o = QJsonDocument::fromJson(payload).object();
    CalibState c;
    c.ncc            = o.value("ncc").toDouble();
    c.reprojPx        = o.value("reproj_px").toDouble();
    c.retry           = o.value("retry").toInt();
    c.maxRetry        = o.value("max_retry").toInt(3);
    c.progress        = o.value("progress").toInt();
    c.scanPoints      = o.value("points").toInt();
    c.coverage        = o.value("coverage").toDouble();
    c.inliers         = o.value("inliers").toInt();
    c.candidateLines  = o.value("candidate_lines").toInt();
    c.stamp           = QDateTime::currentDateTime();
    emit calibUpdated(c);
}
