#pragma once
#include <QString>
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
    bool level(double tolDeg = 1.0) const {
        return std::abs(roll) <= tolDeg && std::abs(pitch) <= tolDeg;
    }
};

struct CalibState {
    double ncc = 0.0;
    double reprojPx = 0.0;
    int    retry = 0, maxRetry = 3;
    int    progress = 0;           // %
    int    scanPoints = 0;
    double coverage = 0.0;         // 3D map coverage 0..1
    int    inliers = 0, candidateLines = 0;
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
