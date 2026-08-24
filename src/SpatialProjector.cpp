#include "SpatialProjector.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
#include <algorithm>

SpatialProjector::SpatialProjector() {
    applyModeProfiles();
}

SpatialProjector &SpatialProjector::instance() {
    static SpatialProjector instance;
    return instance;
}

void SpatialProjector::setCalibrationMode(CalibrationMode mode) {
    m_calibMode = mode;
    applyModeProfiles();
}

CalibrationMode SpatialProjector::calibrationMode() const {
    return m_calibMode;
}

QString SpatialProjector::calibrationModeName() const {
    return m_calibMode == CalibrationMode::Automatic ? QStringLiteral("AUTOMATIC") : QStringLiteral("MANUAL");
}

void SpatialProjector::applyModeProfiles() {
    CameraProfile cp1;
    cp1.channel = 1;
    cp1.imageWidth = 2592;
    cp1.imageHeight = 1520;
    cp1.fx = 2033.901952;
    cp1.fy = 2037.779638;
    cp1.cx = 1337.029701;
    cp1.cy = 745.370056;
    cp1.defaultGroundY = 1.789;
    cp1.k1 = -0.5653174394854492;
    cp1.k2 =  0.34459385610316223;
    cp1.p1 = -0.0039145365522611315;
    cp1.p2 =  0.0008182748566205869;
    cp1.k3 = -0.10809412486837452;

    if (m_calibMode == CalibrationMode::Automatic) {
        // 최신 카메라 이동 후 캘리브레이션 RT (ch1_20260819_coverage_gate50_holdout_pair3_v1)
        const double autoR[9] = {
            -0.8993516327308704, -0.021677413359161492,  0.4366883676655169,
             0.03164503356422315,  0.9929235347745676,   0.11446154787306081,
            -0.43607938790435485,  0.11676019801642104, -0.8923014197030784
        };
        const double autoT[3] = { 0.05155019269565374, 0.0786413559428178, 0.035313251335202904 };
        for (int i = 0; i < 9; ++i) cp1.r[i] = autoR[i];
        for (int i = 0; i < 3; ++i) cp1.t[i] = autoT[i];
    } else {
        // Manual Calibration 실측 RT (T_camera_lidar_110828.json 차루코 보드 기준)
        const double manualR[9] = {
            -0.9835214245376979, -0.14262115873630146,  0.11110721198937619,
            -0.07778521792557222,  0.8885863982986189,   0.452066004728247,
            -0.16320253474627378,  0.43597410225769473, -0.8850375781925809
        };
        const double manualT[3] = { -0.040047519204563925, 0.07478418688424698, 0.1357633054748434 };
        for (int i = 0; i < 9; ++i) cp1.r[i] = manualR[i];
        for (int i = 0; i < 3; ++i) cp1.t[i] = manualT[i];
    }

    m_profiles.insert(1, cp1);
}

bool SpatialProjector::hasProfile(int channel) const {
    return m_profiles.contains(channel) && m_profiles.value(channel).enabled;
}

CameraProfile SpatialProjector::profile(int channel) const {
    static const CameraProfile defaultProf;
    return m_profiles.value(channel, defaultProf);
}

void SpatialProjector::setProfile(int channel, const CameraProfile &profile) {
    m_profiles.insert(channel, profile);
}

void SpatialProjector::setGlobalGroundY(double y) {
    if (y > 0.1 && std::isfinite(y)) {
        m_globalGroundY = y;
    }
}

double SpatialProjector::globalGroundY() const {
    return m_globalGroundY;
}

void SpatialProjector::setRoomBounds(double xMin, double xMax, double zMin, double zMax) {
    if (xMax > xMin && zMax > zMin) {
        m_roomXMin = xMin;
        m_roomXMax = xMax;
        m_roomZMin = zMin;
        m_roomZMax = zMax;
        m_haveCustomRoomBounds = true;
    }
}

