#include "MainWindow.h"
#include "TopBar.h"
#include "TiltBanner.h"
#include "StatusBar.h"
#include "CameraTile.h"
#include "TopViewPanel.h"
#include "CalibrationTab.h"
#include "DevicesTab.h"
#include "EventLogTab.h"
#include "SettingsTab.h"
#include "MqttBridge.h"
#include "DemoBridge.h"
#include "RtspSource.h"
#include "EnrollDialog.h"
#include "ScanFetcher.h"
#include "ScanListDialog.h"
#include "CameraCalibDialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include "CameraConfig.h"
#include "Theme.h"
#include "ConfigPath.h"
#include "SpatialProjector.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QShortcut>
#include <QTabWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QSettings>
#include <QLabel>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QAuthenticator>

namespace {

/* 카메라 OpenSDK(캘리브레이션 API) 접속 스킴.
 *
 * ⚠️ 브링업 편의로 http 를 쓰는 중이다. 되돌릴 때는 이 한 줄만 "https" 로
 *   바꾸면 되고, URL 조립과 스킴 검증이 전부 이 상수를 본다.
 *
 * ⚠️ http 로 두면 카메라 계정/비밀번호가 Basic 인증 헤더에 실려 **평문으로**
 *   나간다(base64 는 인코딩이지 암호화가 아니다). 같은 네트워크에 있는
 *   누구든 그대로 읽을 수 있다. 데모·제출본으로 나가기 전에 https 로
 *   되돌릴 것. */
constexpr const char *kCalibScheme = "http";

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
    // 하한을 화면보다 크게 잡으면 안 된다. 윈도우에서 배율 125~150% 를 쓰면
    // 860 논리픽셀이 물리적으로 1075~1290 이 되어 1080p 화면 세로에 안 들어가고,
    // 그러면 Qt 가 레이아웃을 잘라내면서 **맨 아래 배치된 위젯부터** 사라진다
    // (TOP-VIEW 의 ROLL/PITCH/SCAN PTS 통계바가 여기 해당했다). 작업표시줄과
    // 윈도우 쪽 큰 기본 폰트가 몇십 픽셀을 더 먹어 경계에서 밀린다.
    // 좁으면 지도가 먼저 줄고 수치는 남도록, 하한은 실제로 들어가는 크기로 낮춘다.
    setMinimumSize(1280, 720);

    // 설치 높이는 기본 1.789m(1789mm)이며, SETTINGS 탭에서 언제든 변경 가능
    m_sensorHeightMm = QSettings().value(QStringLiteral("scan/sensor_height_mm"), 1789).toInt();
    if (m_sensorHeightMm <= 0) m_sensorHeightMm = 1789;
    SpatialProjector::instance().setGlobalGroundY(m_sensorHeightMm / 1000.0);

    const bool isManual = QSettings().value(QStringLiteral("calib/is_manual"), false).toBool();
    SpatialProjector::instance().setCalibrationMode(isManual ? CalibrationMode::Manual : CalibrationMode::Automatic);

    m_mqtt = new MqttBridge(this);
    m_demo = new DemoBridge(this);
    m_video = new RtspSource(this);
    m_scanFetcher = new ScanFetcher(this);

    // ScanFetcher 는 MainWindow 소유라 rebuildUi 로 위젯이 갈려도 살아남는다.
    // 여기서 한 번만 연결하고, 람다는 호출 시점의 m_topView 를 본다.
    connect(m_scanFetcher, &ScanFetcher::cloudReady, this, [this](const ScanCloud &c) {
        m_lastCloud = c; m_haveCloud = true;
        m_topView->setScanCloud(c);
        if (c.xMax > c.xMin && c.zMax > c.zMin) {
            SpatialProjector::instance().setRoomBounds(c.xMin, c.xMax, c.zMin, c.zMax);
        }
        appendLog("EXPORT", QString::fromUtf8("포인트클라우드 %1점 표시 (미반사 %2, 반경 %3 m)")
                                .arg(c.count()).arg(c.invalid).arg(c.radiusM(), 0, 'f', 1));
    });
    connect(m_scanFetcher, &ScanFetcher::failed, this, [this](const QString &why) {
        m_topView->setCloudStatus(QString::fromUtf8("CLOUD 실패"), true);
        appendLog("EXPORT", QString::fromUtf8("포인트클라우드 가져오기 실패 — %1").arg(why));
    });
    connect(m_scanFetcher, &ScanFetcher::progress, this, [this](const QString &m) {
        appendLog("EXPORT", m);
    });
    connect(m_scanFetcher, &ScanFetcher::listReady, this,
            [this](const QVector<ScanEntry> &e, const QString &note) {
        if (m_scanListDialog != nullptr) m_scanListDialog->setEntries(e, note);
    });

    // 모드/테마 메뉴에 있던 항목은 SETTINGS 탭으로 옮겼다(SettingsTab 참고).
    // 메뉴와 탭에 같은 토글을 두 벌 두면 체크 상태가 어긋나므로 한쪽만 남긴다.
    // 다만 전체화면은 단축키가 붙어 있어야 쓸모가 있어서 메뉴에도 남긴다 —
    // 이건 토글 상태를 갖지 않는 동작이라 어긋날 여지가 없다.
    auto *viewMenu = menuBar()->addMenu(QString::fromUtf8("보기"));
    auto *fullTopViewAction = viewMenu->addAction(QString::fromUtf8("TOP-VIEW 전체화면 켜기/끄기"));
    fullTopViewAction->setShortcut(QKeySequence::FullScreen);
    connect(fullTopViewAction, &QAction::triggered, this, &MainWindow::toggleTopViewFullScreen);

    rebuildUi();

    // 설정이 없으면 최초 설정을 받는다. 사용자가 취소하면 브로커 접속을 시도하는
    // 대신 Demo 모드로 띄운다 — 설정 없이 접속을 걸면 5초마다 재시도만 반복하고
    // 화면은 비어 있어서, 빈 실화면보다 데모가 낫다.
    // setDemoMode 가 SETTINGS 탭의 체크 상태까지 맞춰주므로 그대로 호출한다.
    setDemoMode(!ensureConfigured());
    m_video->loadConfigAndStart();
}

void MainWindow::setThemeMode(Theme::Mode mode) {
    if (Theme::CurrentMode == mode) return;
    Theme::setMode(mode);
    rebuildUi();
}

