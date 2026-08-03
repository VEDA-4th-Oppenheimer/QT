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
    // 실제 영상은 RTSP 직결(RtspSource)이라 MQTT 와 무관하다 — 이건 RTSP 미설정 시의
    // 데모용 채널 상태 시뮬레이션일 뿐이다.
    emit channelStatusChanged(1, true, 24.9);
    emit channelStatusChanged(2, true, 25.0);
    emit channelStatusChanged(3, true, 25.0);
    emit channelStatusChanged(4, false, 0.0);
    emit logLine("RTSP", QStringLiteral("CH4 응답 없음 (데모 시뮬레이션)"));
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

// "01. Point Cloud 생성 및 인계 계획" + "02. Point Cloud 이후 Camera Automatic
// Calibration 상세 계획" 문서의 실제 세션 흐름을 재생한다: STM32 스캔(scan/status·
// scan/done) -> 카메라 단 캘리브(현재 토픽 미정, calib/result 로 가정) -> quality
// gate PASS. edge_rmse_px/inlier_ratio 가 메인 지표이고 NCC 는 진단용 참고값이다
// (문서 §11.3) — 낮은 edge_rmse 로 한 번 실패시켜 auto-retry(최대 2회)를 보여준다.
void DemoBridge::runCalibScript() {
    struct Item {
        int delayMs; QString tag; QString msg; QString status;
        int progress; quint32 points, expected; int retry;
        double edgeRmse, inlier, ncc;
        bool haveExtrinsic;
    };
    static const QVector<Item> steps = {
        { 400, "SCAN",   QStringLiteral("scan/start 발행 — pan 0..180°, step 1.0° (연속 raster sweep)"),
          "SCANNING", 5,   0,     18432, 0, 0.0,  0.0,  0.0, false },
        { 900, "SCAN",   QStringLiteral("STM32 CMD_SCAN_START ack — 연속 pan sweep 진행 중 (~100°/s)"),
          "SCANNING", 35,  6400,  18432, 0, 0.0,  0.0,  0.0, false },
        { 700, "SCAN",   QStringLiteral("scan/status — 18,432 points, organized grid 수집 완료"),
          "SCANNING", 70,  18432, 18432, 0, 0.0,  0.0,  0.0, false },
        { 600, "EXPORT", QStringLiteral("scan/done — PointCloudPackage 인계 (organized_cloud.pcd, QA PASS)"),
          "EXPORT",   85,  18432, 18432, 0, 0.0,  0.0,  0.0, false },
        { 700, "MATCH",  QStringLiteral("Canonical scene 빌드 — camera edge DT + LiDAR depth-edge/LSD 라인"),
          "EXPORT",   90,  18432, 18432, 0, 0.0,  0.0,  0.0, false },
        { 700, "MATCH",  QStringLiteral("Bounded coarse search 128 candidates → fine seed 8개 선정"),
          "EXPORT",   93,  18432, 18432, 0, 0.0,  0.0,  0.0, false },
        { 800, "CHECK",  QStringLiteral("SE(3) fine optimize — edge_rmse 4.82px (기준 3px 초과) — auto-retry (1/2)"),
          "SCANNING", 40,  18432, 18432, 1, 4.82, 0.58, 0.61, false },
        { 900, "SCAN",   QStringLiteral("재시도 — 근거리 구조물 포함 장면으로 재촬영"),
          "SCANNING", 70,  18432, 18432, 1, 4.82, 0.58, 0.61, false },
        { 900, "CHECK",  QStringLiteral("multi-scene joint refine — edge_rmse 2.31px, inlier 81% — PASS"),
          "PASS",     100, 18432, 18432, 1, 2.31, 0.81, 0.79, true  },
        { 400, "EXPORT", QStringLiteral("extrinsic_candidate.yaml 기록 — Commit → active calibration 승격"),
          "PASS",     100, 18432, 18432, 1, 2.31, 0.81, 0.79, true  },
    };

    if (m_calibStep >= steps.size()) return;
    const Item s = steps[m_calibStep];
    QTimer::singleShot(s.delayMs, this, [this, s] {
        emit logLine(s.tag, s.msg);
        CalibState c;
        c.status = s.status; c.progress = s.progress;
        c.scanPoints = s.points; c.expectedPoints = s.expected;
        c.retry = s.retry; c.maxRetry = 2;
        c.edgeRmsePx = s.edgeRmse; c.inlierRatio = s.inlier; c.ncc = s.ncc;
        if (s.status != "PASS" && !s.haveExtrinsic)
            c.failureReasons = s.retry > 0 ? QStringList{"LOW_EDGE_COUNT"} : QStringList{};
        if (s.haveExtrinsic) {
            // 카메라 바로 아래 천장 스택 배치 → 시차(translation) 거의 0, 회전은 미세 tilt.
            c.translationM[0] = 0.012; c.translationM[1] = -0.018; c.translationM[2] = 0.152;
            c.quaternionXyzw[0] = 0.011; c.quaternionXyzw[1] = 0.019;
            c.quaternionXyzw[2] = -0.004; c.quaternionXyzw[3] = 0.9997;
        }
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
