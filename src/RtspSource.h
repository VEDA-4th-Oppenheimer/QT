#pragma once
#include <QObject>
#include <QMap>
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
    // 브로커가 retained 로 내려주는 카메라 설정(adts/config/cameras)을 적용한다.
    // 카메라는 사용자별 자산이 아니라 킷의 일부라, 관리자가 한 곳에서 바꾸면
    // 접속 중인 콘솔 전부에 반영되는 편이 맞다. 등록 시점에 받은 파일은
    // "브로커가 아직 안 붙었을 때"를 위한 초기값으로 남는다.
    //
    // retained 라 재접속할 때마다 같은 내용이 다시 온다. 그때마다 스트림을
    // 끊고 다시 붙으면 화면이 깜빡이므로, 내용이 같으면 아무것도 하지 않는다.
    void applyChannels(const QJsonObject &channels, const QString &origin);

signals:
    void frameReceived(int channel, const QImage &frame);
    void channelStatusChanged(int channel, bool online, double fps);
    void logLine(const QString &tag, const QString &msg);

private:
    void stopAll();

    QMap<int, RtspDecoder *> m_decoders;
    QJsonObject              m_applied;   // 현재 돌고 있는 채널 설정(중복 적용 방지)
};
