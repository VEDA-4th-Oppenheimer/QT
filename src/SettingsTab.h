#pragma once
#include <QWidget>
#include "Theme.h"

class QLabel;
class QPushButton;
class QCheckBox;

// SETTINGS 탭: 모드/테마 메뉴에 흩어져 있던 항목을 성격별로 묶어 한 화면에 놓는다.
// 메뉴는 항목이 늘수록 찾기 어렵고 현재 값(센서 높이·카메라 IP 등)을 보여줄 수
// 없어서, 값과 조작을 같이 두는 탭으로 옮겼다.
//
// 이 위젯은 상태를 갖지 않는다. 테마 전환 때 MainWindow::rebuildUi 가 중앙
// 위젯을 통째로 다시 만들기 때문에, 생성 시점의 값을 받아 표시만 하고 조작은
// 전부 시그널로 넘긴다.
class SettingsTab : public QWidget {
    Q_OBJECT
public:
    struct State {
        Theme::Mode theme     = Theme::Mode::Developer;
        bool        demoMode  = false;
        int         sensorHeightMm = 2400;
        QString     cameraHost;       // 비어 있으면 "미설정" 으로 표시
        int         cameraChannels = 0;
        bool        topViewDetached = false;
    };

    explicit SettingsTab(const State &s, QWidget *parent = nullptr);

    // 값이 바뀌었을 때 탭을 다시 만들지 않고 표시만 갱신한다.
    void setSensorHeight(int mm);
    void setCameraSummary(const QString &host, int channels);
    void setTopViewDetached(bool detached);
    void setDemoMode(bool demo);

signals:
    void themeChangeRequested(Theme::Mode mode);
    void demoModeToggled(bool demo);
    void cameraSettingsRequested();
    void cameraReconnectRequested();
    void topViewFullScreenToggled();
    void sensorHeightRequested();
    void openScanFileRequested();
    void logoutRequested();

private:
    void refreshThemeButtons();
    void refreshTopViewButton();

    Theme::Mode  m_theme;
    QPushButton *m_devTheme  = nullptr;
    QPushButton *m_userTheme = nullptr;
    QPushButton *m_topView   = nullptr;
    QCheckBox   *m_demo      = nullptr;
    QLabel      *m_heightValue = nullptr;
    QLabel      *m_cameraValue = nullptr;
    bool m_topViewDetached = false;
};
