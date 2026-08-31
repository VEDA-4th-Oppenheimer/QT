#include "RtspSource.h"
#include "RtspDecoder.h"
#include "ConfigPath.h"
#include "CameraConfig.h"
#include "SpatialMetadata.h"
#include "SpatialProjector.h"

#ifdef USE_FFMPEG
extern "C" {
#include <libavutil/log.h>
}
#endif

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QTimer>
#include <utility>

namespace {
constexpr qsizetype kMaxMetadataBufferBytes = 1024 * 1024;
}

RtspSource::RtspSource(QObject *parent) : QObject(parent) {
#ifdef USE_FFMPEG
    av_log_set_level(AV_LOG_ERROR);
#endif
    m_flushTimer = new QTimer(this);
    connect(m_flushTimer, &QTimer::timeout, this, &RtspSource::onFlushTimer);
    m_flushTimer->start(100);
}

RtspSource::~RtspSource() {
    stopAll();
}

void RtspSource::stopAll() {
    if (m_flushTimer) m_flushTimer->stop();
    for (auto *d : std::as_const(m_decoders)) d->stop();
    for (auto *d : std::as_const(m_decoders)) { d->wait(3000); delete d; }
    m_decoders.clear();
    m_gaveUp.clear();
    m_metadataBuffers.clear();
    if (!m_cachedObjectsByChannel.isEmpty()) {
        m_cachedObjectsByChannel.clear();
        m_metadataLogSeen.clear();
        emit objectsUpdated({});
    }
}

void RtspSource::reconnectAll() {
    if (m_applied.isEmpty()) {
        emit logLine("RTSP", QString::fromUtf8("설정된 카메라 채널이 없습니다 — '카메라 설정' 에서 IP 를 먼저 입력하세요."));
        return;
    }
    const QJsonObject cfg = m_applied;
    m_applied = QJsonObject();
    applyChannels(cfg, QString::fromUtf8("CCTV 재연결"));
}

void RtspSource::setSoloChannel(int channel) {
    m_currentSoloChannel = channel;
    for (auto it = m_decoders.begin(); it != m_decoders.end(); ++it) {
        const int ch = it.key();
        auto *dec = it.value();
        if (dec == nullptr) continue;
        const bool solo = (channel > 0 && ch == channel);
        dec->setFullResolution(solo);

        const QString curUrl = dec->url();
        const QString targetUrl = CameraConfig::setUrlProfile(curUrl, solo ? 2 : 4);
        if (curUrl != targetUrl) {
            dec->setUrl(targetUrl);
            emit logLine("RTSP", QString("CH%1 프로파일 전환: %2 -> %3 (%4)")
                                     .arg(ch)
                                     .arg(solo ? QStringLiteral("profile4") : QStringLiteral("profile2"))
                                     .arg(solo ? QStringLiteral("profile2") : QStringLiteral("profile4"))
                                     .arg(solo ? QStringLiteral("1채널 확대 2592x1520 원본 모드") : QStringLiteral("2x2 분할 서브스트림 모드")));
        }
    }
}

void RtspSource::loadConfigAndStart(const QString &path) {
    SpatialProjector::instance().loadProfiles(resolveConfigPath("config/calibration_profiles.json"));

    QFile f(resolveConfigPath(path));
    if (!f.open(QIODevice::ReadOnly)) {
        const QString envUrl = qEnvironmentVariable("QT_RTSP_CH1_URL").trimmed();
        if (!envUrl.isEmpty()) {
            QJsonObject channels;
            channels.insert(QStringLiteral("1"), envUrl);
            applyChannels(channels, QStringLiteral("QT_RTSP_CH1_URL"));
            return;
        }
        emit logLine("RTSP", QString("%1 없음 — RTSP 비활성화 (기존 데모/라이브 채널 상태 유지)").arg(path));
        return;
    }
    const auto root = QJsonDocument::fromJson(f.readAll()).object();
    applyChannels(root.value("channels").toObject(), path);
}

