#include "SpatialProjector.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
#include <algorithm>

SpatialProjector::SpatialProjector() {
    // CH1 기본 Auto RT (기구 기하 기준)
    ExtrinsicRt autoRt1;
    const double defAutoR[9] = {
        -0.8993516327308704, -0.021677413359161492,  0.4366883676655169,
         0.03164503356422315,  0.9929235347745676,   0.11446154787306081,
        -0.43607938790435485,  0.11676019801642104, -0.8923014197030784
    };
    const double defAutoT[3] = { 0.05155019269565374, 0.0786413559428178, 0.035313251335202904 };
    for (int i = 0; i < 9; ++i) autoRt1.r[i] = defAutoR[i];
    for (int i = 0; i < 3; ++i) autoRt1.t[i] = defAutoT[i];
    autoRt1.valid = true;
    m_autoRts.insert(1, autoRt1);

    // CH1 기본 Manual RT (T_camera_lidar_110828.json 차루코 실측 기준)
    ExtrinsicRt manRt1;
    const double defManR[9] = {
        -0.9835214245376979, -0.14262115873630146,  0.11110721198937619,
        -0.07778521792557222,  0.8885863982986189,   0.452066004728247,
        -0.16320253474627378,  0.43597410225769473, -0.8850375781925809
    };
    const double defManT[3] = { -0.040047519204563925, 0.07478418688424698, 0.1357633054748434 };
    for (int i = 0; i < 9; ++i) manRt1.r[i] = defManR[i];
    for (int i = 0; i < 3; ++i) manRt1.t[i] = defManT[i];
    manRt1.valid = true;
    m_manualRts.insert(1, manRt1);

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
    for (int ch = 1; ch <= 4; ++ch) {
        CameraProfile cp = m_profiles.value(ch);
        cp.channel = ch;
        if (cp.imageWidth <= 0) cp.imageWidth = 2592;
        if (cp.imageHeight <= 0) cp.imageHeight = 1520;
        if (cp.fx <= 0.0) cp.fx = 2033.901952;
        if (cp.fy <= 0.0) cp.fy = 2037.779638;
        if (cp.cx <= 0.0) cp.cx = 1337.029701;
        if (cp.cy <= 0.0) cp.cy = 745.370056;
        if (cp.defaultGroundY <= 0.0) cp.defaultGroundY = 1.789;
        if (cp.k1 == 0.0 && cp.k2 == 0.0) {
            cp.k1 = -0.5653174394854492;
            cp.k2 =  0.34459385610316223;
            cp.p1 = -0.0039145365522611315;
            cp.p2 =  0.0008182748566205869;
            cp.k3 = -0.10809412486837452;
        }

        const auto &activeMap = (m_calibMode == CalibrationMode::Automatic) ? m_autoRts : m_manualRts;
        if (activeMap.contains(ch) && activeMap.value(ch).valid) {
            const ExtrinsicRt &rt = activeMap.value(ch);
            for (int i = 0; i < 9; ++i) cp.r[i] = rt.r[i];
            for (int i = 0; i < 3; ++i) cp.t[i] = rt.t[i];
        }
        if (ch == 1 || activeMap.contains(ch)) {
            cp.enabled = true;
            m_profiles.insert(ch, cp);
        }
    }
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

bool SpatialProjector::loadCalibrationResultJson(const QString &filePath, int channel, QString *outSummary) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (outSummary) *outSummary = QStringLiteral("파일을 열 수 없습니다: ") + filePath;
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();
    return loadCalibrationResultData(data, channel, outSummary);
}

