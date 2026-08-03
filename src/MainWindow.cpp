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
#include "Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QDateTime>

namespace {
QString sourceForTag(const QString &tag) {
    if (tag == "SCAN" || tag == "LSD" || tag == "MATCH" || tag == "CHECK" || tag == "EXPORT" || tag == "CALIB")
        return "RPi4B";
    if (tag == "MQTT")  return "BROKER";
    if (tag == "POWER") return "KIT";
    if (tag == "TILT")  return "IMU";
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
        connect(src, &DataBridge::calibUpdated, this, [this](const CalibState &c) {
            m_topBar->setCalib(c);
            m_topView->setCalib(c);
            m_calibTab->setCalib(c);
            m_devicesTab->setCalib(c);
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

    connect(m_banner, &TiltBanner::tiltOnset, this, [this](const ImuState &imu) {
        appendLog("TILT", QString(QString::fromUtf8("킷 수평 이탈 감지 — Roll %1° / Pitch %2°. 재설치 필요"))
                              .arg(imu.roll, 0, 'f', 1).arg(imu.pitch, 0, 'f', 1));
    });

    connect(m_topBar, &TopBar::powerToggled, this, [this](bool on) {
        (m_demoMode ? static_cast<DataBridge *>(m_demo) : static_cast<DataBridge *>(m_mqtt))->setKitPower(on);
        m_statusBar->setPower(on);
    });
    connect(m_topBar, &TopBar::rescanRequested, this, [this] {
        (m_demoMode ? static_cast<DataBridge *>(m_demo) : static_cast<DataBridge *>(m_mqtt))->requestRescan();
    });
    connect(m_topBar, &TopBar::calibrateRequested, this, [this, tabs] {
        (m_demoMode ? static_cast<DataBridge *>(m_demo) : static_cast<DataBridge *>(m_mqtt))->requestRescan();
        tabs->setCurrentIndex(1);
    });

    connect(m_calibTab, &CalibrationTab::exportRequested, this, [this](const QString &fmt) {
        appendLog("EXPORT", QString("calib_ch1_%1.%2 written")
                                 .arg(QDateTime::currentDateTime().toString("yyyyMMdd")).arg(fmt));
    });
    connect(m_datasetTab, &DatasetTab::exportRequested, this, [this] {
        appendLog("EXPORT", QString::fromUtf8("RGB-D 데이터셋 내보내기 완료"));
    });

    setDemoMode(true);
}

QWidget *MainWindow::buildDashboardTab() {
    auto *page = new QWidget;
    auto *l = new QHBoxLayout(page);
    l->setContentsMargins(10, 10, 10, 10);
    l->setSpacing(10);

    // 좌: 2x2 CCTV 그리드 (천장 중앙 4채널, 사분면 커버)
    const ChannelState defs[4] = {
        {1, QString::fromUtf8("북측 (0°) · 창측 벽면"),   "cctv/ch1/h264", false, 0, QString::fromUtf8("1920x1080 · WiseAI ON")},
        {2, QString::fromUtf8("동측 (90°) · 회의 구역"),  "cctv/ch2/h264", false, 0, QString::fromUtf8("1920x1080 · WiseAI ON")},
        {3, QString::fromUtf8("남측 (180°) · 출입문"),    "cctv/ch3/h264", false, 0, QString::fromUtf8("1920x1080 · WiseAI ON")},
        {4, QString::fromUtf8("서측 (270°) · 기둥/복도"), "cctv/ch4/h264", false, 0, QString::fromUtf8("NO STREAM")},
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
    } else {
        m_demo->stop();
        m_mqtt->connectToBroker("192.168.0.42", 1883);
    }
}

void MainWindow::appendLog(const QString &tag, const QString &msg) {
    m_calibTab->appendLog(tag, msg);
    m_eventsTab->appendEvent(tag, sourceForTag(tag), msg);
}
