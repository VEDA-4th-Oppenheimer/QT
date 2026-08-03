#pragma once
#include <QWidget>
#include "Models.h"

// 실내 3D 맵의 조감(Top-View) 단면.
// 원점 = 천장 중앙 킷 위치. 4채널 카메라는 사분면(0/90/180/270°)을 나눠 커버.
class TopViewWidget : public QWidget {
    Q_OBJECT
public:
    explicit TopViewWidget(QWidget *parent = nullptr);

    void setRoomSize(double widthM, double depthM);   // 기본 10 x 10 m
    void setEdges(const QVector<MapEdge> &edges);     // LiDAR 스캔 벽/기둥 에지
    void setObjects(const QVector<SpatialObject> &objects);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QPointF toPx(const QPointF &m) const;             // meter -> widget px
    double  pxPerM() const;

    double m_roomW = 10.0, m_roomD = 10.0;
    QVector<MapEdge>       m_edges;
    QVector<SpatialObject> m_objects;
};
