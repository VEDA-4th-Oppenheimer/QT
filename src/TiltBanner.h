#pragma once
#include <QFrame>
#include "Models.h"

class QLabel;

// IMU 수평 이탈 시 상단에 노출되는 재설치 안내 배너
class TiltBanner : public QFrame {
    Q_OBJECT
public:
    explicit TiltBanner(QWidget *parent = nullptr);
    void update(const ImuState &imu, double tolDeg = 1.5);

signals:
    void tiltOnset(const ImuState &imu);   // 수평(level) -> 이탈 로 처음 전환되는 순간

private:
    QLabel *m_detail;
    bool m_wasLevel = true;
};