static bool parseSingleCalibrationResult(const QJsonObject &obj, int targetChannel, CameraProfile &cp) {
    bool haveR = false;
    bool haveT = false;

    // 1. visualization_t_camera_lidar 형식
    if (obj.contains("visualization_t_camera_lidar") && obj.value("visualization_t_camera_lidar").isObject()) {
        const QJsonObject vt = obj.value("visualization_t_camera_lidar").toObject();
        if (vt.contains("rotation_matrix") && vt.value("rotation_matrix").isArray()) {
            const QJsonArray rMat = vt.value("rotation_matrix").toArray();
            if (rMat.size() == 3) {
                int idx = 0;
                for (int row = 0; row < 3; ++row) {
                    const QJsonArray rRow = rMat.at(row).toArray();
                    for (int col = 0; col < 3 && col < rRow.size(); ++col) {
                        cp.r[idx++] = rRow.at(col).toDouble();
                    }
                }
                if (idx == 9) haveR = true;
            }
        }
        if (vt.contains("translation_m") && vt.value("translation_m").isArray()) {
            const QJsonArray tArr = vt.value("translation_m").toArray();
            if (tArr.size() == 3) {
                for (int i = 0; i < 3; ++i) cp.t[i] = tArr.at(i).toDouble();
                haveT = true;
            }
        }
    }

    // 2. candidate_results 배열에서 선택된 후보 파싱 (fallback)
    if ((!haveR || !haveT) && obj.contains("candidate_results") && obj.value("candidate_results").isArray()) {
        const QJsonArray cands = obj.value("candidate_results").toArray();
        int selIdx = obj.value("selected_candidate").toInt(0);
        if (selIdx < 0 || selIdx >= cands.size()) selIdx = 0;
        if (selIdx < cands.size()) {
            const QJsonObject cand = cands.at(selIdx).toObject();
            const QJsonObject est = cand.contains("estimated") ? cand.value("estimated").toObject()
                                                               : cand.value("diagnostic_candidate").toObject();
            if (est.contains("rotation_matrix") && est.value("rotation_matrix").isArray()) {
                const QJsonArray rMat = est.value("rotation_matrix").toArray();
                if (rMat.size() == 3) {
                    int idx = 0;
                    for (int row = 0; row < 3; ++row) {
                        const QJsonArray rRow = rMat.at(row).toArray();
                        for (int col = 0; col < 3 && col < rRow.size(); ++col) {
                            cp.r[idx++] = rRow.at(col).toDouble();
                        }
                    }
                    if (idx == 9) haveR = true;
                }
            }
            if (est.contains("translation_m") && est.value("translation_m").isArray()) {
                const QJsonArray tArr = est.value("translation_m").toArray();
                if (tArr.size() == 3) {
                    for (int i = 0; i < 3; ++i) cp.t[i] = tArr.at(i).toDouble();
                    haveT = true;
                }
            }
        }
    }

    // 3. 단순 { rotation_matrix: [...], translation_vector / translation_m: [...] } 형식 파싱
    if ((!haveR || !haveT) && obj.contains("rotation_matrix")) {
        const QJsonArray rMat = obj.value("rotation_matrix").toArray();
        if (rMat.size() == 3) {
            int idx = 0;
            for (int row = 0; row < 3; ++row) {
                const QJsonArray rRow = rMat.at(row).toArray();
                for (int col = 0; col < 3 && col < rRow.size(); ++col) {
                    cp.r[idx++] = rRow.at(col).toDouble();
                }
            }
            if (idx == 9) haveR = true;
        }
        const QString tKey = obj.contains("translation_m") ? QStringLiteral("translation_m")
                                                           : QStringLiteral("translation_vector");
        if (obj.contains(tKey) && obj.value(tKey).isArray()) {
            const QJsonArray tArr = obj.value(tKey).toArray();
            if (tArr.size() == 3) {
                for (int i = 0; i < 3; ++i) cp.t[i] = tArr.at(i).toDouble();
                haveT = true;
            }
        }
    }

    // 4. Intrinsics (내부 파라미터) 업데이트 (존재하는 경우)
    if (obj.contains("active_intrinsics") && obj.value("active_intrinsics").isObject()) {
        const QJsonObject inObj = obj.value("active_intrinsics").toObject();
        if (inObj.contains("fx")) cp.fx = inObj.value("fx").toDouble(cp.fx);
        if (inObj.contains("fy")) cp.fy = inObj.value("fy").toDouble(cp.fy);
        if (inObj.contains("cx")) cp.cx = inObj.value("cx").toDouble(cp.cx);
        if (inObj.contains("cy")) cp.cy = inObj.value("cy").toDouble(cp.cy);
        if (inObj.contains("resolution") && inObj.value("resolution").isArray()) {
            const QJsonArray res = inObj.value("resolution").toArray();
            if (res.size() >= 2) {
                cp.imageWidth = res.at(0).toInt(cp.imageWidth);
                cp.imageHeight = res.at(1).toInt(cp.imageHeight);
            }
        }
    }

    return (haveR && haveT);
}

