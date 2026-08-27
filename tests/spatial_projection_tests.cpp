#include "SpatialMetadata.h"
#include "SpatialProjector.h"
#include "Models.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <cstdlib>

void testSpatialProjectorCH1() {
    SpatialProjector &projector = SpatialProjector::instance();
    assert(projector.hasProfile(1));

    CameraProfile cp = projector.profile(1);
    assert(cp.channel == 1);
    assert(std::abs(cp.fx - 2033.901952) < 1e-3);

    // 실제 CH1 카메라 사람 감지 실측 BBox (2592x1520 해상도, 의자/책상 앞 사람)
    QRectF bbox(QPointF(1350.0, 600.0), QPointF(1650.0, 1450.0));
    QPointF topViewPos;
    double dist = 0.0;

    bool ok = projector.projectBBoxToGround(1, bbox, 1.789, &topViewPos, &dist, "test1");
    assert(ok);
    assert(dist > 0.5); // 유효한 양수 거리
    std::cout << "[PASS] testSpatialProjectorCH1: BBox -> TopView pos: ("
              << topViewPos.x() << ", " << topViewPos.y() << "), dist: " << dist << " m\n";
}

void testSpatialMetadataOnvifWithProjection() {
    // ODM 실제 스크린샷과 동일한 ONVIF XML 스트림 (사람 BBox)
    const QByteArray onvifXml =
        "<tt:MetadataStream xmlns:tt=\"http://www.onvif.org/ver10/schema\">\n"
        "  <tt:VideoAnalytics>\n"
        "    <tt:Frame UtcTime=\"2026-08-20T02:02:19.573Z\">\n"
        "      <tt:Transformation>\n"
        "        <tt:Translate x=\"-1.0\" y=\"1.0\"/>\n"
        "        <tt:Scale x=\"0.000772\" y=\"-0.001316\"/>\n"
        "      </tt:Transformation>\n"
        "      <tt:Object ObjectId=\"2\" Parent=\"0\">\n"
        "        <tt:Appearance>\n"
        "          <tt:Shape>\n"
        "            <tt:BoundingBox left=\"1350.0\" top=\"600.0\" right=\"1650.0\" bottom=\"1450.0\"/>\n"
        "            <tt:CenterOfGravity x=\"1500.0\" y=\"1025.0\"/>\n"
        "          </tt:Shape>\n"
        "          <tt:Class>\n"
        "            <tt:ClassCandidate>\n"
        "              <tt:Type>Human</tt:Type>\n"
        "              <tt:Likelihood>0.92</tt:Likelihood>\n"
        "            </tt:ClassCandidate>\n"
        "            <tt:Type Likelihood=\"0.92\">Person</tt:Type>\n"
        "          </tt:Class>\n"
        "        </tt:Appearance>\n"
        "      </tt:Object>\n"
        "    </tt:Frame>\n"
        "  </tt:VideoAnalytics>\n"
        "</tt:MetadataStream>";

    spatial_metadata::ParseResult res = spatial_metadata::parse(onvifXml, 1);
    assert(res.recognized);
    assert(res.objects.size() == 1);

    const SpatialObject &obj = res.objects.first();
    assert(obj.id == "2");
    assert(obj.cls == "PERSON"); // Head -> PERSON 정규화 확인
    assert(obj.hasTopViewPosition()); // CH1 프로파일 투영으로 top_view_m 확인
    assert(obj.coordinateFrame == "top_view_m");
    assert(obj.isProjected);
    assert(obj.distM > 0.0);

    std::cout << "[PASS] testSpatialMetadataOnvifWithProjection: parsed and projected obj id="
              << obj.id.toStdString() << ", cls=" << obj.cls.toStdString()
              << ", posM=(" << obj.posM.x() << ", " << obj.posM.y() << ")\n";
}

