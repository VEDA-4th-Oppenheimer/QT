#pragma once
#include <QFrame>
#include "Models.h"
#include "ScanCloud.h"
#include "ScanFetcher.h"

class TopViewWidget;
class ScanView3D;
class QStackedWidget;
class QListWidget;
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
    // 3D 칸의 파일 목록. MainWindow 가 ScanFetcher 결과를 넘겨준다.
    void setScanList(const QVector<ScanEntry> &entries, const QString &note);
    void showScanList();      // 목록 화면으로

signals:
    void refreshRequested();
    void scanChosen(const QString &name, const QString &localPath);

private:
    TopViewWidget *m_map;
    QLabel *m_coverage;
    QLabel *m_cloudInfo;
    ScanView3D *m_view3d;
    QListWidget *m_list;
    QLabel *m_listNote;
    QLabel *m_viewTitle;
    QPushButton *m_btn2d, *m_btn3d;
    QStackedWidget *m_stack;
    QLabel *m_scanSummary;
    QLabel *m_roll, *m_pitch, *m_scanPts, *m_expected;
};
