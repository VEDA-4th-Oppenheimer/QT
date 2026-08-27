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
class SettingsTab;
class DataBridge;
class MqttBridge;
class DemoBridge;
class RtspSource;
class ScanFetcher;
class ScanListDialog;
class CameraCalibDialog;
class QTabWidget;
class QSplitter;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    // 전체화면으로 뺀 TOP-VIEW 창이 닫힐 때(Cmd+W·창 닫기) 패널을 되찾아온다.
    // 안 그러면 패널이 그대로 삭제돼 대시보드 우측이 빈 채로 남는다.
    bool eventFilter(QObject *watched, QEvent *ev) override;

private:
    QWidget *buildDashboardTab();
    // cameras.json 에서 SETTINGS 탭에 보여줄 요약(호스트·채널 수)을 뽑는다.
    void cameraSummary(QString *host, int *channels) const;
    void rebuildUi();          // 테마 전환 시 중앙 위젯을 통째로 다시 만든다
    void setThemeMode(Theme::Mode mode);
    void setDemoMode(bool demo);
    // 최초 실행 설정. 접속 설정이 없으면 등록 마법사를 띄워 1회용 토큰으로
    // 인증서·카메라 설정을 받아온다. 설정이 갖춰졌으면 true.
    bool ensureConfigured();
    // 사용자 데이터(인증서·설정)를 지우고 앱을 종료한다. 다음 실행 때 마법사가 뜬다.
    void logout();
    // 모달 다이얼로그의 부모 창(전체화면으로 빠진 TOP-VIEW 창 대응).
    QWidget *modalParent();
    // 등록 후에도 카메라를 바꿀 수 있게 한다. 재등록을 시키면 인증서까지 다시
    // 받아야 해서 과하다 — 카메라는 인증서와 무관하다.
    void editCameraSettings();
    // 지면→라이다 회전축 높이. 설치할 때 한 번 실측해 넣는 값이라 메뉴에 둔다.
    void editSensorHeight();
    void appendLog(const QString &tag, const QString &msg);
    // TOP-VIEW 를 별도 창(전체화면)으로 빼거나 대시보드로 되돌린다. 패널을 복제
    // 하지 않고 위젯 자체를 옮긴다 — 두 벌이면 스캔 점군·IMU 배선을 이중으로
    // 유지해야 하고, 어느 쪽이 최신인지 헷갈린다.
    void toggleTopViewFullScreen();
    // 0 이면 4분할, 1~4 면 그 채널 타일만 남기고 나머지를 감춘다.
    void setSoloChannel(int channel);
    void detachTopView();
    void attachTopView();
    // 스캔 완료(state/scan) 시 .pcd 를 받아 Top-View 에 깔기까지의 배선.
    void configureScanFetcher();
    void openScanFile();
    void openCalibrationResultFile();
    void openIntrinsicProfileFile(int channel = 0);
    void openManualRtFile(int channel = 0);
    void downloadCalibrationResult(const QString &sessionId,
                                   const QString &downloadUrl,
                                   const QString &downloadFileName);
    void showCameraCalibDialog();
    void showScanListDialog();

    TopBar         *m_topBar    = nullptr;
    TiltBanner     *m_banner    = nullptr;
    StatusBar      *m_statusBar = nullptr;
    TopViewPanel   *m_topView   = nullptr;
    CalibrationTab *m_calibTab  = nullptr;
    DevicesTab     *m_devicesTab= nullptr;
    EventLogTab    *m_eventsTab = nullptr;
    SettingsTab    *m_settingsTab = nullptr;
    ScanListDialog *m_scanListDialog = nullptr;
    CameraCalibDialog *m_cameraCalibDialog = nullptr;
    CameraTile     *m_tiles[4]  = {nullptr, nullptr, nullptr, nullptr};

    // 대시보드 좌(CCTV 4채널)/우(TOP-VIEW) 분할. 사용자가 핸들을 끌어 비율을
    // 바꾸고, 그 비율은 QSettings 에 남겨 재실행·테마 전환에도 유지된다.
    QSplitter *m_dashSplitter = nullptr;
    QByteArray m_splitterState;
    // 이 프로세스에서 대시보드를 처음 만드는지. 첫 회에만 TOP-VIEW 를 하한으로
    // 접는다 — 테마 전환으로 다시 만들 때까지 초기화하면 끌어둔 비율이 날아간다.
    bool m_dashInitialized = false;
    // rebuildUi 가 탭 위젯을 새로 만들기 때문에, 보고 있던 탭을 기억해
    // 테마를 바꿔도 같은 자리에 남게 한다.
    int m_activeTab = 0;
    // 1채널 크게보기 상태(0 = 4분할). rebuildUi 로 타일을 새로 만들어도
    // 보고 있던 채널이 유지되도록 MainWindow 가 들고 있는다.
    int m_soloChannel = 0;
    // TOP-VIEW 가 별도 창에 나가 있는 동안 스플리터 자리를 지키는 안내 라벨.
    // 비워두면 스플리터가 좌측 칸을 폭 전체로 늘려서 되돌릴 때 비율이 깨진다.
    QWidget *m_topViewWindow = nullptr;
    QLabel  *m_topViewSlot   = nullptr;

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
    QVector<SpatialObject> m_lastObjects;
    bool m_haveDaemonState = false, m_haveImu = false, m_haveProgress = false, m_haveResult = false;
    bool m_haveCloud = false, m_haveObjects = false;
    // 이번 실행에서 사용자가 스캔을 걸었는지. adts/state/scan 은 retained 라
    // 접속하자마자 지난 스캔 결과가 그대로 배달된다 — 그걸 자동으로 받아 깔면
    // 앱을 켜자마자 TOP-VIEW 에 예전 점군이 떠 있다. 사용자가 스캔을 돌렸을
    // 때만 자동 표시하고, 그 전에는 파일을 직접 고르게 한다.
    bool m_scanRequestedThisRun = false;
    bool m_rtspMetadataActive = false;
    // 브로커 연결 상태도 캐싱해야 한다 — 연결이 유지되는 동안에는 신호가 다시
    // 오지 않아서, 테마 전환으로 TopBar 를 새로 만들면 생성자 기본값
    // (DISCONNECTED)에 갇힌 채 영영 정정되지 않는다.
    bool m_lastBrokerUp = false, m_haveBrokerState = false;
};