bool SpatialProjector::loadCalibrationResultData(const QByteArray &jsonData, int channel, QString *outSummary) {
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonData, &err);
    if (doc.isNull() || !doc.isObject()) {
        if (outSummary) *outSummary = QStringLiteral("유효하지 않은 JSON 데이터입니다: ") + err.errorString();
        return false;
    }

    const QJsonObject root = doc.object();

    // ── CASE A: 멀티채널 세션 결과 형식 (root 에 "channels" 배열이 있는 경우) ──
    if (root.contains("channels") && root.value("channels").isArray()) {
        const QJsonArray chList = root.value("channels").toArray();
        QStringList appliedDetails;
        QStringList rejectedDetails;
        int appliedCount = 0;

        for (const QJsonValue &cv : chList) {
            if (!cv.isObject()) continue;
            const QJsonObject co = cv.toObject();

            int uiCh = co.value("ui_channel").toInt(0);
            if (uiCh <= 0) {
                uiCh = co.value("sdk_channel").toInt(-1) + 1;
            }
            if (uiCh < 1 || uiCh > 4) continue;

            // 특정 채널만 타깃팅된 경우(channel > 0), 다른 채널은 건너뜀
            if (channel > 0 && uiCh != channel) continue;

            const QString state = co.value("state").toString();
            const QString detail = co.value("detail").toString();
            const QJsonObject resObj = co.value("result").toObject();
            const QString reasonCode = resObj.value("reason_code").toString();

            // 성공/품질통과 여부 판정 (state 가 candidate_ready 이거나 detail 이 INTERNAL_GATE_PASS 이거나 reason_code 가 PASS)
            const bool isPass = (state == QStringLiteral("candidate_ready") ||
                                 detail == QStringLiteral("INTERNAL_GATE_PASS") ||
                                 reasonCode == QStringLiteral("PASS"));

            CameraProfile cp = profile(uiCh);
            cp.channel = uiCh;

            if (isPass && parseSingleCalibrationResult(resObj, uiCh, cp)) {
                cp.enabled = true;
                m_profiles.insert(uiCh, cp);

                ExtrinsicRt rt;
                for (int i = 0; i < 9; ++i) rt.r[i] = cp.r[i];
                for (int i = 0; i < 3; ++i) rt.t[i] = cp.t[i];
                rt.valid = true;
                m_autoRts.insert(uiCh, rt);

                appliedDetails.append(QString::fromUtf8("CH%1 (t=[%2, %3, %4] m)")
                                         .arg(uiCh)
                                         .arg(cp.t[0], 0, 'f', 2)
                                         .arg(cp.t[1], 0, 'f', 2)
                                         .arg(cp.t[2], 0, 'f', 2));
                ++appliedCount;
            } else {
                QString failReason = reasonCode;
                if (failReason.isEmpty()) failReason = detail;
                if (failReason.isEmpty()) failReason = state;
                if (failReason == QStringLiteral("FINALIST_AMBIGUOUS")) {
                    failReason = QStringLiteral("모호성 탈락 (FINALIST_AMBIGUOUS)");
                } else if (failReason == QStringLiteral("OVERLAP_INSUFFICIENT")) {
                    failReason = QStringLiteral("중첩 부족 (OVERLAP_INSUFFICIENT)");
                }
                rejectedDetails.append(QString::fromUtf8("CH%1: %2").arg(uiCh).arg(failReason));
            }
        }

        setCalibrationMode(CalibrationMode::Automatic);

        if (appliedCount > 0) {
            QString summary = QString::fromUtf8("✅ 자동 적용 성공 (%1개 채널):\n• %2")
                                  .arg(appliedCount)
                                  .arg(appliedDetails.join(QStringLiteral("\n• ")));
            if (!rejectedDetails.isEmpty()) {
                summary += QString::fromUtf8("\n\n⚠️ 품질 미달로 제외 (%1개 채널):\n• %2")
                               .arg(rejectedDetails.size())
                               .arg(rejectedDetails.join(QStringLiteral("\n• ")));
            }
            if (outSummary) *outSummary = summary;
            return true;
        } else {
            if (outSummary) {
                *outSummary = QString::fromUtf8("유효한 캘리브레이션 채널이 없습니다.\n제외 사유:\n• %1")
                                  .arg(rejectedDetails.join(QStringLiteral("\n• ")));
            }
            return false;
        }
    }

    // ── CASE B: 단일 채널 결과 파일 형식 ──
    const int targetCh = (channel > 0) ? channel : 1;
    CameraProfile cp = profile(targetCh);
    cp.channel = targetCh;

    if (!parseSingleCalibrationResult(root, targetCh, cp)) {
        if (outSummary) *outSummary = QStringLiteral("JSON에서 회전(R) 및 이동(t) 매트릭스를 찾을 수 없습니다.");
        return false;
    }

    cp.enabled = true;
    m_profiles.insert(targetCh, cp);

    ExtrinsicRt rt;
    for (int i = 0; i < 9; ++i) rt.r[i] = cp.r[i];
    for (int i = 0; i < 3; ++i) rt.t[i] = cp.t[i];
    rt.valid = true;
    m_autoRts.insert(targetCh, rt);

    setCalibrationMode(CalibrationMode::Automatic);

    const QString summaryText = QString::fromUtf8("CH%1 최신 캘리브레이션 R,t 성공적 적용\n"
                                                  "t = [%2, %3, %4] m")
                                    .arg(targetCh)
                                    .arg(cp.t[0], 0, 'f', 4)
                                    .arg(cp.t[1], 0, 'f', 4)
                                    .arg(cp.t[2], 0, 'f', 4);
    if (outSummary) *outSummary = summaryText;

    return true;
}

