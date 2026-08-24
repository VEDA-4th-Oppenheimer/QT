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
    void showMap2D();         // 객체/점군 2D replay 및 화면 전환용
    void showCloud3D();       // 객체/점군 3D replay 및 화면 전환용

    // 별도 창(전체화면)으로 빠져 있는지. 헤더 버튼 라벨을 상태에 맞춘다.
    void setDetached(bool detached);

signals:
    void refreshRequested();
    void scanChosen(const QString &name, const QString &localPath);
    // 헤더의 전체화면 버튼, 또는 지도/헤더/통계바 더블클릭. MainWindow 가 이
    // 패널을 별도 창으로 빼거나 대시보드로 되돌린다.
    void fullScreenToggleRequested();

protected:
    // 더블클릭을 자식 위젯들에서 주워 담는다. 3D 캔버스(ScanView3D)는 더블클릭이
    // 이미 시점 초기화라 일부러 뺐다 — 아래 installEventFilter 목록 참고.
    bool eventFilter(QObject *watched, QEvent *ev) override;

private:
    TopViewWidget *m_map;
    QLabel *m_coverage;
    QLabel *m_cloudInfo;
    QLabel *m_objectInfo;
    ScanView3D *m_view3d;
    QListWidget *m_list;
    QLabel *m_listNote;
    QLabel *m_viewTitle;
    QPushButton *m_btn2d, *m_btn3d, *m_btnFull;
    QStackedWidget *m_stack;
    QLabel *m_scanSummary;
    QLabel *m_roll, *m_pitch, *m_scanPts, *m_expected;
};
