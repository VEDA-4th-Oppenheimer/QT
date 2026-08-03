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
    // 수평 게이트 임계값: "Point Cloud 이후 Camera Automatic Calibration" 문서 권장 초기값(1~2°, 미결/튜닝 대상).
    bool level(double tolDeg = 1.5) const {
        return std::abs(roll) <= tolDeg && std::abs(pitch) <= tolDeg;
    }
};

// 스캔 진행 + 캘리브 품질. 필드는 실제 팀 인터페이스 문서를 따른다:
//   - 스캔 진행(status/progress/scanPoints/expectedPoints/retry)은 RPi 데몬이
//     scan/status·scan/done 으로 발행 (protocol.h v5, daemon_module.h).
//   - 캘리브 품질(edgeRmsePx 이하)은 카메라 단(이영민, OpenCV)이 산출해 발행 —
//     "02. Point Cloud 이후 Camera Automatic Calibration" §14.2 quality/extrinsic 스키마.
//     발행 토픽명은 아직 미정(팀 협의 중)이라 MqttBridge 쪽에 TODO로 표시해 둔다.
struct CalibState {
    QString status = "IDLE";       // IDLE / SCANNING / EXPORT / PASS / FAIL
    int     progress = 0;          // % (SCANNING 중에만 의미 있음)
    quint32 scanPoints = 0;
    quint32 expectedPoints = 0;
    int     retry = 0, maxRetry = 2;   // 문서: 세션당 자동 재시도 최대 2회
    QStringList failureReasons;

    // 품질 지표. NCC는 진단용 참고값일 뿐 단독 PASS 기준이 아니다(문서 §11.3) —
    // edgeRmsePx(초기 목표 3px 이하) + inlierRatio 가 메인 판정 지표.
    double  edgeRmsePx = 0.0;
    double  inlierRatio = 0.0;     // 0..1
    double  ncc = 0.0;

    // extrinsic: translation_m + quaternion_xyzw (문서 §14.2)
    double  translationM[3]   = {0.0, 0.0, 0.0};
    double  quaternionXyzw[4] = {0.0, 0.0, 0.0, 1.0};

    QDateTime stamp;
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