void MainWindow::rebuildUi() {
    setStyleSheet(Theme::appStyleSheet());

    // 아래에서 중앙 위젯을 통째로 지운다. TOP-VIEW 가 별도 창에 나가 있으면 그
    // 패널만 중앙 위젯 밖에 살아남아 유령 창이 되므로, 먼저 제자리로 돌려놓는다.
    attachTopView();

    // m_mqtt/m_demo/m_video 는 MainWindow 소유라 테마 전환으로 중앙 위젯이
    // 통째로 바뀌어도 살아남는다 — 그래서 아래에서 이 객체들을 this(MainWindow)
    // 로 다시 connect() 하기 전에 이전 연결을 끊어야 한다. 안 그러면 재생성될
    // 때마다 연결이 쌓여서 이벤트 하나가 N번 중복 처리된다(EVENT LOG 중복 등).
    m_demo->disconnect(this);
    m_mqtt->disconnect(this);
    m_video->disconnect(this);

    QWidget *old = takeCentralWidget();
    delete old;

    m_topBar = new TopBar(this);
    m_banner = new TiltBanner(this);
    m_statusBar = new StatusBar(this);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildDashboardTab(), QString::fromUtf8("메인 대시보드"));
    m_calibTab = new CalibrationTab(this);
    tabs->addTab(m_calibTab, "CALIBRATION");
    m_devicesTab = new DevicesTab(this);
    tabs->addTab(m_devicesTab, "DEVICES / MQTT");
    m_eventsTab = new EventLogTab(this);
    tabs->addTab(m_eventsTab, "EVENT LOG");

    // SETTINGS 는 마지막에 둔다 — 자주 쓰는 탭을 앞에.
    SettingsTab::State st;
    st.theme           = Theme::CurrentMode;
    st.demoMode        = m_demoMode;
    st.sensorHeightMm  = m_sensorHeightMm;
    st.topViewDetached = (m_topViewWindow != nullptr);
    st.manualCalib     = QSettings().value(QStringLiteral("calib/is_manual"), false).toBool();
    cameraSummary(&st.cameraHost, &st.cameraChannels);
    m_settingsTab = new SettingsTab(st, this);
    tabs->addTab(m_settingsTab, "SETTINGS");

    connect(m_settingsTab, &SettingsTab::themeChangeRequested, this, &MainWindow::setThemeMode);
    connect(m_settingsTab, &SettingsTab::demoModeToggled, this, &MainWindow::setDemoMode);
    connect(m_settingsTab, &SettingsTab::calibModeToggled, this, [this](bool manual) {
        SpatialProjector::instance().setCalibrationMode(manual ? CalibrationMode::Manual : CalibrationMode::Automatic);
        QSettings().setValue(QStringLiteral("calib/is_manual"), manual);
        appendLog("CALIB", QString::fromUtf8("캘리브레이션 RT 모드 전환: %1").arg(SpatialProjector::instance().calibrationModeName()));
    });
    connect(m_settingsTab, &SettingsTab::loadManualRtRequested, this, &MainWindow::openManualRtFile);
    connect(m_settingsTab, &SettingsTab::cameraSettingsRequested, this, &MainWindow::editCameraSettings);
    connect(m_settingsTab, &SettingsTab::cameraReconnectRequested, this, [this] {
        appendLog("RTSP", QString::fromUtf8("사용자 요청 — 카메라에 다시 연결합니다"));
        m_video->reconnectAll();
    });
    connect(m_settingsTab, &SettingsTab::sensorHeightRequested, this, &MainWindow::editSensorHeight);
    connect(m_settingsTab, &SettingsTab::loadIntrinsicProfileRequested, this, &MainWindow::openIntrinsicProfileFile);
    connect(m_settingsTab, &SettingsTab::logoutRequested, this, &MainWindow::logout);

    // 테마를 바꾸면 여기까지 다시 오므로, 보고 있던 탭으로 되돌려 놓는다.
    // (안 하면 SETTINGS 에서 테마를 누른 순간 메인 대시보드로 튄다)
    if (m_activeTab >= 0 && m_activeTab < tabs->count())
        tabs->setCurrentIndex(m_activeTab);
    connect(tabs, &QTabWidget::currentChanged, this, [this](int i) { m_activeTab = i; });

    auto *central = new QWidget(this);
    auto *l = new QVBoxLayout(central);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    l->addWidget(m_topBar);
    l->addWidget(m_banner);
    l->addWidget(tabs, 1);
    l->addWidget(m_statusBar);
    setCentralWidget(central);

    // --- 시그널 배선 (데모/라이브 소스 모두 동일 슬롯으로 수신) -----------------
    // 위젯을 새로 만들 때마다 다시 연결한다 — 이전 위젯이 delete 되면서 이전
    // connect() 는 Qt 가 알아서 끊어준다(브리지 쪽 m_mqtt/m_demo/m_video 는
    // MainWindow 소유라 재생성 사이에도 그대로 살아있는다).
    for (DataBridge *src : {static_cast<DataBridge *>(m_demo), static_cast<DataBridge *>(m_mqtt)}) {
        connect(src, &DataBridge::brokerStateChanged, this, [this](bool up) {
            m_lastBrokerUp = up; m_haveBrokerState = true;
            m_topBar->setBrokerConnected(up);
        });
        connect(src, &DataBridge::imuUpdated, this, [this](const ImuState &imu) {
            m_lastImu = imu; m_haveImu = true;
            m_topBar->setImu(imu);
            m_topView->setImu(imu);
            m_devicesTab->setImu(imu);
            m_banner->update(imu);
        });
        connect(src, &DataBridge::daemonStateUpdated, this, [this](const DaemonState &s) {
            m_lastDaemonState = s; m_haveDaemonState = true;
            m_topBar->setDaemonState(s);
            m_topView->setDaemonState(s);
            m_calibTab->setDaemonState(s);
            m_statusBar->setDaemonState(s);
        });
        connect(src, &DataBridge::scanProgressUpdated, this, [this](const ScanProgress &p) {
            m_lastProgress = p; m_haveProgress = true;
            m_topView->setScanProgress(p);
            m_calibTab->setScanProgress(p);
            m_devicesTab->setScanProgress(p);
        });
        connect(src, &DataBridge::scanResultUpdated, this, [this](const ScanResult &r) {
            m_lastResult = r; m_haveResult = true;
            m_topView->setScanResult(r);
            m_calibTab->setScanResult(r);
            // durationS 는 실구현(develop 브랜치)이 아직 안 보낸다 — 있을 때만 붙인다.
            QString msg = QString("state/scan — %1 (%2점)").arg(r.pcdPath).arg(r.points);
            if (r.durationS > 0.0) msg += QString(", %1s").arg(r.durationS, 0, 'f', 1);
            appendLog("EXPORT", msg);
            // 스캔이 끝나면 .pcd 를 받아 Top-View 에 자동으로 깐다. 계약 §9 가
            // 미결이라 경로만 오므로, 로컬에 있으면 그걸 쓰고 없으면 발급
            // 서비스(8443)의 GET /scan 으로 받는다 — ScanFetcher 참고.
            if (r.ok && !r.pcdPath.isEmpty()) m_scanFetcher->fetch(r.pcdPath);
        });
        connect(src, &DataBridge::kitErrorReceived, this, [this](const KitError &e) {
            appendLog("ERROR", QString("[%1] %2 %3").arg(e.code).arg(e.name, e.msg));
        });
        connect(src, &DataBridge::objectsUpdated, this, [this, src](const QVector<SpatialObject> &objects) {
            // 실제 RTSP metadata가 들어오기 시작하면 데모의 주기적 가상 PERSON이
            // 실측 객체를 덮어쓰지 않게 한다.
            if (src == static_cast<DataBridge *>(m_demo) && m_rtspMetadataActive) return;
            m_lastObjects = objects;
            m_haveObjects = true;
            m_topView->setObjects(objects);
            for (auto *t : m_tiles) t->setDetectedObjects(objects);
        });
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
    connect(m_video, &RtspSource::objectsUpdated, this,
            [this](const QVector<SpatialObject> &objects) {
        m_rtspMetadataActive = true;
        m_lastObjects = objects;
        m_haveObjects = true;
        m_topView->setObjects(objects);
        for (auto *t : m_tiles) t->setDetectedObjects(objects);
    });

    connect(m_banner, &TiltBanner::tiltOnset, this, [this](const ImuState &imu) {
        appendLog("TILT", QString(QString::fromUtf8("킷 수평 이탈 감지 — Roll %1° / Pitch %2°. 재설치 필요"))
                              .arg(imu.roll, 0, 'f', 1).arg(imu.pitch, 0, 'f', 1));
    });

    // pan [0,1791] / tilt [-900,900] / step 9 (0.9도).
    //
    // 팬 끝각은 step 에 물려 있다. 틸트가 바닥을 지나면 한 줄이 방위 p 와 p+180 을
    // 함께 훑으므로 팬은 반 바퀴만 돌면 되는데, 1800 까지 돌면 첫 줄과 마지막 줄이
    // 같은 평면이라 중복된다. 그래서 "1800 - step" 까지만 간다(scan_warn_seam).
    // step 을 바꾸면 이 값도 같이 바꿀 것 — 10 → 1790, 9 → 1791.
    // 마지막 인자는 지면→라이다 회전축 높이(mm). 좌표 계산에는 들어가지 않고
    // .pcd 헤더에 sensor_height_m 주석으로만 실린다 — 소비자가 바닥평면을 잡거나
    // 다른 좌표계로 옮길 때 쓴다. 0 은 "모름". 모드 → 센서 높이 설정… 에서 바꾼다.
    connect(m_topBar, &TopBar::scanRequested, this, [this, tabs] {
        (m_demoMode ? static_cast<DataBridge *>(m_demo) : static_cast<DataBridge *>(m_mqtt))
            ->requestScan(0, 1791, -900, 900, 9, m_sensorHeightMm);
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

    // 테마 전환으로 위젯을 새로 만든 경우, 다음 업데이트가 오기 전까지
    // OFFLINE/기본값으로 잠깐 보이지 않도록 마지막으로 받은 값을 즉시 채운다.
    if (m_haveBrokerState) m_topBar->setBrokerConnected(m_lastBrokerUp);
    if (m_haveDaemonState) {
        m_topBar->setDaemonState(m_lastDaemonState);
        m_topView->setDaemonState(m_lastDaemonState);
        m_calibTab->setDaemonState(m_lastDaemonState);
        m_statusBar->setDaemonState(m_lastDaemonState);
    }
    if (m_haveImu) {
        m_topBar->setImu(m_lastImu);
        m_topView->setImu(m_lastImu);
        m_devicesTab->setImu(m_lastImu);
    }
    if (m_haveProgress) {
        m_topView->setScanProgress(m_lastProgress);
        m_calibTab->setScanProgress(m_lastProgress);
        m_devicesTab->setScanProgress(m_lastProgress);
    }
    if (m_haveResult) {
        m_topView->setScanResult(m_lastResult);
        m_calibTab->setScanResult(m_lastResult);
    }
    if (m_haveCloud) m_topView->setScanCloud(m_lastCloud);
    if (m_haveObjects) m_topView->setObjects(m_lastObjects);

    // TOP-VIEW 패널의 데이터 열기 및 화면 전환 시그널 연결
    connect(m_topView, &TopViewPanel::openScanFileRequested, this, &MainWindow::openScanFile);
    connect(m_topView, &TopViewPanel::showScanListDialogRequested, this, &MainWindow::showScanListDialog);
    connect(m_topView, &TopViewPanel::openCalibResultRequested, this, &MainWindow::openCalibrationResultFile);
    connect(m_topView, &TopViewPanel::fetchCalibResultFromCameraRequested, this, &MainWindow::showCameraCalibDialog);
    // 큐드로 받는다. 이 시그널은 지도 위젯이 자기 더블클릭을 처리하는 도중에
    // 나오는데, 슬롯은 그 위젯이 든 패널을 다른 창으로 reparent 한다. 패널 안에
    // ScanView3D(QOpenGLWidget) 가 있어 reparent 마다 GL 컨텍스트가 파기·재생성
    // 되므로, 이벤트 전달이 끝난 뒤로 미뤄야 안전하다.
    connect(m_topView, &TopViewPanel::fullScreenToggleRequested,
            this, &MainWindow::toggleTopViewFullScreen, Qt::QueuedConnection);
}

QWidget *MainWindow::buildDashboardTab() {
    auto *page = new QWidget;
    auto *l = new QHBoxLayout(page);
    l->setContentsMargins(10, 10, 10, 10);
    l->setSpacing(0);   // 좌우 간격은 스플리터 핸들이 만든다

    // 좌: 2x2 CCTV 그리드. PNM-C16083RVQ 멀티센서 카메라 4채널, RTSP 직결(RtspSource) —
    // config/cameras.json 채널별 sensor 0~3 = /profile2/media.smp. MVP는 대표 1채널(CH1)
    // 기준이지만 하드웨어가 4채널을 모두 지원해 나머지도 함께 보여준다.
    // 타일 헤더에는 채널 번호만 쓴다. 예전에는 "북측 (0°) · 창측 벽면" 같은 설치
    // 위치 설명이 붙어 있었는데, 이건 시안용으로 지어낸 문구라 실제 카메라가 어디를
    // 보는지와 무관하다 — 화면에 있으면 사실로 읽히므로 뺀다(IMU 모델명을 뺀 것과
    // 같은 이유). 방위가 필요해지면 설치할 때 입력받아 채울 자리다.
    const ChannelState defs[4] = {
        {1, QString(), "RTSP CH1 · sensor 0", false, 0, QString::fromUtf8("PNM-C16083RVQ · profile2")},
        {2, QString(), "RTSP CH2 · sensor 1", false, 0, QString::fromUtf8("PNM-C16083RVQ · profile2")},
        {3, QString(), "RTSP CH3 · sensor 2", false, 0, QString::fromUtf8("PNM-C16083RVQ · profile2")},
        {4, QString(), "RTSP CH4 · sensor 3", false, 0, QString::fromUtf8("PNM-C16083RVQ · profile2")},
    };
    auto *camPane = new QWidget(page);
    auto *grid = new QGridLayout(camPane);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(10);
    for (int i = 0; i < 4; ++i) {
        m_tiles[i] = new CameraTile(defs[i], camPane);
        grid->addWidget(m_tiles[i], i / 2, i % 2);
    }
    // 2x2 그리드가 이보다 좁아지면 타일 안 라벨이 겹친다. 스플리터가 이 값
    // 아래로는 못 끌게 막아준다(TOP-VIEW 쪽은 자기 minimumWidth 로 버틴다).
    camPane->setMinimumWidth(520);

    // 시그널 연결은 rebuildUi() 가 한곳에서 한다 — 여기서 또 connect 하면
    // 같은 슬롯이 두 번 불려서 더블클릭 한 번에 detach 후 즉시 attach 가 된다.
    m_topView = new TopViewPanel(page);

    // 좌(CCTV)/우(TOP-VIEW) 비율을 사용자가 정한다. 현장마다 무엇을 크게 볼지가
    // 달라서(영상 감시 위주 / 스캔 확인 위주) 고정 430px 로는 늘 누군가 손해였다.
    m_dashSplitter = new QSplitter(Qt::Horizontal, page);
    m_dashSplitter->setChildrenCollapsible(false);   // 한쪽을 0 으로 접어버리면 되돌리기 어렵다
    m_dashSplitter->setHandleWidth(10);
    m_dashSplitter->setStyleSheet(QString(
        "QSplitter::handle:horizontal { background:transparent;"
        " border-left:4px solid transparent; border-right:4px solid transparent; }"
        "QSplitter::handle:horizontal { image:none; }"
        "QSplitter::handle:horizontal:hover { background:%1; }"
        "QSplitter::handle:horizontal:pressed { background:%2; }")
        .arg(Theme::BorderSoft.name(), Theme::Accent.name()));
    m_dashSplitter->addWidget(camPane);
    m_dashSplitter->addWidget(m_topView);
    // 창을 키우면 늘어나는 쪽은 CCTV 다 — TOP-VIEW 는 수치 패널이라 넓어져도
    // 얻는 게 적다. 사용자가 끈 비율은 아래 restoreState 가 그대로 되살린다.
    m_dashSplitter->setStretchFactor(0, 1);
    m_dashSplitter->setStretchFactor(1, 0);
    l->addWidget(m_dashSplitter);

    if (m_splitterState.isEmpty()) {
        m_splitterState = QSettings().value(QStringLiteral("dashboard/splitter")).toByteArray();
    }
    if (!m_dashSplitter->restoreState(m_splitterState)) {
        m_dashSplitter->setSizes({760, 430});   // 종전 고정 폭과 같은 초기 비율
    }
    connect(m_dashSplitter, &QSplitter::splitterMoved, this, [this](int, int) {
        m_splitterState = m_dashSplitter->saveState();
        QSettings().setValue(QStringLiteral("dashboard/splitter"), m_splitterState);
    });
    return page;
}

void MainWindow::toggleTopViewFullScreen() {
    if (m_topViewWindow != nullptr) attachTopView();
    else                            detachTopView();
    if (m_settingsTab != nullptr)
        m_settingsTab->setTopViewDetached(m_topViewWindow != nullptr);
}

void MainWindow::detachTopView() {
    if (m_topViewWindow != nullptr || m_topView == nullptr || m_dashSplitter == nullptr) return;

    m_splitterState = m_dashSplitter->saveState();
    const int idx = m_dashSplitter->indexOf(m_topView);

    m_topViewSlot = new QLabel(QString::fromUtf8(
        "TOP-VIEW 를 별도 창에서 보는 중입니다.\n\n그 창에서 Esc 를 누르거나\n"
        "지도를 더블클릭하면 이 자리로 돌아옵니다."), m_dashSplitter);
    m_topViewSlot->setAlignment(Qt::AlignCenter);
    m_topViewSlot->setWordWrap(true);
    m_topViewSlot->setStyleSheet(Theme::mono(11) + QString("color:%1;background:%2;"
        "border:1px dashed %3;border-radius:5px;")
        .arg(Theme::TextFaint.name(), Theme::Panel.name(), Theme::Border.name()));
    m_dashSplitter->insertWidget(idx, m_topViewSlot);

    auto *win = new QWidget(this, Qt::Window);
    win->setWindowTitle(QString::fromUtf8("SPATIAL·VMS — TOP-VIEW"));
    auto *wl = new QVBoxLayout(win);
    wl->setContentsMargins(0, 0, 0, 0);
    wl->addWidget(m_topView);          // 여기서 패널이 창으로 옮겨간다(reparent)
    m_topView->show();
    m_topView->setDetached(true);

    // Esc 로 되돌린다 — 전체화면에는 타이틀바가 없어서 닫을 방법이 헤더 버튼밖에
    // 없기 때문이다(macOS 전체화면에서는 창 닫기 버튼도 안 보인다).
    auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), win);
    connect(esc, &QShortcut::activated, this, &MainWindow::attachTopView);
    win->installEventFilter(this);

    m_topViewWindow = win;
    win->showFullScreen();
    // showFullScreen 만으로는 본창 뒤에 깔린 채로 떴다(macOS). 명시적으로 올린다.
    win->raise();
    win->activateWindow();
}

