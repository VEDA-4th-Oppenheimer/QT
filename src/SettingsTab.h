#pragma once

#include <QWidget>

#include "Theme.h"

class QLabel;
class QCheckBox;
class QPushButton;

// MainWindow가 최신 커밋에서 연결하는 설정 탭의 최소 구현.
// 설정 값의 실제 저장/적용은 MainWindow가 소유하고, 이 위젯은 사용자 입력을
// 신호로만 전달해 테마 전환 때도 상태를 잃지 않는다.
class SettingsTab : public QWidget {
    Q_OBJECT
public:
    struct State {
        Theme::Mode theme = Theme::Mode::Developer;
        bool demoMode = false;
        int sensorHeightMm = 1789;
        bool topViewDetached = false;
        QString cameraHost;
        int cameraChannels = 0;
        bool manualCalib = false;
    };

    explicit SettingsTab(const State &state, QWidget *parent = nullptr);

    void setDemoMode(bool demo);
    void setTopViewDetached(bool detached);
    void setSensorHeight(int mm);
    void setCameraSummary(const QString &host, int channels);
    void setCalibMode(bool manual);

signals:
    void themeChangeRequested(Theme::Mode mode);
    void demoModeToggled(bool enabled);
    void calibModeToggled(bool manual);
    void cameraSettingsRequested();
    void cameraReconnectRequested();
    void topViewFullScreenToggled();
    void sensorHeightRequested();
    void openScanFileRequested();
    void loadCalibResultRequested();
    void logoutRequested();

private:
    QCheckBox *m_demo = nullptr;
    QCheckBox *m_manualCalib = nullptr;
    QLabel *m_theme = nullptr;
    QLabel *m_camera = nullptr;
    QLabel *m_height = nullptr;
    QLabel *m_topView = nullptr;
    QLabel *m_calibLabel = nullptr;
};
