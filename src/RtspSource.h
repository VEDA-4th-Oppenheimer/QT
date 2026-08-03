#pragma once
#include <QObject>
#include <QMap>
#include <QImage>

class RtspDecoder;

// config/cameras.json 에 채널별 RTSP URL이 설정되어 있으면 실제 카메라 영상을 디코딩해
// CameraTile 에 공급한다. 설정이 없는 채널은 건드리지 않으므로(Demo/Live 브리지의
// 채널 상태가 그대로 유지된다) RTSP 없이도 기존 데모 경험이 그대로 동작한다.
class RtspSource : public QObject {
    Q_OBJECT
public:
    explicit RtspSource(QObject *parent = nullptr);
    ~RtspSource() override;

    void loadConfigAndStart(const QString &path = QStringLiteral("config/cameras.json"));

signals:
    void frameReceived(int channel, const QImage &frame);
    void channelStatusChanged(int channel, bool online, double fps);
    void logLine(const QString &tag, const QString &msg);

private:
    QMap<int, RtspDecoder *> m_decoders;
};
