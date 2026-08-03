#pragma once
#include <QObject>
#include <QImage>
#include <QVector>
#include "Models.h"

// MQTT 브로커(MqttBridge)와 데모 시뮬레이터(DemoBridge)가 공유하는 신호 인터페이스.
// MainWindow 및 각 탭은 이 시그널 표면만 알면 되고, 실제로 어느 쪽이 연결되어 있는지는
// 신경 쓰지 않는다.
class DataBridge : public QObject {
    Q_OBJECT
public:
    explicit DataBridge(QObject *parent = nullptr) : QObject(parent) {}
    ~DataBridge() override = default;

    virtual void start() = 0;   // 데이터 발생 시작(데모: 타이머 시작 / 라이브: 아무 것도 안 함)
    virtual void stop() = 0;    // 데이터 발생 중단

public slots:
    virtual void setKitPower(bool on) = 0;
    virtual void requestRescan() = 0;

signals:
    void brokerStateChanged(bool connected);
    void frameReceived(int channel, const QImage &frame);
    void channelStatusChanged(int channel, bool online, double fps);
    void imuUpdated(const ImuState &imu);
    void objectsUpdated(const QVector<SpatialObject> &objects);
    void mapEdgesUpdated(const QVector<MapEdge> &edges);
    void calibUpdated(const CalibState &calib);
    void logLine(const QString &tag, const QString &msg);
};