void MainWindow::attachTopView() {
    if (m_topViewWindow == nullptr) return;

    QWidget *win = m_topViewWindow;
    m_topViewWindow = nullptr;           // 재진입(창 close 이벤트 -> 여기) 차단
    win->removeEventFilter(this);

    if (m_topView != nullptr && m_dashSplitter != nullptr && m_topViewSlot != nullptr) {
        const int idx = m_dashSplitter->indexOf(m_topViewSlot);
        m_dashSplitter->insertWidget(idx, m_topView);
        m_topView->show();
        m_topView->setDetached(false);
        delete m_topViewSlot;
        m_topViewSlot = nullptr;
        m_dashSplitter->restoreState(m_splitterState);
    }
    // 자기 close 이벤트 처리 중에 불릴 수 있어 즉시 delete 하지 않는다.
    win->deleteLater();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *ev) {
    if (watched == m_topViewWindow && ev->type() == QEvent::Close) {
        attachTopView();
        return true;
    }
    return QMainWindow::eventFilter(watched, ev);
}

bool MainWindow::ensureConfigured() {
    if (configReady()) return true;

    // 배포본 최초 실행. 인증서(MQTT)와 카메라 설정(RTSP)을 발급 서버에서 한 번에
    // 받아오므로, 사용자는 토큰만 입력하면 둘 다 붙는다.
    EnrollDialog dlg(this);
    return (dlg.exec() == QDialog::Accepted) && configReady();
}