// 🛡️ 30초 연속(900프레임) 외곽 무이탈 및 시간적 안정화 검증
void test30SecondsContinuousNoBoundaryEscape() {
    SpatialProjector &projector = SpatialProjector::instance();
    projector.setRoomBounds(-6.0, 6.0, -6.0, 6.0);

    // CH1 의자에 앉아 있는 사람 BBox (우측 하단 책상/의자 실측 영역)
    const double baseLeft = 1350.0;
    const double baseTop = 600.0;
    const double baseRight = 1650.0;
    const double baseBottom = 1450.0;

    QPointF prevPos;
    bool hasPrev = false;

    // 30fps * 30초 = 900 프레임
    for (int frame = 0; frame < 900; ++frame) {
        // 프레임 노이즈 및 손 흔들림/착석 자세 변동 시뮬레이션 (±10px 지터)
        const double noiseX = ((frame * 17) % 21) - 10.0;
        const double noiseY = ((frame * 23) % 21) - 10.0;

        QRectF bbox(QPointF(baseLeft + noiseX, baseTop + noiseY),
                    QPointF(baseRight + noiseX, baseBottom + noiseY));

        QPointF topViewPos;
        double dist = 0.0;

        bool ok = projector.projectBBoxToGround(1, bbox, 1.789, &topViewPos, &dist, "person_seated_30s");
        assert(ok);

        // 1) 900프레임(30초) 동안 단 1프레임도 방 경계를 벗어나지 않는지 엄격 검증
        assert(topViewPos.x() >= -6.0 && topViewPos.x() <= 6.0);
        assert(topViewPos.y() >= -6.0 && topViewPos.y() <= 6.0);

        // 2) 유효한 실내 거리(0.5m ~ 6.0m) 내에 완벽하게 안착하는지 검증
        assert(dist >= 0.5 && dist <= 6.0);

        // 3) 시간적 EMA 스무딩으로 프레임 간 급격한 튐(Jitter)이 없는지 검증
        if (hasPrev) {
            const double frameDelta = std::hypot(topViewPos.x() - prevPos.x(), topViewPos.y() - prevPos.y());
            assert(frameDelta < 0.15); // 프레임당 15cm 이하의 매우 부드러운 이동
        }
        prevPos = topViewPos;
        hasPrev = true;
    }

    std::cout << "[PASS] test30SecondsContinuousNoBoundaryEscape: 900 frames (30s) tested, 0 boundary violations, perfectly stable!\n";
}

void testLoadCalibrationResultJson() {
    SpatialProjector &projector = SpatialProjector::instance();

    // OpenSDK / auto_calib 출력 형식의 JSON 데이터 테스트
    const QByteArray sampleResultJson = R"({
        "status": "PASS",
        "active_intrinsics": {
            "cx": 1337.029,
            "cy": 745.370,
            "fx": 2033.901,
            "fy": 2037.779,
            "resolution": [2592, 1520]
        },
        "visualization_t_camera_lidar": {
            "rotation_matrix": [
                [-0.89935, -0.02167, 0.43668],
                [0.03164, 0.99292, 0.11446],
                [-0.43607, 0.11676, -0.89230]
            ],
            "translation_m": [0.0515, 0.0786, 0.0353]
        }
    })";

    QString summary;
    bool ok = projector.loadCalibrationResultData(sampleResultJson, 1, &summary);
    assert(ok);
    assert(summary.contains("성공적"));

    CameraProfile cp = projector.profile(1);
    assert(std::abs(cp.t[0] - 0.0515) < 1e-3);
    assert(std::abs(cp.t[1] - 0.0786) < 1e-3);
    assert(std::abs(cp.t[2] - 0.0353) < 1e-3);

    std::cout << "[PASS] testLoadCalibrationResultJson: loaded result.json format successfully!\n";
}

void testLoadIntrinsicProfileJson() {
    SpatialProjector &projector = SpatialProjector::instance();

    // camera.json 형식 테스트
    const QByteArray sampleCameraJson = R"({
        "camera": {
            "model": "PNM-C16083RVQ",
            "resolution": [1920, 1080],
            "intrinsic": {"fx": 1500.5, "fy": 1502.5, "cx": 960.0, "cy": 540.0},
            "distortion": [-0.25, 0.12, -0.001, 0.0005, -0.05]
        }
    })";

    QString summary;
    bool ok = projector.loadIntrinsicProfileData(sampleCameraJson, 1, &summary);
    assert(ok);
    assert(summary.contains("적용 완료"));

    CameraProfile cp = projector.profile(1);
    assert(std::abs(cp.fx - 1500.5) < 1e-3);
    assert(std::abs(cp.fy - 1502.5) < 1e-3);
    assert(std::abs(cp.cx - 960.0) < 1e-3);
    assert(std::abs(cp.cy - 540.0) < 1e-3);
    assert(cp.imageWidth == 1920);
    assert(cp.imageHeight == 1080);
    assert(std::abs(cp.k1 - (-0.25)) < 1e-3);

    std::cout << "[PASS] testLoadIntrinsicProfileJson: loaded camera.json intrinsic format successfully!\n";
}

