#pragma once
#include <QFrame>
#include "Models.h"

class QLabel;
class QPushButton;
class QTimer;

// 상단 바: MQTT / IMU / CALIB 상태 칩 + 시계 + CALIBRATE / RE-SCAN / POWER
class TopBar : public QFrame {
    Q_OBJECT
public:
    explicit TopBar(QWidget *parent = nullptr);

    void setBrokerConnected(bool up);
    void setImu(const ImuState &imu);
    void setCalib(const CalibState &c);
    void setPower(bool on);

signals:
    void calibrateRequested();
    void rescanRequested();
    void powerToggled(bool on);

private:
    QLabel *m_mqttDot, *m_mqtt;
    QLabel *m_imu;
    QLabel *m_calib;
    QLabel *m_clock;
    QPushButton *m_power;
    bool m_powerOn = true;
};
