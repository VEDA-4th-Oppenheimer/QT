#include "MainWindow.h"
#include "TopBar.h"
#include "TiltBanner.h"
#include "StatusBar.h"
#include "CameraTile.h"
#include "TopViewPanel.h"
#include "CalibrationTab.h"
#include "DevicesTab.h"
#include "DatasetTab.h"
#include "EventLogTab.h"
#include "MqttBridge.h"
#include "DemoBridge.h"
#include "RtspSource.h"
#include "Theme.h"
#include "ConfigPath.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
QString sourceForTag(const QString &tag) {
    if (tag == "SCAN" || tag == "EXPORT")
        return "RPi4B";
    if (tag == "MQTT")   return "BROKER";
    if (tag == "POWER")  return "KIT";
    if (tag == "ERROR")  return "KIT";
    if (tag == "TILT" || tag == "LEVEL") return "IMU";
    if (tag == "RTSP")   return "CAMERA";
    return "KIT";
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QString::fromUtf8("SPATIAL·VMS — Indoor 3D Mapping Console"));
    resize(1600, 940);
    setMinimumSize(1440, 860);
    setStyleSheet(Theme::appStyleSheet());

    m_mqtt = new MqttBridge(this);
    m_demo = new DemoBridge(this);
    m_video = new RtspSource(this);
    m_topBar = new TopBar(this);
    m_banner = new TiltBanner(this);
    m_statusBar = new StatusBar(this);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildDashboardTab(), QString::fromUtf8("메인 대시보드"));
    m_calibTab = new CalibrationTab(this);
    tabs->addTab(m_calibTab, "CALIBRATION");
    m_devicesTab = new DevicesTab(this);
    tabs->addTab(m_devicesTab, "DEVICES / MQTT");
    m_datasetTab = new DatasetTab(this);
    tabs->addTab(m_datasetTab, "RGB-D DATASET");
    m_eventsTab = new EventLogTab(this);
    tabs->addTab(m_eventsTab, "EVENT LOG");

    auto *central = new QWidget(this);
    auto *l = new QVBoxLayout(central);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    l->addWidget(m_topBar);
    l->addWidget(m_banner);
    l->addWidget(tabs, 1);
    l->addWidget(m_statusBar);
    setCentralWidget(central);

    auto *modeMenu = menuBar()->addMenu(QString::fromUtf8("모드"));
    auto *demoAction = modeMenu->addAction(QString::fromUtf8("Demo Mode (브로커 없이 실행)"));
    demoAction->setCheckable(true);
    demoAction->setChecked(true);
    connect(demoAction, &QAction::toggled, this, &MainWindow::setDemoMode);

    // --- 시그널 배선 (데모/라이브 소스 모두 동일 슬롯으로 수신) -----------------
    for (DataBridge *src : {static_cast<DataBridge *>(m_demo), static_cast<DataBridge *>(m_mqtt)}) {
        connect(src, &DataBridge::brokerStateChanged, m_topBar, &TopBar::setBrokerConnected);
        connect(src, &DataBridge::imuUpdated, this, [this](const ImuState &imu) {
            m_topBar->setImu(imu);
            m_topView->setImu(imu);
            m_devicesTab->setImu(imu);
            m_banner->update(imu);
        });
        connect(src, &DataBridge::daemonStateUpdated, this, [this](const DaemonState &s) {
            m_topBar->setDaemonState(s);
            m_topView->setDaemonState(s);
            m_calibTab->setDaemonState(s);
            m_statusBar->setDaemonState(s);
        });
        connect(src, &DataBridge::scanProgressUpdated, this, [this](const ScanProgress &p) {
            m_topView->setScanProgress(p);
            m_calibTab->setScanProgress(p);
            m_devicesTab->setScanProgress(p);
        });
        connect(src, &DataBridge::scanResultUpdated, this, [this](const ScanResult &r) {
            m_topView->setScanResult(r);
            m_calibTab->setScanResult(r);
            appendLog("EXPORT", QString("state/scan — %1 (%2점, %3s)")
                                     .arg(r.pcdPath).arg(r.points).arg(r.durationS, 0, 'f', 1));
        });
        connect(src, &DataBridge::kitErrorReceived, this, [this](const KitError &e) {
            appendLog("ERROR", QString("[%1] %2 %3").arg(e.code).arg(e.name, e.msg));
        });
        connect(src, &DataBridge::objectsUpdated, m_topView, &TopViewPanel::setObjects);
        connect(src, &DataBridge::mapEdgesUpdated, m_topView, &TopViewPanel::setEdges);
        connect(src, &DataBridge::frameReceived, this, [this](int ch, const QImage &img) {
            if (ch >= 1 && ch <= 4) m_tiles[ch - 1]->setFrame(img);
        });
        connect(src, &DataBridge::channelStatusChanged, this, [this](int ch, bool online, double fps) {
            if (ch < 1 || ch > 4) return;
            m_tiles[ch - 1]->setOnline(online);
            m_tiles[ch - 1]->setFps(fps);
            m_devicesTab->setChannelOnline(ch, online);
        });
        connect(src, &DataBridge::logLine, this, &MainWindow::appendLog);
    }

    // RTSP 실제 카메라 영상 — config/cameras.json 에 설정된 채널만 덮어씌운다.
    // (설정 안 된 채널은 위 Demo/Live 브리지가 계속 상태를 채운다.)
    connect(m_video, &RtspSource::frameReceived, this, [this](int ch, const QImage &img) {
        if (ch >= 1 && ch <= 4) m_tiles[ch - 1]->setFrame(img);
    });
    connect(m_video, &RtspSource::channelStatusChanged, this, [this](int ch, bool online, double fps) {
        if (ch < 1 || ch > 4) return;
        m_tiles[ch - 1]->setOnline(online);
        m_tiles[ch - 1]->setFps(fps);
        m_devicesTab->setChannelOnline(ch, online);
    });
    connect(m_video, &RtspSource::logLine, this, &MainWindow::appendLog);

    connect(m_banner, &TiltBanner::tiltOnset, this, [this](const ImuState &imu) {
        appendLog("TILT", QString(QString::fromUtf8("킷 수평 이탈 감지 — Roll %1° / Pitch %2°. 재설치 필요"))
                              .arg(imu.roll, 0, 'f', 1).arg(imu.pitch, 0, 'f', 1));
    });

    // 계약 §3.1 UI 기본값 권장: pan [0,1790] / tilt [-900,900] / step 10.
    connect(m_topBar, &TopBar::scanRequested, this, [this, tabs] {
        (m_demoMode ? static_cast<DataBridge *>(m_demo) : static_cast<DataBridge *>(m_mqtt))
            ->requestScan(0, 1790, -900, 900, 10, 0);
        tabs->setCurrentIndex(1);
    });
    connect(m_topBar, &TopBar::stopRequested, this, [this] {
        (m_demoMode ? static_cast<DataBridge *>(m_demo) : static_cast<DataBridge *>(m_mqtt))->requestStop();
    });
    connect(m_topBar, &TopBar::homeRequested, this, [this] {
        (m_demoMode ? static_cast<DataBridge *>(m_demo) : static_cast<DataBridge *>(m_mqtt))->requestHome();
    });
    connect(m_topBar, &TopBar::disarmRequested, this, [this] {
        (m_demoMode ? static_cast<DataBridge *>(m_demo) : static_cast<DataBridge *>(m_mqtt))->requestDisarm();
    });
    connect(m_topBar, &TopBar::rearmRequested, this, [this] {
        (m_demoMode ? static_cast<DataBridge *>(m_demo) : static_cast<DataBridge *>(m_mqtt))->requestRearm();
    });

    connect(m_datasetTab, &DatasetTab::exportRequested, this, [this] {
        appendLog("EXPORT", QString::fromUtf8("RGB-D 데이터셋 내보내기 완료"));
    });

    setDemoMode(true);
    m_video->loadConfigAndStart();
}

