#pragma once
#include <QString>
#include <QStringList>
#include <QPointF>
#include <QVector>
#include <QDateTime>
#include <cmath>

struct ChannelState {
    int      no        = 1;
    QString  name;                 // "북측 (0°) · 창측 벽면"
    QString  topic;                // "cctv/ch1/h264"
    bool     online    = false;
    double   fps       = 0.0;
    QString  meta      = "1920x1080 · WiseAI ON";
};

// WiseAI 감지 객체를 캘리브레이션 RT 로 변환한 실내 평면 좌표(미터, 킷 원점)
struct SpatialObject {
    QString  cls;                  // "PERSON" / "VEHICLE"
    QPointF  posM;                 // (x, y) meters
    double   distM = 0.0;
    int      channel = 1;
};

struct ImuState {
    double roll = 0.0, pitch = 0.0;
    bool   valid = true;    // false = 아직 측정 없음/IMU 미구현 — 화면에 표시하지 말 것(계약 §3.3)
    // 수평 게이트 임계값: "Point Cloud 이후 Camera Automatic Calibration" 문서 권장 초기값(1~2°, 미결/튜닝 대상).
    bool level(double tolDeg = 1.5) const {
        return std::abs(roll) <= tolDeg && std::abs(pitch) <= tolDeg;
    }
};

// ---------------------------------------------------------------------------
// 아래 4개 구조체는 MQTT_INTERFACE_CONTRACT.md v1.0 (데몬=이현우/Qt=송영빈/
// 브로커=이광진 서명)을 그대로 따른다. 필드명은 계약서 §3의 JSON 키와 1:1 대응.
// ---------------------------------------------------------------------------

// adts/kit1/state/daemon (retained) — 계약 §3.3. LWT 로 데몬 사망 시 자동으로
// state="OFFLINE", online=false 가 온다.
struct DaemonState {
    QString  state = "OFFLINE";   // IDLE / SCANNING / EXPORT / DISARM / OFFLINE
    bool     online = false;
    bool     linkAlive = false;
    bool     homed = false;
    bool     scanning = false;
    int      curPanDdeg = 0, curTiltDdeg = 0;   // 기구각(§7) — 계약각 아님
    int      lastErr = 0;
    ImuState level;
    QDateTime ts;
};

// adts/kit1/state/scan (retained) — 계약 §3.4. 포인트클라우드 파일 자체는 오지
// 않고 경로만 온다(파일 전달 방식은 미결, §9).
struct ScanResult {
    QString reqId;
    bool    ok = false;
    QString sessionId, scanId, pcdPath, jsonPath;
    int     rows = 0, columns = 0;
    quint32 points = 0, expected = 0;
    double  durationS = 0.0;
    QDateTime ts;
};

// adts/kit1/event/progress (QoS0, not retained, ~2Hz) — 계약 §3.6. 유실 가정,
// 완료 판정은 DaemonState.state 로 할 것.
struct ScanProgress {
    QString reqId;
    quint32 points = 0, expected = 0;
    int     percent = 0;
    QDateTime ts;
};

// adts/kit1/event/error — 계약 §3.5. code 1~6 은 STM32 CMD_ERROR(protocol.h)
// 원본, 100/101 은 데몬이 링크단절/홈타임아웃을 감지해 합성한 코드.
struct KitError {
    QString reqId;
    int     code = 0;
    QString name, msg;
    bool    fatal = false;
    QDateTime ts;
};

// LiDAR 스캔에서 뽑은 실내 벽/기둥 에지 (평면 투영, 미터)
struct MapEdge { QPointF a, b; };

// CALIBRATION 탭 파이프라인 단계 상태
struct CalibStep {
    QString id, desc, state;       // state: PENDING / DONE / PASS
};

// DEVICES / MQTT 탭 상단 장비 카드
struct DeviceInfo {
    QString name, desc, value;
    bool online = true;
};

// DEVICES / MQTT 탭 토픽 테이블 행
struct TopicInfo {
    QString topic, rate, desc, state;  // state: RX / TX / LOST
};

// RGB-D DATASET 탭 세트 테이블 행
struct DatasetEntry {
    QString id, ch, points, ncc, size, capturedAt;
};
