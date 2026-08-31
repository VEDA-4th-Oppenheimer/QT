#pragma once
#include <QThread>
#include <QByteArray>
#include <QImage>
#include <QSet>
#include <QString>
#include <atomic>

#ifdef USE_FFMPEG
struct AVFormatContext;
struct AVCodecContext;
struct SwsContext;
struct AVStream;
#endif

#include <QMutex>

// 채널 1개의 RTSP 스트림을 백그라운드 스레드에서 디코딩한다.
class RtspDecoder : public QThread {
    Q_OBJECT
public:
    RtspDecoder(int channel, QString url, QObject *parent = nullptr);
    ~RtspDecoder() override;

    void stop();   // run() 루프를 정리하고 스레드를 종료시킨다
    void setFullResolution(bool full);
    bool isFullResolution() const { return m_fullResolution.load(); }
    void setUrl(const QString &newUrl);
    QString url() const;

signals:
    void frameReady(int channel, const QImage &frame);
    void metadataReady(int channel, const QByteArray &payload);
    void statusChanged(int channel, bool online, double fps);
    void logLine(const QString &tag, const QString &msg);
    void gaveUp(int channel);

protected:
    void run() override;

private:
#ifdef USE_FFMPEG
    static int interruptCallback(void *opaque);
    bool openStream();
    void closeStream();
    bool waitBeforeRetry(int attempt);

    AVFormatContext *m_fmt = nullptr;
    AVCodecContext  *m_codec = nullptr;
    SwsContext      *m_sws = nullptr;
    int m_videoStreamIndex = -1;
    QSet<int> m_metadataStreamIndices;
    int m_dstW = 0, m_dstH = 0;
    qint64  m_deadlineMs = 0;
#endif

    int     m_channel;
    QString m_url;
    mutable QMutex m_urlMutex;
    int     m_attempt = 0;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_fullResolution{false};
    std::atomic<bool> m_urlChanged{false};
};