bool SpatialProjector::loadIntrinsicProfileJson(const QString &filePath, int channel, QString *outSummary) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (outSummary) *outSummary = QStringLiteral("파일을 열 수 없습니다: ") + filePath;
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();
    return loadIntrinsicProfileData(data, channel, outSummary);
}

bool SpatialProjector::loadIntrinsicProfileData(const QByteArray &jsonData, int channel, QString *outSummary) {
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonData, &err);
    if (doc.isNull() || !doc.isObject()) {
        if (outSummary) *outSummary = QStringLiteral("유효하지 않은 JSON 데이터입니다: ") + err.errorString();
        return false;
    }

    const QJsonObject root = doc.object();
    QList<int> targetChannels;
    if (channel >= 1 && channel <= 4) {
        targetChannels.append(channel);
    } else {
        // channel == 0 (전체 자동)
        for (int ch = 1; ch <= 4; ++ch) {
            if (root.contains(QString::number(ch))) targetChannels.append(ch);
        }
        if (targetChannels.isEmpty()) targetChannels.append(1);
    }

    int appliedCount = 0;
    QString lastDetail;

    for (int ch : targetChannels) {
        QJsonObject target = root;
        if (root.contains(QString::number(ch)) && root.value(QString::number(ch)).isObject()) {
            target = root.value(QString::number(ch)).toObject();
        } else if (root.contains("camera") && root.value("camera").isObject()) {
            target = root.value("camera").toObject();
        }

        CameraProfile cp = profile(ch);
        cp.channel = ch;
        bool updated = false;

        QJsonObject intr = target.contains("intrinsic") && target.value("intrinsic").isObject()
                               ? target.value("intrinsic").toObject()
                               : target;

        if (intr.contains("fx")) { cp.fx = intr.value("fx").toDouble(cp.fx); updated = true; }
        if (intr.contains("fy")) { cp.fy = intr.value("fy").toDouble(cp.fy); updated = true; }
        if (intr.contains("cx")) { cp.cx = intr.value("cx").toDouble(cp.cx); updated = true; }
        if (intr.contains("cy")) { cp.cy = intr.value("cy").toDouble(cp.cy); updated = true; }

        if (target.contains("distortion") && target.value("distortion").isArray()) {
            const QJsonArray dist = target.value("distortion").toArray();
            if (dist.size() >= 4) {
                cp.k1 = dist.at(0).toDouble();
                cp.k2 = dist.at(1).toDouble();
                cp.p1 = dist.at(2).toDouble();
                cp.p2 = dist.at(3).toDouble();
                cp.k3 = (dist.size() >= 5) ? dist.at(4).toDouble() : 0.0;
                updated = true;
            }
        }

        if (target.contains("resolution") && target.value("resolution").isArray()) {
            const QJsonArray res = target.value("resolution").toArray();
            if (res.size() >= 2) {
                cp.imageWidth = res.at(0).toInt(cp.imageWidth);
                cp.imageHeight = res.at(1).toInt(cp.imageHeight);
                updated = true;
            }
        } else {
            if (target.contains("image_width"))  { cp.imageWidth  = target.value("image_width").toInt(cp.imageWidth); updated = true; }
            if (target.contains("image_height")) { cp.imageHeight = target.value("image_height").toInt(cp.imageHeight); updated = true; }
        }

        if (updated) {
            cp.enabled = true;
            m_profiles.insert(ch, cp);
            ++appliedCount;
            lastDetail = QString::fromUtf8("CH%1: fx=%2, fy=%3, cx=%4, cy=%5 (%6x%7)")
                             .arg(ch)
                             .arg(cp.fx, 0, 'f', 2)
                             .arg(cp.fy, 0, 'f', 2)
                             .arg(cp.cx, 0, 'f', 2)
                             .arg(cp.cy, 0, 'f', 2)
                             .arg(cp.imageWidth)
                             .arg(cp.imageHeight);
        }
    }

    if (appliedCount == 0) {
        if (outSummary) *outSummary = QStringLiteral("JSON에서 카메라 내부 파라미터(fx, fy, cx, cy 등)를 찾을 수 없습니다.");
        return false;
    }

    if (outSummary) {
        if (appliedCount == 1) {
            *outSummary = QString::fromUtf8("카메라 내부 파라미터 적용 완료\n%1").arg(lastDetail);
        } else {
            *outSummary = QString::fromUtf8("총 %1개 채널 내부 파라미터 적용 완료\n마지막: %2").arg(appliedCount).arg(lastDetail);
        }
    }
    return true;
}

