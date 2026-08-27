#include "RtspSource.h"

#include <QCoreApplication>
#include <QDebug>

#include <iostream>

namespace {
void require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "rtsp_metadata_source_tests FAILED: " << message << std::endl;
        std::exit(1);
    }
}
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    RtspSource source;
    int updateCount = 0;
    QVector<SpatialObject> latest;
    QObject::connect(&source, &RtspSource::objectsUpdated,
                     [&updateCount, &latest](const QVector<SpatialObject> &objects) {
        ++updateCount;
        latest = objects;
    });

    source.ingestMetadataPayload(1, QByteArrayLiteral(
        R"({"coordinate_frame":"top_view_m","objects":[
            {"id":"ch1-p1","class":"person","x_m":1.0,"y_m":2.0}
        ]})"));
    require(updateCount == 1, "CH1 frame did not emit objectsUpdated");
    require(latest.size() == 1 && latest.first().channel == 1
                && latest.first().hasTopViewPosition(),
            "CH1 object was not retained with its channel and position");

    source.ingestMetadataPayload(2, QByteArrayLiteral(
        R"({"coordinate_frame":"lidar_xy_m","objects":[
            {"id":"ch2-p1","class":"pedestrian","x_m":-1.0,"y_m":0.5}
        ]})"));
    require(updateCount == 2 && latest.size() == 2,
            "objects from multiple channels were not merged");
    require(latest.at(0).channel == 1 && latest.at(1).channel == 2,
            "channel identity was lost during merge");

    source.ingestMetadataPayload(2, QByteArrayLiteral(
        R"({"coordinate_frame":"image","objects":[
            {"id":"ch2-p1","class":"person","bbox":{"left":0.1,"top":0.2,"right":0.3,"bottom":0.4}}
        ]})"));
    require(updateCount == 3 && latest.size() == 2,
            "image-only frame did not update the previous CH2 object");
    int topViewCount = 0;
    int unprojectedCount = 0;
    for (const SpatialObject &object : latest) {
        if (object.hasTopViewPosition()) ++topViewCount;
        else ++unprojectedCount;
    }
    require(topViewCount == 1 && unprojectedCount == 1,
            "image coordinates were plotted instead of being marked unprojected");

    source.ingestMetadataPayload(2, QByteArrayLiteral("{}"));
    require(updateCount == 4 && latest.size() == 1 && latest.first().channel == 1,
            "empty recognized frame did not clear the channel object list");

    const QByteArray jsonPartA = QByteArrayLiteral(
        R"({"coordinate_frame":"top_view_m","objects":[{"id":"split","class":"person","x_m":)"
    );
    const QByteArray jsonPartB = QByteArrayLiteral("0.25,\"y_m\":-0.75}]}");
    source.ingestMetadataPayload(3, jsonPartA);
    require(updateCount == 4, "partial metadata frame was emitted too early");
    source.ingestMetadataPayload(3, jsonPartB);
    require(updateCount == 5 && latest.size() == 2,
            "split metadata frame was not assembled and emitted");
    require(latest.last().channel == 3 && latest.last().id == QStringLiteral("split")
                && latest.last().hasTopViewPosition(),
            "assembled metadata object was not retained");

    const QByteArray xmlPartA = QByteArrayLiteral(
        R"(<tt:Frame xmlns:tt="urn:schemas-onvif-org:ver10/schema"><tt:Object ObjectId="ch4-split" coordinate_frame="top_view_m"><tt:Type>Human</tt:Type><tt:CenterOfGravity x=")"
    );
    const QByteArray xmlPartB = QByteArrayLiteral(R"(0.5" y="1.25"/></tt:Object></tt:Frame>)");
    source.ingestMetadataPayload(4, xmlPartA);
    require(updateCount == 5, "partial XML metadata frame was emitted too early");
    source.ingestMetadataPayload(4, xmlPartB);
    require(updateCount == 6 && latest.size() == 3,
            "split XML metadata frame was not assembled and emitted");
    require(latest.last().channel == 4 && latest.last().id == QStringLiteral("ch4-split")
                && latest.last().cls == QStringLiteral("PERSON")
                && latest.last().hasTopViewPosition(),
            "assembled CH4 XML object was not retained");

    // --- 4명 동시 감지 (RTP 분할 패킷 2개로 나뉘어 도착하는 상황 검증) ---
    // 먼저 CH1 을 빈 프레임으로 초기화
    source.ingestMetadataPayload(1, QByteArrayLiteral("{}"));

    // 패킷 1: 객체 1번, 2번
    const QByteArray pkt1 = QByteArrayLiteral(
        R"({"coordinate_frame":"top_view_m","objects":[
            {"id":"p1","class":"person","x_m":1.0,"y_m":2.0},
            {"id":"p2","class":"person","x_m":2.0,"y_m":3.0}
        ]})");
    // 패킷 2: 객체 3번, 4번
    const QByteArray pkt2 = QByteArrayLiteral(
        R"({"coordinate_frame":"top_view_m","objects":[
            {"id":"p3","class":"person","x_m":3.0,"y_m":4.0},
            {"id":"p4","class":"person","x_m":4.0,"y_m":5.0}
        ]})");

    source.ingestMetadataPayload(1, pkt1);
    source.ingestMetadataPayload(1, pkt2);

    int ch1Count = 0;
    for (const auto &obj : latest) {
        if (obj.channel == 1) ++ch1Count;
    }
    require(ch1Count == 4, "CH1 did not merge 4 persons across split packets!");

    // --- 500ms TTL 만료 플러시 검증 ---
    // 현재 시각보다 600ms 뒤 시각으로 강제 flush
    const qint64 futureMs = QDateTime::currentMSecsSinceEpoch() + 600;
    source.flushExpired(futureMs);

    ch1Count = 0;
    for (const auto &obj : latest) {
        if (obj.channel == 1) ++ch1Count;
    }
    require(ch1Count == 0, "Expired objects were not flushed after 500ms TTL!");

    qInfo() << "rtsp_metadata_source_tests: PASS (including 4-person merge and 500ms TTL flush)";
    return 0;
}