void MainWindow::cameraSummary(QString *host, int *channels) const {
    if (host != nullptr)     *host = QString();
    if (channels != nullptr) *channels = 0;
    QFile f(resolveConfigPath(QStringLiteral("config/cameras.json")));
    if (!f.open(QIODevice::ReadOnly)) return;
    const auto ch = QJsonDocument::fromJson(f.readAll()).object()
                        .value(QStringLiteral("channels")).toObject();
    if (ch.isEmpty()) return;
    if (channels != nullptr) *channels = ch.size();
    // 채널마다 같은 카메라의 센서 0~3 이라 호스트는 하나다.
    if (host != nullptr) *host = QUrl(ch.begin().value().toString()).host();
}

void MainWindow::editCameraSettings() {
    // 현재 값에서 IP·계정을 뽑아 기본값으로 채운다(비밀번호는 되읽지 않는다).
    QString curHost, curUser;
    QFile f(resolveConfigPath(QStringLiteral("config/cameras.json")));
    if (f.open(QIODevice::ReadOnly)) {
        const auto ch = QJsonDocument::fromJson(f.readAll()).object()
                            .value(QStringLiteral("channels")).toObject();
        if (!ch.isEmpty()) {
            const QUrl u(ch.begin().value().toString());
            curHost = u.host();
            curUser = u.userName();
        }
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("카메라 설정"));
    auto *host = new QLineEdit(curHost, &dlg);
    host->setPlaceholderText(QString::fromUtf8("예: 172.20.33.8"));
    auto *user = new QLineEdit(curUser, &dlg);
    auto *pass = new QLineEdit(&dlg);
    pass->setEchoMode(QLineEdit::Password);
    pass->setPlaceholderText(QString::fromUtf8("비워두면 기존 비밀번호 유지"));

    auto *form = new QFormLayout;
    form->addRow(QString::fromUtf8("카메라 IP"),      host);
    form->addRow(QString::fromUtf8("계정"),           user);
    form->addRow(QString::fromUtf8("비밀번호"),        pass);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto *lay = new QVBoxLayout(&dlg);
    lay->addWidget(new QLabel(QString::fromUtf8(
        "CCTV 는 이 앱이 카메라에 직접 연결합니다(RPi 경유 아님).\n"
        "4개 채널은 같은 카메라의 센서 0~3 번입니다."), &dlg));
    lay->addLayout(form);
    lay->addWidget(box);

    if (dlg.exec() != QDialog::Accepted) return;
    if (host->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("카메라 설정"),
                             QString::fromUtf8("카메라 IP 를 입력하세요."));
        return;
    }

    // 비밀번호를 비웠으면 기존 URL 의 것을 그대로 쓴다.
    QString pw = pass->text();
    if (pw.isEmpty() && f.isOpen()) {
        f.seek(0);
        const auto ch = QJsonDocument::fromJson(f.readAll()).object()
                            .value(QStringLiteral("channels")).toObject();
        if (!ch.isEmpty()) pw = QUrl(ch.begin().value().toString()).password();
    }

    QJsonObject cams;
    cams.insert(QStringLiteral("channels"),
                CameraConfig::buildChannels(host->text(), user->text().trimmed(), pw));

    const QString dir = userDataRoot() + QStringLiteral("/config");
    QDir().mkpath(dir);
    QFile out(dir + QStringLiteral("/cameras.json"));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, QString::fromUtf8("카메라 설정"),
                              QString::fromUtf8("설정을 저장하지 못했습니다:\n%1").arg(out.fileName()));
        return;
    }
    out.write(QJsonDocument(cams).toJson(QJsonDocument::Indented));
    out.close();
