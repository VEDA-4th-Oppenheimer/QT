#pragma once
#include <QThread>
#include <QImage>
#include <QString>
#include <atomic>

struct AVFormatContext;
struct AVCodecContext;
struct SwsContext;

// 채널 1개의 RTSP 스트림을 백그라운드 스레드에서 디코딩한다.
// 연결이 끊기면 몇 번(kMaxConnectAttempts) 재연결을 시도하고, 그래도 안 붙으면
// gaveUp() 을 올린 뒤 스레드를 끝낸다 — 카메라가 없는 자리에서 무한히 재시도하며
// 로그를 채우지 않도록. 이후 재시도는 RtspSource::reconnectAll() 로 사용자가 시킨다.
class RtspDecoder : public QThread {
    Q_OBJECT
public:
    RtspDecoder(int channel, QString url, QObject *parent = nullptr);
    ~RtspDecoder() override;

    void stop();   // run() 루프를 정리하고 스레드를 종료시킨다 (wait() 는 호출자가 호출)

signals:
    void frameReady(int channel, const QImage &frame);
    void statusChanged(int channel, bool online, double fps);
    void logLine(const QString &tag, const QString &msg);
    void gaveUp(int channel);   // 재시도 예산 소진 — 이 스레드는 곧 끝난다

protected:
    void run() override;

private:
    static int interruptCallback(void *opaque);
    bool openStream();
    void closeStream();
    bool waitBeforeRetry(int attempt);

    int     m_channel;
    QString m_url;
    int     m_attempt = 0;      // 몇 번째 연결 시도인지(로그 표시용)
    std::atomic<bool> m_stop{false};
    qint64  m_deadlineMs = 0;   // 블로킹 호출(av_read_frame 등) 워치독

    AVFormatContext *m_fmt = nullptr;
    AVCodecContext  *m_codec = nullptr;
    SwsContext      *m_sws = nullptr;
    int m_videoStreamIndex = -1;
    int m_dstW = 0, m_dstH = 0;
};
