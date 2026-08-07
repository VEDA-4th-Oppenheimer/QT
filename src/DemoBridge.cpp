#include "DemoBridge.h"
#include <QDateTime>
#include <QUuid>
#include <QVector>
#include <cmath>

DemoBridge::DemoBridge(QObject *parent) : DataBridge(parent) {
    connect(&m_imuTimer, &QTimer::timeout, this, &DemoBridge::tickImu);
    connect(&m_objTimer, &QTimer::timeout, this, &DemoBridge::tickObjects);
    m_imuTimer.setInterval(500);
    m_objTimer.setInterval(1000);
}

void DemoBridge::start() {
    m_running = true;
    emit brokerStateChanged(false);
    QTimer::singleShot(600, this, [this] {
        if (!m_running) return;   // 그 사이 Live 로 전환됐다 — 가짜 CONNECTED 를 쏘면 안 된다
        emit brokerStateChanged(true);
        emit logLine("MQTT", QStringLiteral("데모 브로커 시뮬레이션 — state/#·event/# 구독"));
        publishDaemonState("IDLE");
    });

    emitChannelDefaults();
    emitEdges();

    m_imuTimer.start();
    m_objTimer.start();
}

void DemoBridge::stop() {
    m_running = false;
    m_imuTimer.stop();
    m_objTimer.stop();
}

QString DemoBridge::newReqId() {
    m_reqId = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    return m_reqId;
}

void DemoBridge::publishDaemonState(const QString &state) {
    m_daemonState = state;
    DaemonState s;
    s.state       = state;
    s.online      = true;
    s.linkAlive   = true;
    s.homed       = true;
    s.scanning    = (state == "SCANNING");
    s.curPanDdeg  = 0;
    s.curTiltDdeg = 0;
    s.lastErr     = 0;
    s.level.valid = true;
    s.level.roll  = m_lastRoll;
    s.level.pitch = m_lastPitch;
    s.ts = QDateTime::currentDateTime();
    emit daemonStateUpdated(s);
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

// MQTT_INTERFACE_CONTRACT.md 세션 흐름을 재생: cmd/scan -> event/progress(2Hz 흉내)
// -> state=EXPORT + state/scan -> state=IDLE.
void DemoBridge::runScanScript() {
    if (m_daemonState != "SCANNING") {
        return;   // 중간에 DISARM/STOP 등으로 흐름이 끊겼으면 여기서 멈춘다.
    }

    struct Item { int delayMs; QString msg; int percent; quint32 points; };
    static const QVector<Item> steps = {
        { 400, QStringLiteral("cmd/scan 수신 — 연속 pan sweep 시작"), 5,  900  },
        { 700, QStringLiteral("STM32 CMD_SCAN_START ack — 진행 중"), 30, 5400 },
        { 700, QStringLiteral("스캔 진행 중"),                        65, 11700},
        { 700, QStringLiteral("스캔 진행 중"),                        90, 16200},
        { 500, QStringLiteral("스캔 완료 — 포인트클라우드 마감"),      100,18000},
    };

    if (m_scanStep >= steps.size()) {
        publishDaemonState("EXPORT");

        ScanResult r;
        r.reqId     = m_reqId;
        r.ok        = true;
        r.sessionId = QStringLiteral("calib-%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss"));
        r.scanId    = QStringLiteral("sweep-000001");
        r.pcdPath   = QStringLiteral("./scans/%1_%2.pcd").arg(r.sessionId, r.scanId);
        r.jsonPath  = QStringLiteral("./scans/%1_%2_pan_tilt_lidar.json").arg(r.sessionId, r.scanId);
        r.rows = 180; r.columns = 100;
        r.points = 18000; r.expected = 18000;
        r.stmReported = 18000;
        r.durationS = 4.2;
        r.ts = QDateTime::currentDateTime();
        emit scanResultUpdated(r);
        emit logLine("EXPORT", QString("state/scan 발행 — %1 (%2점)").arg(r.pcdPath).arg(r.points));

        m_scanning = false;
        QTimer::singleShot(400, this, [this] {
            if (!m_running) return;
            if (m_daemonState == "EXPORT") publishDaemonState("IDLE");
        });
        return;
    }

    const Item s = steps[m_scanStep];
    QTimer::singleShot(s.delayMs, this, [this, s] {
        if (!m_running) return;
        if (m_daemonState != "SCANNING") return;
        emit logLine("SCAN", s.msg);
        ScanProgress p;
        p.reqId    = m_reqId;
        p.points   = s.points;
        p.expected = 18000;
        p.percent  = s.percent;
        p.ts = QDateTime::currentDateTime();
        emit scanProgressUpdated(p);
        ++m_scanStep;
        runScanScript();
    });
}

void DemoBridge::tickImu() {
    m_imuPhase += 0.2;
    const bool faultWindow = std::fmod(m_imuPhase, 40.0) > 34.0;
    ImuState imu;
    imu.valid = true;
    imu.roll  = std::sin(m_imuPhase) * 0.6         + (faultWindow ?  2.7 : 0.0);
    imu.pitch = std::cos(m_imuPhase * 0.6) * 0.4    + (faultWindow ? -1.4 : 0.0);
    m_lastRoll = imu.roll;
    m_lastPitch = imu.pitch;
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

void DemoBridge::requestScan(int panStartDdeg, int panEndDdeg,
                              int tiltStartDdeg, int tiltEndDdeg,
                              int stepDdeg, int sensorHeightMm) {
    Q_UNUSED(sensorHeightMm)
    if (m_scanning) {
        emit logLine("SCAN", QStringLiteral("이미 스캔 중 — cmd/scan 무시 (데모)"));
        return;
    }
    m_scanning = true;
    m_scanStep = 0;
    newReqId();
    publishDaemonState("SCANNING");
    emit logLine("SCAN", QString("cmd/scan 발행 (req_id=%1) pan[%2..%3] tilt[%4..%5] step=%6 (데모)")
                              .arg(m_reqId).arg(panStartDdeg).arg(panEndDdeg)
                              .arg(tiltStartDdeg).arg(tiltEndDdeg).arg(stepDdeg));
    runScanScript();
}

void DemoBridge::requestStop() {
    if (!m_scanning) return;
    emit logLine("SCAN", QStringLiteral("cmd/stop 발행 (데모) — 조기 종료"));
    m_scanStep = 5;   // 다음 runScanScript() 호출에서 바로 EXPORT 로
    runScanScript();
}

void DemoBridge::requestHome() {
    emit logLine("SCAN", QStringLiteral("cmd/home 발행 (데모) — 홈 완료 (코어 미지원, 로그만)"));
}

void DemoBridge::requestDisarm() {
    m_scanning = false;
    publishDaemonState("DISARM");

    KitError e;
    e.reqId = m_reqId;
    e.code  = 0;
    e.name  = QStringLiteral("USER_DISARM");
    e.msg   = QStringLiteral("사용자 비상정지 (데모)");
    e.fatal = true;
    e.ts = QDateTime::currentDateTime();
    emit kitErrorReceived(e);
    emit logLine("POWER", QStringLiteral("cmd/disarm 발행 (데모) — 안전정지"));
}

void DemoBridge::requestRearm() {
    if (m_daemonState != "DISARM") return;
    publishDaemonState("IDLE");
    emit logLine("POWER", QStringLiteral("REARM (데모) — DISARM 해제, IDLE 복귀"));
}