void RtspSource::applyChannels(const QJsonObject &channels, const QString &origin) {
    if (channels.isEmpty()) {
        emit logLine("RTSP", QString("%1 에 채널 설정 없음").arg(origin));
        return;
    }
    if (channels == m_applied && m_gaveUp.isEmpty()) return;

    const bool restarting = !m_decoders.isEmpty();
    if (restarting) {
        emit logLine("RTSP", QString("카메라 설정 갱신(%1) — 스트림을 다시 엽니다").arg(origin));
        stopAll();
    }

    for (auto it = channels.begin(); it != channels.end(); ++it) {
        const int ch = it.key().toInt();
        const QString url = it.value().toString();
        if (ch < 1 || ch > 4 || url.isEmpty()) continue;

        auto *dec = new RtspDecoder(ch, url, this);
        connect(dec, &RtspDecoder::frameReady, this, &RtspSource::frameReceived);
        connect(dec, &RtspDecoder::metadataReady, this,
                [this](int channel, const QByteArray &payload) {
            ingestMetadataPayload(channel, payload);
        });
        connect(dec, &RtspDecoder::statusChanged, this, &RtspSource::channelStatusChanged);
        connect(dec, &RtspDecoder::logLine, this, &RtspSource::logLine);
        connect(dec, &RtspDecoder::gaveUp, this, [this](int c) { m_gaveUp.insert(c); });
        dec->setFullResolution(m_currentSoloChannel > 0 && ch == m_currentSoloChannel);
        m_decoders.insert(ch, dec);
        dec->start();
    }

    if (m_decoders.isEmpty()) {
        emit logLine("RTSP", QString("%1 에 쓸 수 있는 채널이 없음(1~4 범위 밖이거나 URL 비어 있음)").arg(origin));
        return;
    }
    m_applied = channels;
    emit logLine("RTSP", QString("채널 %1개 시작 (%2)").arg(m_decoders.size()).arg(origin));
}

void RtspSource::onFlushTimer() {
    flushExpired(QDateTime::currentMSecsSinceEpoch());
}

void RtspSource::emitMergedObjects() {
    QVector<SpatialObject> all;
    for (auto itCh = m_cachedObjectsByChannel.cbegin(); itCh != m_cachedObjectsByChannel.cend(); ++itCh) {
        for (auto itObj = itCh.value().cbegin(); itObj != itCh.value().cend(); ++itObj) {
            all.push_back(itObj.value().object);
        }
    }
    emit objectsUpdated(all);
}

void RtspSource::flushExpired(qint64 nowMs) {
    if (nowMs <= 0) nowMs = QDateTime::currentMSecsSinceEpoch();
    bool changed = false;

    for (auto itCh = m_cachedObjectsByChannel.begin(); itCh != m_cachedObjectsByChannel.end(); ++itCh) {
        auto &objMap = itCh.value();
        for (auto itObj = objMap.begin(); itObj != objMap.end();) {
            if (nowMs - itObj.value().lastSeenMs > m_cacheTtlMs) {
                itObj = objMap.erase(itObj);
                changed = true;
            } else {
                ++itObj;
            }
        }
    }

    if (changed) {
        emitMergedObjects();
    }
}

void RtspSource::ingestMetadataPayload(int channel, const QByteArray &payload) {
    if (channel < 1 || channel > 4 || payload.isEmpty()) return;

    QByteArray candidate = m_metadataBuffers.value(channel);
    candidate += payload;
    if (candidate.size() > kMaxMetadataBufferBytes) {
        m_metadataBuffers.remove(channel);
        candidate = payload;
    }

    spatial_metadata::ParseResult parsed = spatial_metadata::parse(payload, channel);
    if (!parsed.recognized && !parsed.objects.isEmpty()) {
        parsed.recognized = true;
    }

    if (!parsed.recognized && candidate != payload) {
        parsed = spatial_metadata::parse(candidate, channel);
    }

    if (!parsed.objects.isEmpty() || parsed.recognized) {
        m_metadataBuffers.remove(channel);

        // 빈 프레임(예: {} 또는 객체 0개)이 명시적으로 온 경우 해당 채널 캐시 즉시 비움
        if (parsed.objects.isEmpty()) {
            if (!m_cachedObjectsByChannel[channel].isEmpty()) {
                m_cachedObjectsByChannel[channel].clear();
                emitMergedObjects();
            }
            return;
        }

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        int newAdded = 0;
        for (const SpatialObject &obj : parsed.objects) {
            QString key = obj.id.trimmed();
            if (key.isEmpty()) {
                key = QStringLiteral("%1_%2_%3").arg(channel)
                          .arg(obj.posM.x(), 0, 'f', 2)
                          .arg(obj.posM.y(), 0, 'f', 2);
            }
            m_cachedObjectsByChannel[channel][key] = { obj, now };
            ++newAdded;
        }

        // 500ms 만료된 오래된 객체 정리 후 전체 병합 목록 전송
        flushExpired(now);
        emitMergedObjects();

        if (!m_metadataLogSeen.contains(channel)) {
            m_metadataLogSeen.insert(channel);
            int topViewCount = 0;
            for (const auto &item : m_cachedObjectsByChannel[channel]) {
                if (item.object.hasTopViewPosition()) ++topViewCount;
            }
            emit logLine("OBJECT", QString("CH%1 RTSP metadata 수신 — %2개 객체 (활성 캐시 %3개, Top-View %4개)")
                                         .arg(channel)
                                         .arg(newAdded)
                                         .arg(m_cachedObjectsByChannel[channel].size())
                                         .arg(topViewCount));
        }
    } else {
        m_metadataBuffers.insert(channel, candidate);
    }
}