#ifndef Q_OS_WIN
    QFile::setPermissions(out.fileName(), QFile::ReadOwner | QFile::WriteOwner);
#endif

    // 재시작 없이 바로 적용한다.
    m_video->applyChannels(cams.value(QStringLiteral("channels")).toObject(),
                           QString::fromUtf8("카메라 설정"));

    if (m_settingsTab != nullptr) {
        QString h; int n = 0;
        cameraSummary(&h, &n);
        m_settingsTab->setCameraSummary(h, n);
    }
}

void MainWindow::logout() {
    const auto btn = QMessageBox::question(
        this, QString::fromUtf8("로그아웃"),
        QString::fromUtf8(
            "이 기기에 저장된 인증서와 접속 설정을 삭제합니다.\n"
            "다시 사용하려면 새 토큰을 발급받아야 합니다.\n\n계속할까요?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (btn != QMessageBox::Yes) return;

    const QString root = userDataRoot();
    if (!QDir(root).removeRecursively()) {
        QMessageBox::critical(
            this, QString::fromUtf8("삭제 실패"),
            QString::fromUtf8("아래 폴더를 지우지 못했습니다. 직접 삭제해 주세요.\n\n%1").arg(root));
        return;
    }

    // 여기서 마법사를 다시 띄우지 않고 종료한다. MQTT 연결과 RTSP 디코더를 살아
    // 있는 채로 갈아끼우려면 정리 경로가 필요한데, 재시작이 훨씬 단순하고 확실하다.
    QMessageBox::information(
        this, QString::fromUtf8("로그아웃 완료"),
        QString::fromUtf8("설정을 삭제했습니다. 앱을 종료합니다.\n다시 실행하면 등록 화면이 나타납니다."));
    close();
}

void MainWindow::setDemoMode(bool demo) {
    m_demoMode = demo;
    if (m_settingsTab != nullptr) m_settingsTab->setDemoMode(demo);
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
    // 8443 TLS 검증에 쓸 이름. 서버 인증서가 CN=raspberrypi 로 발급되므로 기본값도
    // 그것으로 둔다 — ScanFetcher::makeRequest 주석 참고. 인증서를 현재 주소로
    // 재발급했다면 mqtt.json 에서 ""로 덮어써 host 검증으로 되돌리면 된다.
    QString serverName = "raspberrypi";
    QFile f(resolveConfigPath("config/mqtt.json"));
    if (f.open(QIODevice::ReadOnly)) {
        const auto o = QJsonDocument::fromJson(f.readAll()).object();
        host    = o.value("host").toString(host);
        port    = static_cast<quint16>(o.value("port").toInt(port));
        certDir = o.value("cert_dir").toString();
        if (o.contains("server_name")) serverName = o.value("server_name").toString();
        // cert_dir 은 보통 "certs" 같은 상대경로다. 그대로 넘기면 MqttBridge 가
        // 프로세스 CWD 기준으로 찾는데, Finder 더블클릭이나 IDE 실행이면 CWD 가
        // "/" 라 인증서를 못 찾는다. 그러면 평문 tcp:// 로 degraded 접속하고,
        // 브로커 8883 은 TLS 전용이라 조용히 실패한다. mqtt.json 과 같은 방식으로
        // 실행파일 위치 기준으로 해석해 CWD 와 무관하게 만든다.
        if (!certDir.isEmpty()) {
            certDir = resolveConfigPath(certDir);
        }
    }
    m_mqtt->connectToBroker(host, port, certDir);

    // 스캔 파일도 같은 RPi 에서 받는다 — 브로커(8883)와 발급 서비스(8443)가
    // 같은 호스트에 있다. 포트만 다르다.
    m_scanFetcher->setServer(host, 8443);
    m_scanFetcher->setPeerVerifyName(serverName);
    if (!certDir.isEmpty()) {
        // QSslKey 는 PKCS#8 을 못 읽고 조용히 null 을 돌려준다 — gen-certs.sh 가
        // 같이 만들어 주는 전통 RSA(-trad) 쪽을 먼저 본다.
        const QString trad = certDir + QStringLiteral("/qt-console-trad.key");
        const QString plain = certDir + QStringLiteral("/qt-console.key");
        m_scanFetcher->setClientCert(certDir + QStringLiteral("/qt-console.crt"),
                                     QFileInfo::exists(trad) ? trad : plain);
    }
}

void MainWindow::editSensorHeight() {
    // 입력은 미터로 받는다 — 설치할 때 재는 단위가 그쪽이고, mm 로 0 을 더 붙이다
    // 자릿수를 틀리기 쉽다. 계약(§3.1)이 mm 정수라 저장 직전에 환산한다.
    bool ok = false;
    const double m = QInputDialog::getDouble(
        this, QString::fromUtf8("센서 높이"),
        QString::fromUtf8("바닥에서 라이다 회전축까지의 높이 (m)\n\n"
                          "좌표 계산에는 쓰이지 않습니다. 스캔 결과 .pcd 헤더에\n"
                          "sensor_height_m 으로 기록돼, 나중에 바닥평면을 잡거나\n"
                          "다른 좌표계로 옮길 때 쓰입니다. 0 은 \"모름\"입니다."),
        m_sensorHeightMm / 1000.0, 0.0, 10.0, 3, &ok);
    if (!ok) return;

    m_sensorHeightMm = static_cast<int>(qRound(m * 1000.0));
    QSettings().setValue(QStringLiteral("scan/sensor_height_mm"), m_sensorHeightMm);
    SpatialProjector::instance().setGlobalGroundY(m_sensorHeightMm / 1000.0);
    appendLog("SCAN", QString::fromUtf8("센서 높이 %1 m (%2 mm) — 실시간 투영 및 다음 스캔 적용")
                          .arg(m, 0, 'f', 3).arg(m_sensorHeightMm));
    if (m_settingsTab != nullptr) m_settingsTab->setSensorHeight(m_sensorHeightMm);
}

void MainWindow::openScanFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, QString::fromUtf8("스캔 파일 열기"), QString(),
        QString::fromUtf8("포인트클라우드 (*.pcd);;모든 파일 (*)"));
    if (path.isEmpty()) return;
    m_scanFetcher->loadLocal(path);
}