bool SpatialProjector::loadProfiles(const QString &jsonFilePath) {
    QFile file(jsonFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return true;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (doc.isNull() || !doc.isObject()) return false;

    const QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        bool ok = false;
        const int ch = it.key().toInt(&ok);
        if (!ok || !it.value().isObject()) continue;

        const QJsonObject obj = it.value().toObject();
        CameraProfile cp;
        cp.channel = ch;
        cp.imageWidth = obj.value("image_width").toInt(2592);
        cp.imageHeight = obj.value("image_height").toInt(1520);
        cp.fx = obj.value("fx").toDouble(2033.90);
        cp.fy = obj.value("fy").toDouble(2037.78);
        cp.cx = obj.value("cx").toDouble(1337.03);
        cp.cy = obj.value("cy").toDouble(745.37);
        cp.defaultGroundY = obj.value("ground_y").toDouble(1.789);

        if (obj.contains("distortion") && obj.value("distortion").isArray()) {
            const QJsonArray distArr = obj.value("distortion").toArray();
            if (distArr.size() >= 4) {
                cp.k1 = distArr.at(0).toDouble(cp.k1);
                cp.k2 = distArr.at(1).toDouble(cp.k2);
                cp.p1 = distArr.at(2).toDouble(cp.p1);
                cp.p2 = distArr.at(3).toDouble(cp.p2);
                if (distArr.size() >= 5) cp.k3 = distArr.at(4).toDouble(cp.k3);
            }
        }

        if (obj.contains("r") && obj.value("r").isArray()) {
            const QJsonArray rArr = obj.value("r").toArray();
            if (rArr.size() == 9) {
                for (int i = 0; i < 9; ++i) cp.r[i] = rArr.at(i).toDouble();
            }
        }
        if (obj.contains("t") && obj.value("t").isArray()) {
            const QJsonArray tArr = obj.value("t").toArray();
            if (tArr.size() == 3) {
                for (int i = 0; i < 3; ++i) cp.t[i] = tArr.at(i).toDouble();
            }
        }
        cp.enabled = obj.value("enabled").toBool(true);
        m_profiles.insert(ch, cp);
    }
    return true;
}

bool SpatialProjector::projectImageToGround(int channel,
                                           double u,
                                           double v,
                                           double groundY,
                                           QPointF *outTopViewPosM,
                                           double *outDistanceM) const {
    if (!hasProfile(channel) || !outTopViewPosM) return false;
    const CameraProfile &cp = m_profiles.value(channel);

    // 정규화 좌표 (0~1 범위)로 들어온 경우 이미지 해상도 스케일링 적용
    if (u >= 0.0 && u <= 1.0 && v >= 0.0 && v <= 1.0 && cp.imageWidth > 0 && cp.imageHeight > 0) {
        u *= cp.imageWidth;
        v *= cp.imageHeight;
    }

    if (std::abs(cp.fx) < 1e-6 || std::abs(cp.fy) < 1e-6) return false;

    // 1. 왜곡된 픽셀 좌표를 정규화 좌표로 변환
    const double xd = (u - cp.cx) / cp.fx;
    const double yd = (v - cp.cy) / cp.fy;

    // 2. OpenCV RadTan (Brown-Conrady) 렌즈 역왜곡 보정 (Iterative Undistortion)
    double x = xd;
    double y = yd;
    if (std::abs(cp.k1) > 1e-6 || std::abs(cp.k2) > 1e-6 || std::abs(cp.p1) > 1e-6) {
        for (int iter = 0; iter < 8; ++iter) {
            const double r2 = x * x + y * y;
            const double r4 = r2 * r2;
            const double r6 = r4 * r2;
            const double radial = 1.0 + cp.k1 * r2 + cp.k2 * r4 + cp.k3 * r6;
            const double dx = 2.0 * cp.p1 * x * y + cp.p2 * (r2 + 2.0 * x * x);
            const double dy = cp.p1 * (r2 + 2.0 * y * y) + 2.0 * cp.p2 * x * y;
            if (std::abs(radial) > 1e-6) {
                x = (xd - dx) / radial;
                y = (yd - dy) / radial;
            }
        }
    }

    // 3. 왜곡 보정된 이상적 카메라 광선 방향 벡터
    const double d_cam_x = x;
    const double d_cam_y = y;
    const double d_cam_z = 1.0;

    // 4. LiDAR 좌표계에서의 카메라 원점 위치: C_lidar = -R^T * t
    const double c_lidar_x = -(cp.r[0] * cp.t[0] + cp.r[3] * cp.t[1] + cp.r[6] * cp.t[2]);
    const double c_lidar_y = -(cp.r[1] * cp.t[0] + cp.r[4] * cp.t[1] + cp.r[7] * cp.t[2]);
    const double c_lidar_z = -(cp.r[2] * cp.t[0] + cp.r[5] * cp.t[1] + cp.r[8] * cp.t[2]);

    // 5. LiDAR 좌표계에서의 광선 방향 벡터: D_lidar = R^T * d_cam
    const double d_lidar_x = cp.r[0] * d_cam_x + cp.r[3] * d_cam_y + cp.r[6] * d_cam_z;
    const double d_lidar_y = cp.r[1] * d_cam_x + cp.r[4] * d_cam_y + cp.r[7] * d_cam_z;
    const double d_lidar_z = cp.r[2] * d_cam_x + cp.r[5] * d_cam_y + cp.r[8] * d_cam_z;

    // 6. 지면 평면 Y_lidar = targetGroundY 와의 교점 계산
    double targetGroundY = cp.defaultGroundY;
    if (m_globalGroundY > 0.1) targetGroundY = m_globalGroundY;
    else if (groundY > 0.1) targetGroundY = groundY;

    // 실내 광각 카메라 하향 지면 투영: 수평선 부근이라도 실효 하향각(effectiveDy >= 0.04)을 보장하여 누락 방지
    const double effectiveDy = qMax(0.04, d_lidar_y);
    const double heightDelta = qMax(0.1, targetGroundY - c_lidar_y);
    const double lambda = heightDelta / effectiveDy;

    // 실내 최대 투영 거리 안전 캡 (8m) — 카메라 하향각이 얕을 때 거리 폭발 방지
    static constexpr double kMaxLambda = 50.0;  // ray parameter cap
    static constexpr double kMaxDist   = 8.0;   // 결과 거리 cap (미터)
    const double safeLambda = std::min(lambda, kMaxLambda);

    double p_lidar_x = c_lidar_x + safeLambda * d_lidar_x;
    double p_lidar_z = c_lidar_z + safeLambda * d_lidar_z;

    const double rawDist = std::sqrt(p_lidar_x * p_lidar_x + p_lidar_z * p_lidar_z);
    if (rawDist > kMaxDist) {
        // 방향은 유지하되 거리를 kMaxDist로 축소
        const double scale = kMaxDist / rawDist;
        p_lidar_x *= scale;
        p_lidar_z *= scale;
    }

    // Top-View 평면 좌표: +x = LiDAR X (오른쪽), +y = LiDAR Z (전방/북)
    *outTopViewPosM = QPointF(p_lidar_x, p_lidar_z);
    if (outDistanceM) {
        *outDistanceM = std::sqrt(p_lidar_x * p_lidar_x + p_lidar_z * p_lidar_z);
    }
    return true;
}