QWidget *MainWindow::buildDashboardTab() {
    auto *page = new QWidget;
    auto *l = new QHBoxLayout(page);
    l->setContentsMargins(10, 10, 10, 10);
    l->setSpacing(10);

    // 좌: 2x2 CCTV 그리드. PNM-C16083RVQ 멀티센서 카메라 4채널, RTSP 직결(RtspSource) —
    // config/cameras.json 채널별 sensor 0~3 = /profile2/media.smp. MVP는 대표 1채널(CH1)
    // 기준이지만 하드웨어가 4채널을 모두 지원해 나머지도 함께 보여준다.
    const ChannelState defs[4] = {
        {1, QString::fromUtf8("북측 (0°) · 창측 벽면"),   "RTSP CH1 · sensor 0", false, 0, QString::fromUtf8("PNM-C16083RVQ · profile2")},
        {2, QString::fromUtf8("동측 (90°) · 회의 구역"),  "RTSP CH2 · sensor 1", false, 0, QString::fromUtf8("PNM-C16083RVQ · profile2")},
        {3, QString::fromUtf8("남측 (180°) · 출입문"),    "RTSP CH3 · sensor 2", false, 0, QString::fromUtf8("PNM-C16083RVQ · profile2")},
        {4, QString::fromUtf8("서측 (270°) · 기둥/복도"), "RTSP CH4 · sensor 3", false, 0, QString::fromUtf8("PNM-C16083RVQ · profile2")},
    };
    auto *grid = new QGridLayout;
    grid->setSpacing(10);
    for (int i = 0; i < 4; ++i) {
        m_tiles[i] = new CameraTile(defs[i], page);
        grid->addWidget(m_tiles[i], i / 2, i % 2);
    }
    l->addLayout(grid, 1);

    m_topView = new TopViewPanel(page);
    l->addWidget(m_topView);
    return page;
}

void MainWindow::setDemoMode(bool demo) {
    m_demoMode = demo;
    if (demo) {
        m_mqtt->stop();
        m_demo->start();
        return;
    }
    m_demo->stop();

    // config/mqtt.json (계약서 §1/§6): {"host","port","cert_dir"}. 없으면
    // 브로커가 아직 없는 로컬 개발 기준 localhost:1883 평문으로 시도한다.
    QString host = "localhost";
    quint16 port = 1883;
    QString certDir;
    QFile f(resolveConfigPath("config/mqtt.json"));
    if (f.open(QIODevice::ReadOnly)) {
        const auto o = QJsonDocument::fromJson(f.readAll()).object();
        host    = o.value("host").toString(host);
        port    = static_cast<quint16>(o.value("port").toInt(port));
        certDir = o.value("cert_dir").toString();
    }
    m_mqtt->connectToBroker(host, port, certDir);
}

void MainWindow::appendLog(const QString &tag, const QString &msg) {
    m_calibTab->appendLog(tag, msg);
    m_eventsTab->appendEvent(tag, sourceForTag(tag), msg);
}
