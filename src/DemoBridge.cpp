#include "DemoBridge.h"
#include <QDateTime>
#include <QVector>
#include <cmath>

DemoBridge::DemoBridge(QObject *parent) : DataBridge(parent) {
    connect(&m_imuTimer, &QTimer::timeout, this, &DemoBridge::tickImu);
    connect(&m_objTimer, &QTimer::timeout, this, &DemoBridge::tickObjects);
    m_imuTimer.setInterval(500);
    m_objTimer.setInterval(1000);
}

void DemoBridge::start() {
    emit brokerStateChanged(false);
    QTimer::singleShot(600, this, [this] {
        emit brokerStateChanged(true);
        emit logLine("MQTT", QStringLiteral("데모 브로커 시뮬레이션 — 8개 토픽 구독"));
    });

    emitChannelDefaults();
    emitEdges();

    m_imuTimer.start();
    m_objTimer.start();

    m_calibStep = 0;
    runCalibScript();
}

void DemoBridge::stop() {
    m_imuTimer.stop();
    m_objTimer.stop();
}

void DemoBridge::emitChannelDefaults() {
    emit channelStatusChanged(1, true, 24.9);
    emit channelStatusChanged(2, true, 25.0);
    emit channelStatusChanged(3, true, 25.0);
    emit channelStatusChanged(4, false, 0.0);
    emit logLine("MQTT", QStringLiteral("cctv/ch4/h264 구독 응답 없음 — DISCONNECTED (데모)"));
}

void DemoBridge::emitEdges() {
    QVector<MapEdge> edges;
    constexpr double h = 5.0;   // 10 x 10 m 방 절반 크기
    edges.push_back({{-h, -h}, { h, -h}});
    edges.push_back({{ h, -h}, { h,  h}});
    edges.push_back({{ h,  h}, {-h,  h}});
    edges.push_back({{-h,  h}, {-h, -h}});
    // 데모용 내부 파티션/기둥
    edges.push_back({{-1.0,  h}, {-1.0, 1.5}});
    edges.push_back({{ 2.0, -h}, { 2.0, -1.0}});
    edges.push_back({{ 2.0, -1.0}, { 3.4, -1.0}});
    emit mapEdgesUpdated(edges);
}

void DemoBridge::runCalibScript() {
    struct Item {
        int delayMs; QString tag; QString msg;
        int progress; double ncc, reproj; int retry, points, inliers, candidates; double coverage;
    };
    static const QVector<Item> steps = {
        { 400, "SCAN",  QStringLiteral("360° grid scan started — θ 0..360°, φ −78°..+8° (ceiling mount)"),
          5,   0.0,   0.0,  0, 0,     0,  0,  0.0  },
        { 900, "SCAN",  QStringLiteral("18,432 points captured, 1,204 depth-edge candidates (실내 10.0 × 10.0 m)"),
          20,  0.0,   0.0,  0, 18432, 0,  0,  0.10 },
        { 600, "LSD",   QStringLiteral("19 structural lines after NFA filtering (V:11 / H:8)"),
          35,  0.0,   0.0,  0, 18432, 0,  19, 0.25 },
        { 600, "MATCH", QStringLiteral("19 candidate line pairs → RANSAC inliers 12"),
          50,  0.0,   0.0,  0, 18432, 12, 19, 0.55 },
        { 600, "CHECK", QStringLiteral("NCC 0.684 < 0.720 — auto-retry triggered (1/3)"),
          55,  0.684, 0.0,  1, 18432, 12, 19, 0.55 },
        { 700, "SCAN",  QStringLiteral("Re-scan started with φ offset +6°"),
          65,  0.684, 0.0,  1, 18432, 12, 19, 0.6  },
        { 900, "MATCH", QStringLiteral("RANSAC inliers 14 / 19, reprojection 2.14 px"),
          85,  0.684, 2.14, 1, 18432, 14, 19, 0.75 },
        { 500, "CHECK", QStringLiteral("NCC 0.813 ≥ 0.720 — PASS"),
          100, 0.813, 2.14, 1, 18432, 14, 19, 0.92 },
        { 400, "EXPORT",QStringLiteral("calib_ch1_20260803.json written (RT, H, report)"),
          100, 0.813, 2.14, 1, 18432, 14, 19, 0.92 },
    };

    if (m_calibStep >= steps.size()) return;
    const Item s = steps[m_calibStep];
    QTimer::singleShot(s.delayMs, this, [this, s] {
        emit logLine(s.tag, s.msg);
        CalibState c;
        c.progress = s.progress; c.ncc = s.ncc; c.reprojPx = s.reproj;
        c.retry = s.retry; c.maxRetry = 3; c.scanPoints = s.points;
        c.inliers = s.inliers; c.candidateLines = s.candidates;
        c.coverage = s.coverage;
        c.stamp = QDateTime::currentDateTime();
        emit calibUpdated(c);
        ++m_calibStep;
        runCalibScript();
    });
}

void DemoBridge::tickImu() {
    m_imuPhase += 0.2;
    const bool faultWindow = std::fmod(m_imuPhase, 40.0) > 34.0;
    ImuState imu;
    imu.roll  = std::sin(m_imuPhase) * 0.6         + (faultWindow ?  2.7 : 0.0);
    imu.pitch = std::cos(m_imuPhase * 0.6) * 0.4    + (faultWindow ? -1.4 : 0.0);
    emit imuUpdated(imu);
}

void DemoBridge::tickObjects() {
    m_objPhase += 0.3;
    QVector<SpatialObject> objs;
    SpatialObject person;
    person.cls  = QStringLiteral("PERSON");
    person.posM = QPointF(-1.7 + std::sin(m_objPhase) * 0.3, 1.6 + std::cos(m_objPhase * 0.7) * 0.3);
    person.distM = std::hypot(person.posM.x(), person.posM.y());
    person.channel = 1;
    objs.push_back(person);
    emit objectsUpdated(objs);
}

void DemoBridge::setKitPower(bool on) {
    m_powered = on;
    emit logLine("POWER", on ? QStringLiteral("킷 전원 ON (데모)") : QStringLiteral("킷 전원 OFF (데모)"));
}

void DemoBridge::requestRescan() {
    emit logLine("SCAN", QStringLiteral("재스캔 명령 (데모)"));
    m_calibStep = 0;
    runCalibScript();
}
