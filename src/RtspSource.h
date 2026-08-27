#pragma once
#include <QByteArray>
#include <QObject>
#include <QMap>
#include <QSet>
#include <QImage>
#include <QJsonObject>
#include <QVector>
#include "Models.h"

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

    // RTSP decoder 외의 transport/replay adapter가 확보한 metadata frame도 같은
    // 채널 병합 경로로 넣는다. 실제 decoder는 이 경계를 사용하고, offline fixture는
    // 카메라 없이 decoder 이후의 source 동작을 검증할 때 사용한다.
    void ingestMetadataPayload(int channel, const QByteArray &payload);

    // TTL 만료 검사 및 플러시 (테스트 및 주기적 호출용)
    void flushExpired(qint64 nowMs = 0);

    // 캐시 수명(기본 500ms) 설정 및 조회
    static constexpr qint64 kDefaultTtlMs = 500;
    void setCacheTtlMs(qint64 ttlMs) { m_cacheTtlMs = ttlMs; }
    qint64 cacheTtlMs() const { return m_cacheTtlMs; }

public slots:
    // 카메라 설정을 적용한다(최초 로드, 그리고 '카메라 설정' 메뉴에서 변경 시).
    // 같은 내용이면 아무것도 하지 않는다 — 돌고 있는 스트림을 괜히 끊지 않으려고.
    // (단, 연결을 포기한 채널이 있으면 같은 설정이라도 다시 연다.)
    void applyChannels(const QJsonObject &channels, const QString &origin);

    // 마지막으로 적용된 설정 그대로 모든 채널을 다시 연다. 디코더는 몇 번 실패하면
    // 자동 재시도를 멈추므로, 카메라를 나중에 켠 경우 사용자가 이걸로 다시 붙인다.
    void reconnectAll();

    // 1채널 확대(solo > 0) 시 해당 채널 디코더를 원본 해상도로 전환하고, 4분할(0) 시 다운샘플링 복귀
    void setSoloChannel(int channel);

signals:
    void frameReceived(int channel, const QImage &frame);
    void objectsUpdated(const QVector<SpatialObject> &objects);
    void channelStatusChanged(int channel, bool online, double fps);
    void logLine(const QString &tag, const QString &msg);

private slots:
    void onFlushTimer();

private:
    void stopAll();
    void emitMergedObjects();

    struct CachedObject {
        SpatialObject object;
        qint64 lastSeenMs = 0;
    };

    QMap<int, RtspDecoder *> m_decoders;
    QMap<int, QMap<QString, CachedObject>> m_cachedObjectsByChannel;
    QMap<int, QByteArray> m_metadataBuffers;
    QSet<int> m_metadataLogSeen;
    QJsonObject              m_applied;   // 현재 돌고 있는 채널 설정(중복 적용 방지)
    QSet<int>                m_gaveUp;    // 재시도를 포기해 스레드가 끝난 채널
    QTimer                  *m_flushTimer = nullptr;
    qint64                   m_cacheTtlMs = kDefaultTtlMs;
    int                      m_currentSoloChannel = 0;
};