bool SpatialProjector::loadManualExtrinsicJson(const QString &filePath, int channel, QString *outSummary) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (outSummary) *outSummary = QStringLiteral("파일을 열 수 없습니다: ") + filePath;
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();
    return loadManualExtrinsicData(data, channel, outSummary);
}

bool SpatialProjector::loadManualExtrinsicData(const QByteArray &jsonData, int channel, QString *outSummary) {
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonData, &err);
    if (doc.isNull() || !doc.isObject()) {
        if (outSummary) *outSummary = QStringLiteral("유효하지 않은 JSON 데이터입니다: ") + err.errorString();
        return false;
    }

    const QJsonObject root = doc.object();
    QList<int> targetChannels;
    if (channel >= 1 && channel <= 4) {
        targetChannels.append(channel);
    } else {
        // channel == 0 (전체 자동)
        for (int ch = 1; ch <= 4; ++ch) {
            if (root.contains(QString::number(ch))) targetChannels.append(ch);
        }
        if (targetChannels.isEmpty()) targetChannels.append(1);
    }

    int appliedCount = 0;
    QString lastDetail;

    for (int ch : targetChannels) {
        QJsonObject target = root;
        if (root.contains(QString::number(ch)) && root.value(QString::number(ch)).isObject()) {
            target = root.value(QString::number(ch)).toObject();
        }

        QJsonObject ext = target.contains("extrinsic") && target.value("extrinsic").isObject()
                              ? target.value("extrinsic").toObject()
                              : target;

        bool haveR = false;
        bool haveT = false;
        double r[9] = {0};
        double t[3] = {0};

        if (ext.contains("rotation_matrix") && ext.value("rotation_matrix").isArray()) {
            const QJsonArray rMat = ext.value("rotation_matrix").toArray();
            if (rMat.size() == 3 && rMat.at(0).isArray()) {
                int idx = 0;
                for (int row = 0; row < 3; ++row) {
                    const QJsonArray rRow = rMat.at(row).toArray();
                    for (int col = 0; col < 3 && col < rRow.size(); ++col) {
                        r[idx++] = rRow.at(col).toDouble();
                    }
                }
                if (idx == 9) haveR = true;
            } else if (rMat.size() == 9) {
                for (int i = 0; i < 9; ++i) r[i] = rMat.at(i).toDouble();
                haveR = true;
            }
        } else if (ext.contains("r") && ext.value("r").isArray()) {
            const QJsonArray rArr = ext.value("r").toArray();
            if (rArr.size() == 9) {
                for (int i = 0; i < 9; ++i) r[i] = rArr.at(i).toDouble();
                haveR = true;
            }
        }

        QString tKey;
        if (ext.contains("translation_m")) tKey = QStringLiteral("translation_m");
        else if (ext.contains("translation_vector")) tKey = QStringLiteral("translation_vector");
        else if (ext.contains("t")) tKey = QStringLiteral("t");

        if (!tKey.isEmpty() && ext.value(tKey).isArray()) {
            const QJsonArray tArr = ext.value(tKey).toArray();
            if (tArr.size() == 3) {
                for (int i = 0; i < 3; ++i) t[i] = tArr.at(i).toDouble();
                haveT = true;
            }
        }

        if (haveR && haveT) {
            ExtrinsicRt rt;
            for (int i = 0; i < 9; ++i) rt.r[i] = r[i];
            for (int i = 0; i < 3; ++i) rt.t[i] = t[i];
            rt.valid = true;
            m_manualRts.insert(ch, rt);

            CameraProfile cp = profile(ch);
            cp.channel = ch;
            for (int i = 0; i < 9; ++i) cp.r[i] = r[i];
            for (int i = 0; i < 3; ++i) cp.t[i] = t[i];
            cp.enabled = true;
            m_profiles.insert(ch, cp);

            ++appliedCount;
            lastDetail = QString::fromUtf8("CH%1: t = [%2, %3, %4] m")
                             .arg(ch)
                             .arg(t[0], 0, 'f', 4)
                             .arg(t[1], 0, 'f', 4)
                             .arg(t[2], 0, 'f', 4);
        }
    }

    if (appliedCount == 0) {
        if (outSummary) *outSummary = QStringLiteral("JSON에서 외부 파라미터(rotation_matrix 및 translation_m)를 찾을 수 없습니다.");
        return false;
    }

    // 수동 RT 모드로 즉시 전환 및 적용
    setCalibrationMode(CalibrationMode::Manual);

    if (outSummary) {
        if (appliedCount == 1) {
            *outSummary = QString::fromUtf8("Manual RT 적용 완료\n%1\n수동 RT 모드로 즉시 전환되었습니다.").arg(lastDetail);
        } else {
            *outSummary = QString::fromUtf8("총 %1개 채널 Manual RT 적용 완료\n%2\n수동 RT 모드로 즉시 전환되었습니다.").arg(appliedCount).arg(lastDetail);
        }
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
