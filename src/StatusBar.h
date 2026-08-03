#pragma once
#include <QFrame>

class QLabel;

// 하단 상태 바 (26px): KIT POWER · STM32 UART · RPi4B 온도 · 빌드 정보
class StatusBar : public QFrame {
    Q_OBJECT
public:
    explicit StatusBar(QWidget *parent = nullptr);
    void setPower(bool on);

private:
    QLabel *m_power;
};
