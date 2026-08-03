#pragma once
#include <QFrame>
#include "Models.h"

class QLabel;
class QPushButton;
class QTimer;

// 상단 바: MQTT / IMU / STATE 상태 칩 + 시계 + HOME / SCAN / STOP / DISARM.
// 버튼 활성화는 MQTT_INTERFACE_CONTRACT.md §5 상태-버튼 매핑을 따른다.
// DISARM 은 비상정지라 상태와 무관하게 항상 활성.
class TopBar : public QFrame {
    Q_OBJECT
public:
    explicit TopBar(QWidget *parent = nullptr);

    void setBrokerConnected(bool up);
    void setImu(const ImuState &imu);
    void setDaemonState(const DaemonState &s);

signals:
    void homeRequested();
    void scanRequested();
    void stopRequested();
    void disarmRequested();
    void rearmRequested();   // DISARM 상태에서 HOME 버튼이 REARM 으로 바뀌어 이 신호를 낸다

private:
    QLabel *m_mqttDot, *m_mqtt;
    QLabel *m_imu;
    QLabel *m_state;
    QLabel *m_clock;
    QPushButton *m_home, *m_scan, *m_stop, *m_disarm;
    QString m_lastState = "OFFLINE";
};
