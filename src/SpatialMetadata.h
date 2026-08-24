#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

#include "Models.h"

namespace spatial_metadata {

struct ParseResult {
    QVector<SpatialObject> objects;
    bool recognized = false;  // 유효한 JSON/XML metadata frame 이었는지
    int ignored = 0;          // 좌표가 없거나 Top-View 좌표가 아닌 객체 수
    QString warning;
};

// RTSP data/subtitle packet에 실린 JSON 또는 ONVIF XML metadata를 파싱한다.
// 기본 좌표 계약은 coordinate_frame="top_view_m" (x/y meter)이다.
ParseResult parse(const QByteArray &payload, int channel);

} // namespace spatial_metadata
