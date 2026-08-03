#pragma once
#include <QFrame>
#include "Models.h"

class TopViewWidget;
class QLabel;

// Dashboard 탭 우측: TOP-VIEW 헤더 + 캔버스 + 범례 바 + 하단 통계 바를 묶은 패널.
class TopViewPanel : public QFrame {
    Q_OBJECT
public:
    explicit TopViewPanel(QWidget *parent = nullptr);

    void setRoomSize(double w, double d);
    void setEdges(const QVector<MapEdge> &edges);
    void setObjects(const QVector<SpatialObject> &objects);
    void setImu(const ImuState &imu);
    void setCalib(const CalibState &c);

private:
    TopViewWidget *m_map;
    QLabel *m_coverage;
    QLabel *m_calibSummary;
    QLabel *m_roll, *m_pitch, *m_scanPts, *m_retry;
};
