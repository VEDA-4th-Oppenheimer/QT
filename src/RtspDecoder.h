#pragma once
#include <QThread>
#include <QImage>
#include <QString>
#include <atomic>

struct AVFormatContext;
struct AVCodecContext;
struct SwsContext;

// 채널 1개의 RTSP 스트림을 백그라운드 스레드에서 디코딩한다.
// 연결이 끊기면 자동으로 재연결을 시도하고, 새 프레임/온라인 상태를 시그널로 올린다.
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

protected:
    void run() override;

private:
    static int interruptCallback(void *opaque);
    bool openStream();
    void closeStream();

    int     m_channel;
    QString m_url;
    std::atomic<bool> m_stop{false};
    qint64  m_deadlineMs = 0;   // 블로킹 호출(av_read_frame 등) 워치독

    AVFormatContext *m_fmt = nullptr;
    AVCodecContext  *m_codec = nullptr;
    SwsContext      *m_sws = nullptr;
    int m_videoStreamIndex = -1;
    int m_dstW = 0, m_dstH = 0;
};
