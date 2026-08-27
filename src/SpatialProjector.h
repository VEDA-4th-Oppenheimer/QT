#pragma once

#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QMap>
#include <QString>
#include <QDateTime>

enum class CalibrationMode {
    Automatic,
    Manual
};

struct CameraProfile {
    int channel = 1;
    double fx = 2033.901952;
    double fy = 2037.779638;
    double cx = 1337.029701;
    double cy = 745.370056;
    int imageWidth = 2592;
    int imageHeight = 1520;

    // Distortion coefficients: [k1, k2, p1, p2, k3] (OpenCV RadTan)
    double k1 = -0.5653174394854492;
    double k2 =  0.34459385610316223;
    double p1 = -0.0039145365522611315;
    double p2 =  0.0008182748566205869;
    double k3 = -0.10809412486837452;

    // Extrinsics: P_cam = R * P_lidar + t (최신 카메라 이동 후 캘리브레이션 반영)
    // 3x3 rotation matrix (row-major)
    double r[9] = {
        -0.8993516327308704, -0.021677413359161492,  0.4366883676655169,
         0.03164503356422315,  0.9929235347745676,   0.11446154787306081,
        -0.43607938790435485,  0.11676019801642104, -0.8923014197030784
    };
    // 3x1 translation vector [tx, ty, tz] in meters
    double t[3] = { 0.05155019269565374, 0.0786413559428178, 0.035313251335202904 };

    // Default ground plane Y in LiDAR frame (in meters, 기본값 1.789m)
    double defaultGroundY = 1.789;
    bool enabled = true;
};

struct TrackedObjectSmooth {
    QPointF posM;
    qint64 lastSeenMs = 0;
    bool initialized = false;
};

class SpatialProjector {
public:
    static SpatialProjector &instance();

    SpatialProjector();

    // 캘리브레이션 RT 모드 전환 (Automatic <-> Manual)
    void setCalibrationMode(CalibrationMode mode);
    CalibrationMode calibrationMode() const;
    QString calibrationModeName() const;

    void setProfile(int channel, const CameraProfile &profile);
    CameraProfile profile(int channel) const;
    bool hasProfile(int channel) const;

    // 실측 바닥 높이(센서 높이) 및 방 경계(PCD Boundary) 설정
    void setGlobalGroundY(double y);
    double globalGroundY() const;
    void setRoomBounds(double xMin, double xMax, double zMin, double zMax);

    // Load calibration profiles from JSON file
    bool loadProfiles(const QString &filePath);

    // OpenSDK 또는 자동 캘리브레이션 산출물(result.json / calibration_result.json) 로드 및 즉시 반영
    bool loadCalibrationResultJson(const QString &filePath, int channel = 1, QString *outSummary = nullptr);
    bool loadCalibrationResultData(const QByteArray &jsonData, int channel = 1, QString *outSummary = nullptr);

    // 카메라 내부 파라미터 profile (camera.json 또는 calibration_profiles.json 형식) 로드 및 즉시 반영
    bool loadIntrinsicProfileJson(const QString &filePath, int channel = 1, QString *outSummary = nullptr);
    bool loadIntrinsicProfileData(const QByteArray &jsonData, int channel = 1, QString *outSummary = nullptr);

    // 수동 RT (T_camera_lidar_*.json 또는 manual_rt.json 형식) 로드 및 즉시 반영
    bool loadManualExtrinsicJson(const QString &filePath, int channel = 1, QString *outSummary = nullptr);
    bool loadManualExtrinsicData(const QByteArray &jsonData, int channel = 1, QString *outSummary = nullptr);

    // 픽셀 좌표 (u, v)를 렌즈 역왜곡 보정(Undistort) 후 지면 평면과의 교점으로 투영
    bool projectImageToGround(int channel,
                              double u,
                              double v,
                              double groundY,
                              QPointF *outTopViewPosM,
                              double *outDistanceM = nullptr) const;

    // PERSON BoundingBox의 발 위치(하단 중심)를 착석/가림 보정, 역왜곡, 방 경계 가드, 시간적 스무딩 적용하여 1:1 투영
    bool projectBBoxToGround(int channel,
                             const QRectF &pixelBBox,
                             double groundY,
                             QPointF *outTopViewPosM,
                             double *outDistanceM = nullptr,
                             const QString &trackId = QString());

private:
    void applyModeProfiles();

    CalibrationMode m_calibMode = CalibrationMode::Automatic;
    QMap<int, CameraProfile> m_profiles;
    double m_globalGroundY = 0.0;

    // 모드별 RT (동적 로드 지원)
    double m_autoR[9] = {
        -0.8993516327308704, -0.021677413359161492,  0.4366883676655169,
         0.03164503356422315,  0.9929235347745676,   0.11446154787306081,
        -0.43607938790435485,  0.11676019801642104, -0.8923014197030784
    };
    double m_autoT[3] = { 0.05155019269565374, 0.0786413559428178, 0.035313251335202904 };
    double m_manualR[9] = {
        -0.9835214245376979, -0.14262115873630146,  0.11110721198937619,
        -0.07778521792557222,  0.8885863982986189,   0.452066004728247,
        -0.16320253474627378,  0.43597410225769473, -0.8850375781925809
    };
    double m_manualT[3] = { -0.040047519204563925, 0.07478418688424698, 0.1357633054748434 };
    bool   m_hasCustomManualRt = false;

    // PCD 실측 방 물리적 경계 (사무실 기본 크기)
    double m_roomXMin = -5.0;
    double m_roomXMax = 5.0;
    double m_roomZMin = -5.0;
    double m_roomZMax = 5.0;
    bool   m_haveCustomRoomBounds = false;

    // 시간적 궤적 안정화 추적기
    QMap<QString, TrackedObjectSmooth> m_smoothers;
};
