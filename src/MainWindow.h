#pragma once
#include <QMainWindow>
#include "Models.h"
#include "Theme.h"

class TopBar;
class TiltBanner;
class StatusBar;
class CameraTile;
class TopViewPanel;
class CalibrationTab;
class DevicesTab;
class DatasetTab;
class EventLogTab;
class DataBridge;
class MqttBridge;
class DemoBridge;
class RtspSource;
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
    void appendLog(const QString &tag, const QString &msg);

    TopBar         *m_topBar    = nullptr;
    TiltBanner     *m_banner    = nullptr;
    StatusBar      *m_statusBar = nullptr;
    TopViewPanel   *m_topView   = nullptr;
    CalibrationTab *m_calibTab  = nullptr;
    DevicesTab     *m_devicesTab= nullptr;
    DatasetTab     *m_datasetTab= nullptr;
    EventLogTab    *m_eventsTab = nullptr;
    CameraTile     *m_tiles[4]  = {nullptr, nullptr, nullptr, nullptr};

    MqttBridge *m_mqtt = nullptr;
    DemoBridge *m_demo = nullptr;
    RtspSource *m_video = nullptr;
    bool m_demoMode = false;

    // 테마 전환 시 위젯을 다시 만들기 때문에(rebuildUi), 새 위젯이 다음 업데이트
    // 전까지 기본값(OFFLINE 등)으로 잠깐 보이지 않도록 마지막 값을 캐싱해둔다.
    DaemonState  m_lastDaemonState;
    ImuState     m_lastImu;
    ScanProgress m_lastProgress;
    ScanResult   m_lastResult;
    bool m_haveDaemonState = false, m_haveImu = false, m_haveProgress = false, m_haveResult = false;
};
