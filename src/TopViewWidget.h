#pragma once
#include <QWidget>
#include "Models.h"
#include "ScanCloud.h"

// 실내 3D 맵의 조감(Top-View) 단면.
// 원점 = 천장 중앙 킷 위치. 4채널 카메라는 사분면(0/90/180/270°)을 나눠 커버.
class TopViewWidget : public QWidget {
    Q_OBJECT
public:
    explicit TopViewWidget(QWidget *parent = nullptr);

    void setRoomSize(double widthM, double depthM);   // 기본 10 x 10 m
    void setEdges(const QVector<MapEdge> &edges);     // LiDAR 스캔 벽/기둥 에지
    void setObjects(const QVector<SpatialObject> &objects);

    // 실측 포인트클라우드. 방 크기보다 넓게 찍혔으면 뷰가 자동으로 넓어진다.
    void setScanCloud(const ScanCloud &cloud);
    void clearScanCloud();
    const ScanCloud &scanCloud() const { return m_cloud; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QPointF toPx(const QPointF &m) const;             // meter -> widget px
    QPointF toM(const QPointF &px) const;             // widget px -> meter (격자 범위 계산용)
    double  pxPerM() const;
    // 화면에 반드시 담아야 하는 영역 [m]. 방 + (있으면) 점군 바운딩 박스.
    // 스캔은 방보다 넓고 원점 기준으로 비대칭인 게 보통이라 정사각형으로 잡으면
    // 한쪽이 통째로 빈 채 배율만 깎인다.
    QRectF  viewRectM() const;
    void    paintCloud(QPainter &p) const;

    double m_roomW = 10.0, m_roomD = 10.0;
    QVector<MapEdge>       m_edges;
    QVector<SpatialObject> m_objects;
    ScanCloud              m_cloud;
    // 화면좌표 변환 결과를 담는 재사용 버퍼 — 3만 점을 repaint 마다 새로 할당하지
    // 않으려고 들고 있는다(paintEvent 는 const 라 mutable).
    mutable QVector<QPointF> m_pxScratch;
};