void testLoadManualExtrinsicJson() {
    SpatialProjector &projector = SpatialProjector::instance();

    // T_camera_lidar_*.json 형식 테스트
    const QByteArray sampleManualExtrinsicJson = R"({
        "status": "PASS",
        "extrinsic": {
            "rotation_matrix": [
                [-0.9835, -0.1426,  0.1111],
                [-0.0778,  0.8886,  0.4521],
                [-0.1632,  0.4360, -0.8850]
            ],
            "translation_m": [-0.0400, 0.0748, 0.1358]
        }
    })";

    QString summary;
    bool ok = projector.loadManualExtrinsicData(sampleManualExtrinsicJson, 1, &summary);
    assert(ok);
    assert(summary.contains("적용 완료"));
    assert(projector.calibrationMode() == CalibrationMode::Manual);

    CameraProfile cp = projector.profile(1);
    assert(std::abs(cp.t[0] - (-0.0400)) < 1e-3);
    assert(std::abs(cp.t[1] - 0.0748) < 1e-3);
    assert(std::abs(cp.t[2] - 0.1358) < 1e-3);
    assert(std::abs(cp.r[0] - (-0.9835)) < 1e-3);

    std::cout << "[PASS] testLoadManualExtrinsicJson: loaded T_camera_lidar manual RT format successfully!\n";
}

void testPerChannelLoading() {
    SpatialProjector &projector = SpatialProjector::instance();

    // CH2 에 별도의 내부 파라미터 로드
    const QByteArray ch2CameraJson = R"({
        "camera": {
            "resolution": [1280, 720],
            "intrinsic": {"fx": 1100.0, "fy": 1105.0, "cx": 640.0, "cy": 360.0},
            "distortion": [-0.1, 0.05, 0.0, 0.0, 0.0]
        }
    })";
    QString sum;
    bool ok = projector.loadIntrinsicProfileData(ch2CameraJson, 2, &sum);
    assert(ok);
    CameraProfile cp2 = projector.profile(2);
    assert(std::abs(cp2.fx - 1100.0) < 1e-3);
    assert(cp2.imageWidth == 1280);

    // CH3 에 별도의 Manual RT 로드
    const QByteArray ch3ManualJson = R"({
        "extrinsic": {
            "rotation_matrix": [
                [0.0, -1.0, 0.0],
                [1.0,  0.0, 0.0],
                [0.0,  0.0, 1.0]
            ],
            "translation_m": [1.5, 2.5, -0.5]
        }
    })";
    ok = projector.loadManualExtrinsicData(ch3ManualJson, 3, &sum);
    assert(ok);
    CameraProfile cp3 = projector.profile(3);
    assert(std::abs(cp3.t[0] - 1.5) < 1e-3);
    assert(std::abs(cp3.t[1] - 2.5) < 1e-3);
    assert(std::abs(cp3.t[2] - (-0.5)) < 1e-3);
    assert(std::abs(cp3.r[1] - (-1.0)) < 1e-3);

    // CH1 은 영향받지 않고 독립적으로 유지되는지 검증
    CameraProfile cp1 = projector.profile(1);
    assert(std::abs(cp1.fx - 1500.5) < 1e-3);

    std::cout << "[PASS] testPerChannelLoading: CH2 intrinsics and CH3 manual RT loaded independently!\n";
}

int main() {
    testSpatialProjectorCH1();
    testSpatialMetadataOnvifWithProjection();
    test30SecondsContinuousNoBoundaryEscape();
    testLoadCalibrationResultJson();
    testLoadIntrinsicProfileJson();
    testLoadManualExtrinsicJson();
    testPerChannelLoading();
    std::cout << "All spatial projection tests (including per-channel loading) PASS!\n";
    return 0;
}
