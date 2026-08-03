#pragma once
#include <QMainWindow>
#include "Models.h"

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
class QTabWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    QWidget *buildDashboardTab();
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
    bool m_demoMode = true;
};
