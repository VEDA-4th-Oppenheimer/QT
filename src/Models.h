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
    // 기본값은 false 여야 한다 — 위젯들이 생성자에서 setImu({}) 로 자기를 초기화하는데,
    // 여기가 true 면 "수신 전"이 roll=0/pitch=0 인 정상 수평 상태로 초록색 표시된다
    // (데이터가 아예 안 와도 킷이 멀쩡해 보임). false 라야 N/A 로 뜬다.
    bool   valid = false;   // false = 아직 측정 없음/IMU 미구현 — 화면에 표시하지 말 것(계약 §3.3)
    // 수평 게이트 임계값: "Point Cloud 이후 Camera Automatic Calibration" 문서 권장 초기값(1~2°, 미결/튜닝 대상).
    bool level(double tolDeg = 1.5) const {
        return std::abs(roll) <= tolDeg && std::abs(pitch) <= tolDeg;
    }
};

// ---------------------------------------------------------------------------
// 아래 4개 구조체는 MQTT_INTERFACE_CONTRACT.md v1.0 (데몬=이현우/Qt=송영빈/
// 브로커=이광진 서명)의 JSON 키를 기준으로 하되, RPi develop 브랜치 실구현
// (daemon/modules/mqtt/mqtt_module.c, 2026-08-03 mainvoid00)에 맞춰 조정했다.
// 문서와 실구현이 다른 두 지점:
//   1. 토픽에 kit_id 세그먼트가 없다 — 계약 §2는 "adts/kit1/..."이지만 실구현은
//      "adts/..." (브로커가 킷마다 상주하므로 kit_id 가 중복 정보라는 판단, 커밋
//      메시지 참고). 바뀌면 다시 상의할 것.
//   2. state/scan 페이로드가 계약 §3.4보다 적다 — 실구현은
//      {req_id,ok,pcd,points,stm_reported,ts} 만 보낸다. session_id/scan_id/json/
//      rows/columns/expected/duration_s 는 아직 안 보낸다. 필드는 계약대로 남겨두되
//      (나중에 추가되면 코드 변경 없이 채워짐) UI 는 실제로 오는 값만 표시한다.
// ---------------------------------------------------------------------------

// adts/state/daemon (retained) — 계약 §3.3. LWT 로 데몬 사망 시 자동으로
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

// adts/state/scan (retained) — 계약 §3.4. 포인트클라우드 파일 자체는 오지
// 않고 경로만 온다(파일 전달 방식은 미결, §9). session_id/scan_id/jsonPath/
// rows/columns/expected/durationS 는 실구현이 아직 안 보내는 필드 —
// 항상 빈 문자열/0 으로 온다(UI 에서 표시하지 않음, 상단 주석 참고).
struct ScanResult {
    QString reqId;
    bool    ok = false;
    QString sessionId, scanId, pcdPath, jsonPath;
    int     rows = 0, columns = 0;
    quint32 points = 0, expected = 0;
    quint32 stmReported = 0;   // STM32 가 자체 보고한 점 수(대조용) — 실구현 필드
    double  durationS = 0.0;
    QDateTime ts;
};

// adts/event/progress (QoS0, not retained, ~2Hz) — 계약 §3.6. 유실 가정,
// 완료 판정은 DaemonState.state 로 할 것.
struct ScanProgress {
    QString reqId;
    quint32 points = 0, expected = 0;
    int     percent = 0;
    QDateTime ts;
};

// adts/event/error — 계약 §3.5. code 1~6 은 STM32 CMD_ERROR(protocol.h)
// 원본, 100/101 은 데몬이 링크단절/홈타임아웃을 감지해 합성한 코드.
// STM 오류는 항상 name="STM_ERROR"(코드별 세부 이름 아님)로 온다.
//
// fatal 은 이제 데몬이 실제로 채운다(2026-08-12). 정의가 "하드웨어가
// 고장났나" 가 아니라 **화면을 어떻게 그릴까** 라는 점에 유의:
//     true   작업이 멈췄고 사용자가 개입해야 한다 — 배너·모달
//            (3 NOT_HOMED / 5 STALL / 6 LIDAR / 100 안전정지 전부)
//     false  로그 한 줄이면 된다. 계속되거나 다시 시도하면 된다
//            (1 BAD_CRC / 2 BAD_LEN / 4 OUT_OF_RANGE)
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