void MainWindow::openCalibrationResultFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, QString::fromUtf8("OpenSDK 캘리브레이션 결과 파일 열기"), QString(),
        QString::fromUtf8("캘리브레이션 결과 (*.json);;모든 파일 (*)"));
    if (path.isEmpty()) return;

    QString summary;
    const bool success = SpatialProjector::instance().loadCalibrationResultJson(path, 1, &summary);
    if (success) {
        appendLog("CALIB", QString::fromUtf8("최신 캘리브레이션 결과 불러오기 성공: %1").arg(path));
        appendLog("CALIB", summary);
        QMessageBox::information(this, QString::fromUtf8("캘리브레이션 적용 완료"),
                                 QString::fromUtf8("최신 캘리브레이션 결과가 성공적으로 적용되었습니다.\n\n%1").arg(summary));
        if (m_topView) m_topView->update();
    } else {
        appendLog("CALIB", QString::fromUtf8("캘리브레이션 결과 불러오기 실패: %1").arg(summary));
        QMessageBox::warning(this, QString::fromUtf8("불러오기 실패"),
                             QString::fromUtf8("캘리브레이션 결과를 적용하지 못했습니다.\n\n사유: %1").arg(summary));
    }
}

void MainWindow::openIntrinsicProfileFile(int channel) {
    const QString targetStr = (channel >= 1 && channel <= 4)
                                  ? QStringLiteral("CH%1 (센서 %2)").arg(channel).arg(channel - 1)
                                  : QStringLiteral("전체 채널 자동");
    const QString title = QString::fromUtf8("[%1] 카메라 내부 파라미터(Intrinsic) 파일 열기").arg(targetStr);

    const QString path = QFileDialog::getOpenFileName(
        this, title, QString(),
        QString::fromUtf8("카메라 파라미터 (*.json);;모든 파일 (*)"));
    if (path.isEmpty()) return;

    QString summary;
    const bool success = SpatialProjector::instance().loadIntrinsicProfileJson(path, channel, &summary);
    if (success) {
        appendLog("CALIB", QString::fromUtf8("[%1] 내부 파라미터 불러오기 성공: %2").arg(targetStr, path));
        appendLog("CALIB", summary);
        QMessageBox::information(this, QString::fromUtf8("내부 파라미터 적용 완료"),
                                 QString::fromUtf8("[%1] 카메라 내부 파라미터가 성공적으로 적용되었습니다.\n\n%2").arg(targetStr, summary));
        if (m_topView) m_topView->update();
    } else {
        appendLog("CALIB", QString::fromUtf8("[%1] 내부 파라미터 불러오기 실패: %2").arg(targetStr, summary));
        QMessageBox::warning(this, QString::fromUtf8("불러오기 실패"),
                             QString::fromUtf8("[%1] 내부 파라미터를 적용하지 못했습니다.\n\n사유: %2").arg(targetStr, summary));
    }
}

