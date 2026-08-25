#pragma once
#include <QFrame>
#include "Models.h"
#include "ScanCloud.h"
#include "ScanFetcher.h"

class TopViewWidget;
class ScanView3D;
class QStackedWidget;
class QPushButton;
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
    void setDaemonState(const DaemonState &s);
    void setScanProgress(const ScanProgress &p);
    void setScanResult(const ScanResult &r);

    // 실측 포인트클라우드 — 성공하면 지도에 깔고, 실패하면 사유를 범례에 남긴다.
    void setScanCloud(const ScanCloud &cloud);
    void setCloudStatus(const QString &msg, bool isError);

    void showMap2D();         // 2D 지도 화면으로 전환
    void showCloud3D();       // 3D 뷰어 화면으로 전환

    // 별도 창(전체화면)으로 빠져 있는지. 헤더 버튼 라벨을 상태에 맞춘다.
    void setDetached(bool detached);

signals:
    void openScanFileRequested();
    void showScanListDialogRequested();
    void openCalibResultRequested();
    // 헤더의 전체화면 버튼, 또는 지도/헤더/통계바 더블클릭. MainWindow 가 이
    // 패널을 별도 창으로 빼거나 대시보드로 되돌린다.
    void fullScreenToggleRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *ev) override;

private:
    TopViewWidget *m_map = nullptr;
    ScanView3D *m_view3d = nullptr;
    QStackedWidget *m_stack = nullptr;

    QLabel *m_coverage = nullptr;
    QLabel *m_cloudInfo = nullptr;
    QLabel *m_objectInfo = nullptr;
    QLabel *m_cloudSourceLabel = nullptr;

    QPushButton *m_btnScan = nullptr;
    QPushButton *m_btnCalib = nullptr;
    QPushButton *m_btn2d = nullptr;
    QPushButton *m_btn3d = nullptr;
    QPushButton *m_btnFull = nullptr;

    QLabel *m_scanSummary = nullptr;
    QLabel *m_roll = nullptr, *m_pitch = nullptr, *m_scanPts = nullptr, *m_expected = nullptr;
};

