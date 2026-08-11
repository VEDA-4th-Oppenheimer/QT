#pragma once
#include <QMainWindow>
#include "Models.h"
#include "ScanCloud.h"
#include "Theme.h"

class TopBar;
class TiltBanner;
class StatusBar;
class CameraTile;
class TopViewPanel;
class CalibrationTab;
class DevicesTab;
class EventLogTab;
class DataBridge;
class MqttBridge;
class DemoBridge;
class RtspSource;
class ScanFetcher;
class QTabWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    QWidget *buildDashboardTab();
    void rebuildUi();          // 테마 전환 시 중앙 위젯을 통째로 다시 만든다
    void setThemeMode(Theme::Mode mode);
    void setDemoMode(bool demo);
    // 최초 실행 설정. 접속 설정이 없으면 등록 마법사를 띄워 1회용 토큰으로
    // 인증서·카메라 설정을 받아온다. 설정이 갖춰졌으면 true.
    bool ensureConfigured();
    // 사용자 데이터(인증서·설정)를 지우고 앱을 종료한다. 다음 실행 때 마법사가 뜬다.
    void logout();
    // 등록 후에도 카메라를 바꿀 수 있게 한다. 재등록을 시키면 인증서까지 다시
    // 받아야 해서 과하다 — 카메라는 인증서와 무관하다.
    void editCameraSettings();
    // 지면→라이다 회전축 높이. 설치할 때 한 번 실측해 넣는 값이라 메뉴에 둔다.
    void editSensorHeight();
    void appendLog(const QString &tag, const QString &msg);
    // 스캔 완료(state/scan) 시 .pcd 를 받아 Top-View 에 깔기까지의 배선.
    void configureScanFetcher();
    void openScanFile();

    TopBar         *m_topBar    = nullptr;
    TiltBanner     *m_banner    = nullptr;
    StatusBar      *m_statusBar = nullptr;
    TopViewPanel   *m_topView   = nullptr;
    CalibrationTab *m_calibTab  = nullptr;
    DevicesTab     *m_devicesTab= nullptr;
    EventLogTab    *m_eventsTab = nullptr;
    CameraTile     *m_tiles[4]  = {nullptr, nullptr, nullptr, nullptr};

    MqttBridge *m_mqtt = nullptr;
    DemoBridge *m_demo = nullptr;
    RtspSource *m_video = nullptr;
    ScanFetcher *m_scanFetcher = nullptr;
    bool m_demoMode = false;

    // cmd/scan 의 sensor_height_mm. QSettings 에 남겨 재실행해도 유지한다.
    // 좌표에는 안 들어가고 .pcd 헤더 메타데이터로만 나간다(계약 §3.1).
    int m_sensorHeightMm = 2400;

    // 테마 전환 시 위젯을 다시 만들기 때문에(rebuildUi), 새 위젯이 다음 업데이트
    // 전까지 기본값(OFFLINE 등)으로 잠깐 보이지 않도록 마지막 값을 캐싱해둔다.
    DaemonState  m_lastDaemonState;
    ImuState     m_lastImu;
    ScanProgress m_lastProgress;
    ScanResult   m_lastResult;
    ScanCloud    m_lastCloud;
    bool m_haveDaemonState = false, m_haveImu = false, m_haveProgress = false, m_haveResult = false;
    bool m_haveCloud = false;
    // 브로커 연결 상태도 캐싱해야 한다 — 연결이 유지되는 동안에는 신호가 다시
    // 오지 않아서, 테마 전환으로 TopBar 를 새로 만들면 생성자 기본값
    // (DISCONNECTED)에 갇힌 채 영영 정정되지 않는다.
    bool m_lastBrokerUp = false, m_haveBrokerState = false;
};