void MainWindow::openManualRtFile(int channel) {
    const QString targetStr = (channel >= 1 && channel <= 4)
                                  ? QStringLiteral("CH%1 (센서 %2)").arg(channel).arg(channel - 1)
                                  : QStringLiteral("전체 채널 자동");
    const QString title = QString::fromUtf8("[%1] Manual RT (외부 파라미터) 파일 열기").arg(targetStr);

    const QString path = QFileDialog::getOpenFileName(
        this, title, QString(),
        QString::fromUtf8("Manual RT (*.json);;모든 파일 (*)"));
    if (path.isEmpty()) return;

    QString summary;
    const bool success = SpatialProjector::instance().loadManualExtrinsicJson(path, channel, &summary);
    if (success) {
        appendLog("CALIB", QString::fromUtf8("[%1] Manual RT 불러오기 성공: %2").arg(targetStr, path));
        appendLog("CALIB", summary);
        if (m_settingsTab != nullptr) {
            m_settingsTab->setCalibMode(true);
        }
        QSettings().setValue(QStringLiteral("calib/is_manual"), true);
        QMessageBox::information(this, QString::fromUtf8("Manual RT 적용 완료"),
                                 QString::fromUtf8("[%1] 수동 캘리브레이션 RT가 적용되었으며 Manual RT 모드로 전환되었습니다.\n\n%2").arg(targetStr, summary));
        if (m_topView) m_topView->update();
    } else {
        appendLog("CALIB", QString::fromUtf8("[%1] Manual RT 불러오기 실패: %2").arg(targetStr, summary));
        QMessageBox::warning(this, QString::fromUtf8("불러오기 실패"),
                             QString::fromUtf8("[%1] Manual RT를 적용하지 못했습니다.\n\n사유: %2").arg(targetStr, summary));
    }
}

void MainWindow::downloadCalibrationResult(const QString &sessionId,
                                           const QString &downloadUrl,
                                           const QString &downloadFileName) {
    QString host = QStringLiteral("172.20.32.43");
    QString user = QStringLiteral("admin");
    QString password;

    QFile f(resolveConfigPath(QStringLiteral("config/cameras.json")));
    if (f.open(QIODevice::ReadOnly)) {
        const auto ch = QJsonDocument::fromJson(f.readAll()).object()
                            .value(QStringLiteral("channels")).toObject();
        if (!ch.isEmpty()) {
            const QUrl u(ch.begin().value().toString());
            if (!u.host().isEmpty()) host = u.host();
            if (!u.userName().isEmpty()) user = u.userName();
            if (!u.password().isEmpty()) password = u.password();
        }
    }

    QString safeFileName = QFileInfo(downloadFileName).fileName();
    if (safeFileName.isEmpty()) {
        QString safeSessionId = sessionId;
        safeSessionId.replace('/', '_');
        safeSessionId.replace('\\', '_');
        if (safeSessionId.isEmpty()) safeSessionId = QStringLiteral("calibration");
        safeFileName = safeSessionId + QStringLiteral("_calibration_result.json");
    }

    QString downloadDirectory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (downloadDirectory.isEmpty()) downloadDirectory = QDir::homePath();
    const QString savePath = QFileDialog::getSaveFileName(
        this,
        QString::fromUtf8("캘리브레이션 결과 저장"),
        QDir(downloadDirectory).filePath(safeFileName),
        QString::fromUtf8("캘리브레이션 결과 (*.json);;모든 파일 (*)"));
    if (savePath.isEmpty()) return;

    const QString apiBase = QStringLiteral("%1://%2/opensdk/calibration")
                                .arg(QLatin1String(kCalibScheme), host);
    const QString relativePath = downloadUrl.trimmed();
    QUrl resultUrl;
    const QUrl suppliedUrl(relativePath);
    if (!suppliedUrl.isRelative()) {
        resultUrl = suppliedUrl;
    } else if (relativePath.startsWith(QStringLiteral("/opensdk/"))) {
        resultUrl = QUrl(QStringLiteral("%1://%2%3")
                             .arg(QLatin1String(kCalibScheme), host, relativePath));
    } else {
        const QString separator = relativePath.startsWith('/') ? QString() : QStringLiteral("/");
        resultUrl = QUrl(apiBase + separator + relativePath);
    }

    if (!resultUrl.isValid() || resultUrl.scheme() != QLatin1String(kCalibScheme) ||
        resultUrl.host().compare(host, Qt::CaseInsensitive) != 0) {
        const QString error = QString::fromUtf8("서버가 잘못된 결과 다운로드 주소를 반환했습니다: %1")
                                  .arg(downloadUrl);
        appendLog("CALIB", error);
        QMessageBox::warning(this, QString::fromUtf8("다운로드 주소 오류"), error);
        return;
    }

    appendLog("CALIB", QString::fromUtf8("CCTV 캘리브레이션 결과 다운로드 시작: %1 (%2)")
                           .arg(sessionId, resultUrl.toString()));

    auto *mgr = new QNetworkAccessManager(this);
    connect(mgr, &QNetworkAccessManager::authenticationRequired, this,
            [user, password](QNetworkReply *, QAuthenticator *auth) {
        auth->setUser(user);
        auth->setPassword(password);
    });

    QNetworkRequest req(resultUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!user.isEmpty() && !password.isEmpty()) {
        const QString credentials = QStringLiteral("%1:%2").arg(user, password);
        req.setRawHeader("Authorization", "Basic " + credentials.toUtf8().toBase64());
    }

    QNetworkReply *reply = mgr->get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, mgr, sessionId, savePath] {
        const QByteArray responseData = reply->readAll();
        reply->deleteLater();
        mgr->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QString error = QString::fromUtf8("캘리브레이션 결과 다운로드 실패: HTTP %1, %2")
                                      .arg(httpStatus)
                                      .arg(reply->errorString());
            appendLog("CALIB", error);
            QMessageBox::warning(this, QString::fromUtf8("다운로드 실패"), error);
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);
        if (!doc.isObject()) {
            const QString error = QString::fromUtf8("다운로드 응답이 올바른 JSON이 아닙니다: %1")
                                      .arg(parseError.errorString());
            appendLog("CALIB", error);
            QMessageBox::warning(this, QString::fromUtf8("응답 형식 오류"), error);
            return;
        }

        QSaveFile output(savePath);
        if (!output.open(QIODevice::WriteOnly) || output.write(responseData) != responseData.size() ||
            !output.commit()) {
            const QString error = QString::fromUtf8("다운로드 파일 저장 실패: %1").arg(savePath);
            appendLog("CALIB", error);
            QMessageBox::warning(this, QString::fromUtf8("파일 저장 실패"), error);
            return;
        }

        // 캘리브레이션 결과 자동 적용
        QString summary;
        bool applied = SpatialProjector::instance().loadCalibrationResultData(responseData, 1, &summary);

        appendLog("CALIB", QString::fromUtf8("CCTV 캘리브레이션 결과 다운로드 완료: %1 -> %2")
                               .arg(sessionId, savePath));

        QString msg = QString::fromUtf8("캘리브레이션 결과를 저장했습니다.\n\n%1").arg(savePath);
        if (applied) {
            msg += QString::fromUtf8("\n\n✅ 3D/2D Top-View 에 즉시 적용되었습니다:\n%1").arg(summary);
            if (m_topView) m_topView->update();
        }

        QMessageBox::information(this, QString::fromUtf8("다운로드 및 적용 완료"), msg);
    });
}

