#pragma once
#include <QObject>
#include <QMap>
#include <QSet>
#include <QImage>
#include <QJsonObject>

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

public slots:
    // 카메라 설정을 적용한다(최초 로드, 그리고 '카메라 설정' 메뉴에서 변경 시).
    // 같은 내용이면 아무것도 하지 않는다 — 돌고 있는 스트림을 괜히 끊지 않으려고.
    // (단, 연결을 포기한 채널이 있으면 같은 설정이라도 다시 연다.)
    void applyChannels(const QJsonObject &channels, const QString &origin);

    // 마지막으로 적용된 설정 그대로 모든 채널을 다시 연다. 디코더는 몇 번 실패하면
    // 자동 재시도를 멈추므로, 카메라를 나중에 켠 경우 사용자가 이걸로 다시 붙인다.
    void reconnectAll();

signals:
    void frameReceived(int channel, const QImage &frame);
    void channelStatusChanged(int channel, bool online, double fps);
    void logLine(const QString &tag, const QString &msg);

private:
    void stopAll();

    QMap<int, RtspDecoder *> m_decoders;
    QJsonObject              m_applied;   // 현재 돌고 있는 채널 설정(중복 적용 방지)
    QSet<int>                m_gaveUp;    // 재시도를 포기해 스레드가 끝난 채널
};
