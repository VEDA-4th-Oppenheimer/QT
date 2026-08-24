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

int main() {
    testSpatialProjectorCH1();
    testSpatialMetadataOnvifWithProjection();
    test30SecondsContinuousNoBoundaryEscape();
    std::cout << "All spatial projection tests (including 30s stability) PASS!\n";
    return 0;
}
