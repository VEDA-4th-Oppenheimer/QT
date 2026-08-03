#pragma once
#include <QWidget>
#include "Models.h"

class QLabel;
class QTableWidget;

// DEVICES / MQTT 탭: 상단 장비 카드 4개 + 토픽 테이블
class DevicesTab : public QWidget {
    Q_OBJECT
public:
    explicit DevicesTab(QWidget *parent = nullptr);

    void setImu(const ImuState &imu);
    void setCalib(const CalibState &c);
    void setChannelOnline(int channel, bool online);

private:
    QLabel *m_mpuDot, *m_mpuValue;
    QLabel *m_tofDot, *m_tofValue;
    QTableWidget *m_table;
    int m_ch4Row = -1;
};
