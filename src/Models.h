#pragma once
#include <QString>
#include <QStringList>
#include <QPointF>
#include <QVector>
#include <QDateTime>
#include <cmath>

struct ChannelState {
    int      no        = 1;
    QString  name;                 // 타일 헤더에 CH 번호 옆으로 붙는 이름. 지금은
                                   // 비워 둔다(설치 위치를 지어내지 않는다) —
                                   // MainWindow::buildDashboardTab 주석 참고.
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
    // 수평 게이트 임계값: 데몬의 LEVEL_GATE_MAX_DEG 와 같은 값(10.0°)으로 맞춰둔다.
    //
    // 원래는 "Point Cloud 이후 Camera Automatic Calibration" 문서 권장값(1~2°)을
    //   따라 1.5° 였는데, 그러면 데몬은 스캔을 정상적으로 도는데 화면에만
    //   "기울었다" 배너가 뜨는 상태가 계속돼서 배너가 무의미해졌다. 브링업
    //   동안은 데몬 게이트와 같은 기준으로 두고, 배너가 뜨면 실제로 스캔도
    //   거부되는 상태이도록 한다.
    //
    //   근본 원인은 킷이 실제로 3.6° 기울어 있는데 그 자세를 IMU 설치각
    //   오프셋의 기준으로 잡아버린 것이다. 거치를 바닥평면 기준으로 바로잡고
    //   오프셋을 다시 재면 데몬 쪽을 3.0° 이하로 조일 수 있고, 그때 여기도
    //   같이 조여야 한다(데몬 값과 항상 같이 움직인다).
    bool level(double tolDeg = 10.0) const {
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

// adts/state/daemon 의 diag — STM32 펌웨어가 CMD_STATUS 로 1초마다 올리는
// 누적 진단 카운터(proto v6). 부팅 이후 누적이고 65535 에서 포화한다.
//
// 주의: valid 가 false 면 아래 값은 **"모른다"** 이지 "정상" 이 아니다.
//   STM32 에 구버전 펌웨어가 올라가 있거나(그쪽은 CMD_STATUS 를 아예 안 보낸다)
//   아직 첫 주기가 안 온 것이다. 0 을 그대로 초록불로 그리면 안 된다.
//
// 각 값이 뜻하는 것:
//   txFail      STM32 의 UART 송신 실패. 0 이 아니면 상행 프레임이 유실됐다는
//               뜻이라 스캔 점 수가 실제보다 적을 수 있다.
//   rxOvf       STM32 수신 링버퍼 오버플로. 0 이 아니면 **STM32 메인루프가
//               오래 막혔다** 는 뜻이다(256B = 하행 프레임 20개분).
//   encRetry    엔코더 I2C 판독 재시도(양축 합). 계속 오르면 배선 접촉 의심.
//   lidarDrop   라이다 큐가 차서 버린 샘플. 격자에 빈 셀로 남는다.
//   rejectBusy  진행 중이라 거절한 SCAN_START. 조작자가 중복으로 눌렀다는 뜻.
struct StmDiag {
    bool    valid = false;
    quint32 txFail = 0, rxOvf = 0, encRetry = 0, lidarDrop = 0, rejectBusy = 0;
};

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
    // 그 오류가 난 축 (proto v6). 0=축무관 / 1=팬 / 2=틸트 / 3=양축.
    // 비트 플래그다 — 1=bit0(팬), 2=bit1(틸트) 이라 3 은 둘 다라는 뜻이다.
    int      lastErrAxis = 0;
    StmDiag  diag;
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

// adts/event/error — 계약 §3.5.
//
// code 로 **출처**가 갈린다 (데몬 daemon_module.h, 2026-08-13 정리):
//     < 100   STM32 가 CMD_ERROR 로 올린 것 (protocol.h proto_err_code)
//               1 BAD_CRC / 2 BAD_LEN / 3 NOT_HOMED
//               4 OUT_OF_RANGE / 5 STALL / 6 LIDAR
//    >= 100   데몬 자신의 판정
//             100 DISARM         안전정지
//             101 HOME_TIMEOUT   홈 무응답으로 요청 취소
//             102 NOT_LEVEL      수평 게이트가 스캔을 거부
//             103 UPLOAD_FAIL    카메라 업로드 실패 (파일은 로컬에 남는다)
//             104 BAD_REQUEST    cmd 페이로드 필드 누락/형식 오류
//             105 EXPORT_FAIL    산출물 기록 실패 — **측정값 복구 불가**
//             106 BUSY           지금 상태에서 받을 수 없는 요청
//
// 이 경계가 있기 전에는 4 하나가 세 뜻으로 쓰여서, code=4 를 받고도 STM 이
// 거절한 건지 데몬이 페이로드를 못 읽은 건지 구분할 수 없었다.
//
// STM 오류는 항상 name="STM_ERROR"(코드별 세부 이름 아님)로 온다.
// 데몬 판정은 name 이 "ERR_NOT_LEVEL" 처럼 구체적으로 온다.
//
// fatal 은 이제 데몬이 실제로 채운다(2026-08-12). 정의가 "하드웨어가
// 고장났나" 가 아니라 **화면을 어떻게 그릴까** 라는 점에 유의:
//     true   작업이 멈췄고 사용자가 개입해야 한다 — 배너·모달
//            (3 NOT_HOMED / 5 STALL / 6 LIDAR / 100 안전정지,
//             101 홈 타임아웃, 102 수평 NG, 105 산출 실패)
//     false  로그 한 줄이면 된다. 계속되거나 다시 시도하면 된다
//            (1 BAD_CRC / 2 BAD_LEN / 4 OUT_OF_RANGE)
struct KitError {
    QString reqId;
    int     code = 0;
    QString name, msg;
    bool    fatal = false;
    // 오류가 난 축 (proto v6). 0=축무관 / 1=팬 / 2=틸트 / 3=양축.
    // 비트 플래그라 3 은 "둘 다" 다. 데몬 자체 판정(100+)은 항상 0.
    int     axis = 0;
    QDateTime ts;
};

// 축 코드를 사람이 읽는 말로. 축과 무관하면 빈 문자열이라 그대로 이어 붙여도
// 된다("[3] STM_ERROR ... (팬)" / "[100] ERR_DISARM ...").
inline QString axisLabel(int axis) {
    switch (axis) {
    case 1:  return QStringLiteral("팬");
    case 2:  return QStringLiteral("틸트");
    case 3:  return QStringLiteral("팬·틸트");
    default: return QString();
    }
}

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
