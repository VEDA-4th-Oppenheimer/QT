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
#include "EnrollDialog.h"
#include "ScanFetcher.h"
#include <QFileDialog>
#include "CameraConfig.h"
#include "Theme.h"
#include "ConfigPath.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QUrl>
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

    m_mqtt = new MqttBridge(this);
    m_demo = new DemoBridge(this);
    m_video = new RtspSource(this);
    m_scanFetcher = new ScanFetcher(this);

    // ScanFetcher 는 MainWindow 소유라 rebuildUi 로 위젯이 갈려도 살아남는다.
    // 여기서 한 번만 연결하고, 람다는 호출 시점의 m_topView 를 본다.
    connect(m_scanFetcher, &ScanFetcher::cloudReady, this, [this](const ScanCloud &c) {
        m_lastCloud = c; m_haveCloud = true;
        m_topView->setScanCloud(c);
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
        if (m_topView != nullptr) m_topView->setScanList(e, note);
    });

    auto *modeMenu = menuBar()->addMenu(QString::fromUtf8("모드"));
    auto *demoAction = modeMenu->addAction(QString::fromUtf8("Demo Mode (브로커 없이 실행)"));
    demoAction->setCheckable(true);
    demoAction->setChecked(false);
    connect(demoAction, &QAction::toggled, this, &MainWindow::setDemoMode);

    modeMenu->addSeparator();
    auto *camAction = modeMenu->addAction(QString::fromUtf8("카메라 설정…"));
    connect(camAction, &QAction::triggered, this, &MainWindow::editCameraSettings);

    auto *openScanAction = modeMenu->addAction(QString::fromUtf8("스캔 파일 열기… (.pcd)"));
    connect(openScanAction, &QAction::triggered, this, &MainWindow::openScanFile);

    auto *logoutAction = modeMenu->addAction(QString::fromUtf8("로그아웃 (인증서·설정 삭제)"));
    connect(logoutAction, &QAction::triggered, this, &MainWindow::logout);

    // 개발자모드(다크 + 한화비전 오렌지 액센트) / 사용자모드(라이트) — 화면 구성은
    // 동일, 색만 바뀐다. Qt 스타일시트는 위젯 생성 시점에 굳어버리므로(재적용
    // 안 됨), 전환 시 중앙 위젯을 통째로 다시 만든다(rebuildUi 참고).
    auto *themeMenu = menuBar()->addMenu(QString::fromUtf8("테마"));
    auto *devModeAction = themeMenu->addAction(QString::fromUtf8("개발자 모드 (다크 · 한화비전)"));
    auto *userModeAction = themeMenu->addAction(QString::fromUtf8("사용자 모드 (라이트)"));
    devModeAction->setCheckable(true);
    userModeAction->setCheckable(true);
    devModeAction->setChecked(true);
    auto *themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);
    themeGroup->addAction(devModeAction);
    themeGroup->addAction(userModeAction);
    connect(devModeAction, &QAction::triggered, this, [this] { setThemeMode(Theme::Mode::Developer); });
    connect(userModeAction, &QAction::triggered, this, [this] { setThemeMode(Theme::Mode::User); });

    rebuildUi();

    // 설정이 없으면 최초 설정을 받는다. 사용자가 취소하면 브로커 접속을 시도하는
    // 대신 Demo 모드로 띄운다 — 설정 없이 접속을 걸면 5초마다 재시도만 반복하고
    // 화면은 비어 있어서, 빈 실화면보다 데모가 낫다.
    // (메뉴 체크 상태를 함께 맞춰야 하므로 setDemoMode 직접 호출 대신 액션을 켠다)
    if (ensureConfigured()) {
        setDemoMode(false);
    } else {
        demoAction->setChecked(true);
    }
    m_video->loadConfigAndStart();
}

void MainWindow::setThemeMode(Theme::Mode mode) {
    if (Theme::CurrentMode == mode) return;
    Theme::setMode(mode);
    rebuildUi();
}

void MainWindow::rebuildUi() {
    setStyleSheet(Theme::appStyleSheet());

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

    // 3D 칸의 파일 목록 — 위젯이 새로 만들어졌으므로 매번 다시 잇는다.
    connect(m_topView, &TopViewPanel::refreshRequested, this, [this] {
        m_scanFetcher->refreshList();
    });
    connect(m_topView, &TopViewPanel::scanChosen, this,
            [this](const QString &name, const QString &localPath) {
        if (!localPath.isEmpty()) m_scanFetcher->loadLocal(localPath);
        else                      m_scanFetcher->fetch(name);
    });
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

bool MainWindow::ensureConfigured() {
    if (configReady()) return true;

    // 배포본 최초 실행. 인증서(MQTT)와 카메라 설정(RTSP)을 발급 서버에서 한 번에
    // 받아오므로, 사용자는 토큰만 입력하면 둘 다 붙는다.
    EnrollDialog dlg(this);
    return (dlg.exec() == QDialog::Accepted) && configReady();
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

void MainWindow::openScanFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, QString::fromUtf8("스캔 파일 열기"), QString(),
        QString::fromUtf8("포인트클라우드 (*.pcd);;모든 파일 (*)"));
    if (path.isEmpty()) return;
    m_scanFetcher->loadLocal(path);
}

void MainWindow::appendLog(const QString &tag, const QString &msg) {
    m_calibTab->appendLog(tag, msg);
    m_eventsTab->appendEvent(tag, sourceForTag(tag), msg);
}