void MainWindow::showScanListDialog() {
    if (m_scanListDialog == nullptr) {
        m_scanListDialog = new ScanListDialog(this);
        connect(m_scanListDialog, &ScanListDialog::refreshRequested, this, [this] {
            m_scanFetcher->refreshList();
        });
        connect(m_scanListDialog, &ScanListDialog::scanChosen, this, [this](const QString &name, const QString &localPath) {
            if (!localPath.isEmpty()) {
                m_scanFetcher->loadLocal(localPath);
            } else {
                m_scanFetcher->fetch(name);
            }
        });
        connect(m_scanListDialog, &ScanListDialog::openLocalFileRequested, this, &MainWindow::openScanFile);
    }
    m_scanFetcher->refreshList();
    m_scanListDialog->show();
    m_scanListDialog->raise();
    m_scanListDialog->activateWindow();
}

void MainWindow::showCameraCalibDialog() {
    QString host = QStringLiteral("172.20.32.43");
    QString user = QStringLiteral("admin");
    QString password;

    QFile f(resolveConfigPath(QStringLiteral("config/cameras.json")));
    if (f.open(QIODevice::ReadOnly)) {
        const auto ch = QJsonDocument::fromJson(f.readAll()).object()
                            .value(QStringLiteral("channels")).toObject();
        if (!ch.isEmpty()) {
            const QUrl u(ch.begin().value().toString());
            if (!u.host().isEmpty()) host = u.host();
            if (!u.userName().isEmpty()) user = u.userName();
            if (!u.password().isEmpty()) password = u.password();
        }
    }

    if (m_cameraCalibDialog == nullptr) {
        m_cameraCalibDialog = new CameraCalibDialog(this);
        connect(m_cameraCalibDialog, &CameraCalibDialog::openLocalFileRequested, this, &MainWindow::openCalibrationResultFile);
        connect(m_cameraCalibDialog, &CameraCalibDialog::refreshRequested, this, [this] {
            showCameraCalibDialog();
        });
        connect(m_cameraCalibDialog, &CameraCalibDialog::downloadRequested,
                this, &MainWindow::downloadCalibrationResult);
    }

    m_cameraCalibDialog->setCameraInfo(host, QString::fromUtf8("조회 중…"), QString());

    auto *mgr = new QNetworkAccessManager(this);
    connect(mgr, &QNetworkAccessManager::authenticationRequired, this,
            [user, password](QNetworkReply *, QAuthenticator *auth) {
        auth->setUser(user);
        auth->setPassword(password);
    });

    const QUrl resultsUrl(QStringLiteral("%1://%2/opensdk/calibration/results")
                              .arg(QLatin1String(kCalibScheme), host));

    QNetworkRequest request(resultsUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!user.isEmpty() && !password.isEmpty()) {
        const QString credentials = QStringLiteral("%1:%2").arg(user, password);
        request.setRawHeader("Authorization", "Basic " + credentials.toUtf8().toBase64());
    }

    appendLog("CALIB", QString::fromUtf8("CCTV 캘리브레이션 결과 목록 조회: %1")
                           .arg(resultsUrl.toString()));
    QNetworkReply *reply = mgr->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, mgr, host] {
        const QByteArray responseData = reply->readAll();
        reply->deleteLater();
        mgr->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QString error = QString::fromUtf8("결과 목록 조회 실패: HTTP %1, %2")
                                      .arg(httpStatus)
                                      .arg(reply->errorString());
            appendLog("CALIB", error);
            if (m_cameraCalibDialog != nullptr) {
                m_cameraCalibDialog->setCameraInfo(host, QString::fromUtf8("연결 실패"), QString());
                m_cameraCalibDialog->setEntries({});
                m_cameraCalibDialog->setErrorMessage(error);
            }
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(responseData, &parseError);
        if (!document.isObject() || !document.object().value(QStringLiteral("results")).isArray()) {
            const QString error = QString::fromUtf8("결과 목록 응답 형식 오류: %1")
                                      .arg(parseError.error == QJsonParseError::NoError
                                               ? QString::fromUtf8("results 배열이 없습니다.")
                                               : parseError.errorString());
            appendLog("CALIB", error);
            if (m_cameraCalibDialog != nullptr) {
                m_cameraCalibDialog->setCameraInfo(host, QString::fromUtf8("응답 오류"), QString());
                m_cameraCalibDialog->setEntries({});
                m_cameraCalibDialog->setErrorMessage(error);
            }
            return;
        }

        QVector<CameraCalibEntry> entries;
        const QJsonArray results = document.object().value(QStringLiteral("results")).toArray();
        entries.reserve(results.size());
        for (const QJsonValue &value : results) {
            if (!value.isObject()) continue;
            const QJsonObject object = value.toObject();
            CameraCalibEntry entry;
            entry.sessionId = object.value(QStringLiteral("session_id")).toString();
            entry.lidarFileName = object.value(QStringLiteral("lidar_file_name")).toString();
            entry.lidarFileBytes = object.value(QStringLiteral("lidar_file_bytes")).toInteger();
            entry.state = object.value(QStringLiteral("state")).toString();
            entry.detail = object.value(QStringLiteral("detail")).toString();
            entry.resultFileName = object.value(QStringLiteral("result_file_name")).toString();
            entry.downloadFileName = object.value(QStringLiteral("download_file_name")).toString();
            entry.resultFileBytes = object.value(QStringLiteral("result_file_bytes")).toInteger();
            entry.downloadUrl = object.value(QStringLiteral("download_url")).toString();
            entry.resultAvailable = object.value(QStringLiteral("result_available")).toBool() &&
                                    !entry.downloadUrl.isEmpty();
            entries.append(entry);
        }

        const QString status = entries.isEmpty() ? QString::fromUtf8("결과 없음")
                                                  : entries.constFirst().state;
        const QString currentSession = entries.isEmpty() ? QString() : entries.constFirst().sessionId;
        appendLog("CALIB", QString::fromUtf8("CCTV 캘리브레이션 결과 목록 %1건 조회 완료")
                               .arg(entries.size()));
        if (m_cameraCalibDialog != nullptr) {
            m_cameraCalibDialog->setCameraInfo(host, status, currentSession);
            m_cameraCalibDialog->setEntries(entries);
        }
    });

    m_cameraCalibDialog->show();
    m_cameraCalibDialog->raise();
    m_cameraCalibDialog->activateWindow();
}

void MainWindow::appendLog(const QString &tag, const QString &msg) {
    m_calibTab->appendLog(tag, msg);
    m_eventsTab->appendEvent(tag, sourceForTag(tag), msg);
}