bool SpatialProjector::projectBBoxToGround(int channel,
                                          const QRectF &pixelBBox,
                                          double groundY,
                                          QPointF *outTopViewPosM,
                                          double *outDistanceM,
                                          const QString &trackId) {
    if (pixelBBox.isNull() && pixelBBox.isEmpty()) return false;
    if (!hasProfile(channel) || !outTopViewPosM) return false;

    const CameraProfile &cp = m_profiles.value(channel);
    const double bw = qMax(2.0, pixelBBox.width());
    const double bh = qMax(2.0, pixelBBox.height());
    const double aspect = bh / bw;

    double targetU = pixelBBox.center().x();
    double targetV = pixelBBox.bottom();

    // BBox가 상체/머리 위주로 잡혀 상단에 있는 경우 발 위치 추정 복원
    if (pixelBBox.bottom() < cp.imageHeight * 0.70 || aspect < 1.6) {
        const double estimatedFootV = pixelBBox.top() + bw * 2.8;
        if (estimatedFootV <= cp.imageHeight * 0.98) {
            targetV = estimatedFootV;
        }
    }

    double baseGroundY = cp.defaultGroundY;
    if (m_globalGroundY > 0.1) baseGroundY = m_globalGroundY;
    else if (groundY > 0.1) baseGroundY = groundY;

    QPointF rawPos;
    double rawDist = 0.0;
    if (!projectImageToGround(channel, targetU, targetV, baseGroundY, &rawPos, &rawDist)) {
        return false;
    }

    // 방 물리적 경계 안전 클리핑 (PCD 실측 벽면 밖으로 나가지 않도록 가드)
    const double clampedX = qBound(m_roomXMin, rawPos.x(), m_roomXMax);
    const double clampedZ = qBound(m_roomZMin, rawPos.y(), m_roomZMax);
    const QPointF clampedPos(clampedX, clampedZ);

    // 시간적 궤적 스무딩 필터 (다중 객체 독립 EMA 트래커)
    const QString key = QString("CH%1_%2").arg(channel).arg(trackId.isEmpty() ? QStringLiteral("default") : trackId);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    QPointF finalPos = clampedPos;
    if (m_smoothers.contains(key)) {
        TrackedObjectSmooth &ts = m_smoothers[key];
        const qint64 dt = now - ts.lastSeenMs;
        if (ts.initialized && dt < 2500) {
            const double alpha = 0.35;
            finalPos = ts.posM * (1.0 - alpha) + clampedPos * alpha;
        }
        ts.posM = finalPos;
        ts.lastSeenMs = now;
        ts.initialized = true;
    } else {
        TrackedObjectSmooth ts;
        ts.posM = finalPos;
        ts.lastSeenMs = now;
        ts.initialized = true;
        m_smoothers.insert(key, ts);
    }

    // 오래된 추적기 정리 (> 5초)
    for (auto it = m_smoothers.begin(); it != m_smoothers.end();) {
        if (now - it.value().lastSeenMs > 5000) it = m_smoothers.erase(it);
        else ++it;
    }

    *outTopViewPosM = finalPos;
    if (outDistanceM) {
        *outDistanceM = std::hypot(finalPos.x(), finalPos.y());
    }
    return true;
}
