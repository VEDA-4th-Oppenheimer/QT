#include "RtspDecoder.h"

#ifdef USE_FFMPEG
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/dict.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
}
#endif

#include <QDateTime>
#include <QElapsedTimer>

namespace {
constexpr int kReconnectDelayMs = 3000;
constexpr int kReadTimeoutMs = 8000;
constexpr int kMaxDecodeWidth = 960;
constexpr int kMaxConnectAttempts = 3;

#ifdef USE_FFMPEG
bool looksLikeMetadataStream(const AVStream *stream) {
    if (stream == nullptr || stream->codecpar == nullptr) return false;
    const auto type = stream->codecpar->codec_type;
    if (type != AVMEDIA_TYPE_VIDEO && type != AVMEDIA_TYPE_AUDIO) {
        return true;
    }

    AVDictionaryEntry *entry = nullptr;
    while ((entry = av_dict_get(stream->metadata, "", entry, AV_DICT_IGNORE_SUFFIX)) != nullptr) {
        const QString key = QString::fromUtf8(entry->key).toLower();
        const QString value = QString::fromUtf8(entry->value).toLower();
        if (key.contains("metadata") || key.contains("mimetype") || key.contains("handler")
            || value.contains("metadata") || value.contains("onvif")
            || value.contains("application/xml") || value.contains("application/json")) {
            return true;
        }
    }
    return false;
}

QString streamTypeName(const AVStream *stream) {
    if (stream == nullptr || stream->codecpar == nullptr) return QStringLiteral("unknown");
    switch (stream->codecpar->codec_type) {
    case AVMEDIA_TYPE_DATA: return QStringLiteral("data");
    case AVMEDIA_TYPE_SUBTITLE: return QStringLiteral("subtitle");
    case AVMEDIA_TYPE_ATTACHMENT: return QStringLiteral("attachment");
    default: return QStringLiteral("sub-stream");
    }
}
#endif
}

RtspDecoder::RtspDecoder(int channel, QString url, QObject *parent)
    : QThread(parent), m_channel(channel), m_url(std::move(url)) {}

RtspDecoder::~RtspDecoder() {
    stop();
    wait(3000);
}

void RtspDecoder::setFullResolution(bool full) {
    m_fullResolution.store(full);
}

void RtspDecoder::setUrl(const QString &newUrl) {
    QMutexLocker locker(&m_urlMutex);
    if (m_url == newUrl) return;
    m_url = newUrl;
    m_urlChanged.store(true);
}

QString RtspDecoder::url() const {
    QMutexLocker locker(&m_urlMutex);
    return m_url;
}

void RtspDecoder::setHwAccel(const QString &accel) {
    QMutexLocker locker(&m_hwMutex);
    if (m_hwAccelName == accel) return;
    m_hwAccelName = accel;
    m_urlChanged.store(true);
}

QString RtspDecoder::hwAccel() const {
    QMutexLocker locker(&m_hwMutex);
    return m_hwAccelName;
}

void RtspDecoder::stop() { m_stop.store(true); }

#ifdef USE_FFMPEG
int RtspDecoder::interruptCallback(void *opaque) {
    auto *self = static_cast<RtspDecoder *>(opaque);
    return (self->m_stop.load() || self->m_urlChanged.load() || QDateTime::currentMSecsSinceEpoch() > self->m_deadlineMs) ? 1 : 0;
}

int RtspDecoder::getHwFormatCallback(AVCodecContext *ctx, const int *pix_fmts) {
    auto *self = static_cast<RtspDecoder *>(ctx->opaque);
    if (!self) return AV_PIX_FMT_NONE;
    for (const int *p = pix_fmts; *p != -1; p++) {
        if (*p == self->m_hwPixFmt)
            return *p;
    }
    return AV_PIX_FMT_NONE;
}

bool RtspDecoder::openStream() {
    QString targetUrl;
    {
        QMutexLocker locker(&m_urlMutex);
        targetUrl = m_url;
    }
    QString accel;
    {
        QMutexLocker locker(&m_hwMutex);
        accel = m_hwAccelName;
    }

    m_fmt = avformat_alloc_context();
    m_fmt->interrupt_callback.callback = &RtspDecoder::interruptCallback;
    m_fmt->interrupt_callback.opaque = this;
    m_fmt->flags |= AVFMT_FLAG_NOBUFFER;
    m_fmt->flags |= AVFMT_FLAG_FAST_SEEK;
    m_deadlineMs = QDateTime::currentMSecsSinceEpoch() + kReadTimeoutMs;

    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);
    av_dict_set(&opts, "fflags", "nobuffer", 0);
    av_dict_set(&opts, "flags", "low_delay", 0);
    av_dict_set(&opts, "probesize", "65536", 0);       // 기본 5MB -> 64KB (초고속 스트림 프로빙)
    av_dict_set(&opts, "analyzeduration", "100000", 0); // 기본 5초 -> 100ms (0.1초 분석)
    av_dict_set(&opts, "max_delay", "100000", 0);       // 100ms
    av_dict_set(&opts, "timeout", "5000000", 0);

    int ret = avformat_open_input(&m_fmt, targetUrl.toUtf8().constData(), nullptr, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        char buf[256];
        av_strerror(ret, buf, sizeof(buf));
        emit logLine("RTSP", QString("CH%1 연결 실패(%2/%3): %4")
                                 .arg(m_channel).arg(m_attempt).arg(kMaxConnectAttempts).arg(buf));
        m_fmt = nullptr;
        return false;
    }

    m_deadlineMs = QDateTime::currentMSecsSinceEpoch() + kReadTimeoutMs;
    if (avformat_find_stream_info(m_fmt, nullptr) < 0) {
        emit logLine("RTSP", QString("CH%1 스트림 정보를 읽을 수 없음").arg(m_channel));
        closeStream();
        return false;
    }

    m_videoStreamIndex = av_find_best_stream(m_fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_videoStreamIndex < 0) {
        emit logLine("RTSP", QString("CH%1 비디오 스트림 없음").arg(m_channel));
        closeStream();
        return false;
    }

    m_metadataStreamIndices.clear();
    for (unsigned i = 0; i < m_fmt->nb_streams; ++i) {
        if (static_cast<int>(i) != m_videoStreamIndex) {
            m_metadataStreamIndices.insert(static_cast<int>(i));
            emit logLine("RTSP", QString("CH%1 metadata stream 등록 (index=%2, type=%3)")
                                     .arg(m_channel).arg(static_cast<int>(i))
                                     .arg(streamTypeName(m_fmt->streams[i])));
        }
    }
    if (m_metadataStreamIndices.isEmpty()) {
        emit logLine("RTSP", QString("CH%1 metadata stream 없음 — 비디오 인밴드 검사 활성화").arg(m_channel));
    }

    AVCodecParameters *params = m_fmt->streams[m_videoStreamIndex]->codecpar;
    const AVCodec *dec = avcodec_find_decoder(params->codec_id);
    if (!dec) {
        emit logLine("RTSP", QString("CH%1 디코더 없음 (codec_id=%2)").arg(m_channel).arg(params->codec_id));
        closeStream();
        return false;
    }
    m_codec = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(m_codec, params);
    m_codec->flags |= AV_CODEC_FLAG_LOW_DELAY;
#ifdef AV_CODEC_FLAG2_FAST
    m_codec->flags2 |= AV_CODEC_FLAG2_FAST;
#endif

    // GPU 하드웨어 가속 설정 (d3d11va, dxva2 등)
    m_hwPixFmt = -1;
    if (accel != "none" && !accel.trimmed().isEmpty()) {
        enum AVHWDeviceType hwType = av_hwdevice_find_type_by_name(accel.toUtf8().constData());
        if (hwType != AV_HWDEVICE_TYPE_NONE) {
            for (int i = 0;; i++) {
                const AVCodecHWConfig *config = avcodec_get_hw_config(dec, i);
                if (!config) break;
                if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                    config->device_type == hwType) {
                    m_hwPixFmt = static_cast<int>(config->pix_fmt);
                    break;
                }
            }

            if (m_hwPixFmt != -1) {
                if (av_hwdevice_ctx_create(&m_hwDeviceCtx, hwType, nullptr, nullptr, 0) >= 0) {
                    m_codec->hw_device_ctx = av_buffer_ref(m_hwDeviceCtx);
                    m_codec->opaque = this;
                    m_codec->get_format = reinterpret_cast<enum AVPixelFormat (*)(AVCodecContext *, const enum AVPixelFormat *)>(&RtspDecoder::getHwFormatCallback);
                    emit logLine("RTSP", QString("CH%1 GPU 하드웨어 가속(%2) 활성화").arg(m_channel).arg(accel));
                }
            }
        }
    }

    if (avcodec_open2(m_codec, dec, nullptr) < 0) {
        emit logLine("RTSP", QString("CH%1 디코더 open 실패").arg(m_channel));
        closeStream();
        return false;
    }

    const bool initFull = m_fullResolution.load();
    m_dstW = initFull ? m_codec->width : qMin(kMaxDecodeWidth, m_codec->width);
    m_dstH = initFull ? m_codec->height : (m_codec->width > 0 ? m_dstW * m_codec->height / m_codec->width : m_codec->height);
    m_sws = sws_getContext(m_codec->width, m_codec->height, m_codec->pix_fmt,
                           m_dstW, m_dstH, AV_PIX_FMT_RGB24,
                           SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_sws) {
        emit logLine("RTSP", QString("CH%1 스케일러 생성 실패").arg(m_channel));
        closeStream();
        return false;
    }

    emit logLine("RTSP", QString("CH%1 연결 성공 — %2 (%3x%4 @ %5x%6, 가속=%7)")
                             .arg(m_channel).arg(dec->name).arg(m_codec->width).arg(m_codec->height)
                             .arg(m_dstW).arg(m_dstH).arg(m_hwDeviceCtx ? accel : QStringLiteral("CPU")));
    return true;
}

void RtspDecoder::closeStream() {
    if (m_sws)         { sws_freeContext(m_sws);         m_sws = nullptr; }
    if (m_codec)       { avcodec_free_context(&m_codec);   m_codec = nullptr; }
    if (m_fmt)         { avformat_close_input(&m_fmt);    m_fmt = nullptr; }
    if (m_hwDeviceCtx) { av_buffer_unref(&m_hwDeviceCtx); m_hwDeviceCtx = nullptr; }
    if (m_swFrame)     { av_frame_free(&m_swFrame);       m_swFrame = nullptr; }
    m_hwPixFmt = -1;
    m_videoStreamIndex = -1;
    m_metadataStreamIndices.clear();
}

bool RtspDecoder::waitBeforeRetry(int attempt) {
    const int delayMs = kReconnectDelayMs * attempt;
    emit logLine("RTSP", QString("CH%1 %2초 후 재시도 (%3/%4)…")
                             .arg(m_channel).arg(delayMs / 1000).arg(attempt).arg(kMaxConnectAttempts));
    const qint64 until = QDateTime::currentMSecsSinceEpoch() + delayMs;
    while (!m_stop.load() && QDateTime::currentMSecsSinceEpoch() < until) {
        msleep(100);
    }
    return !m_stop.load();
}
#endif

void RtspDecoder::run() {
#ifdef USE_FFMPEG
    m_attempt = 0;
    while (!m_stop.load()) {
        ++m_attempt;
        emit statusChanged(m_channel, false, 0.0);
        if (!openStream()) {
            if (m_attempt >= kMaxConnectAttempts) {
                emit logLine("RTSP", QString("CH%1 연결 재시도 %2회 실패 — 자동 재시도를 멈춥니다. "
                                             "'CCTV 재연결' 로 다시 시도하세요.").arg(m_channel).arg(kMaxConnectAttempts));
                emit gaveUp(m_channel);
                return;
            }
            if (!waitBeforeRetry(m_attempt)) return;
            continue;
        }

        emit statusChanged(m_channel, true, 0.0);
        m_attempt = 0;

        AVPacket *pkt = av_packet_alloc();
        AVFrame  *frame = av_frame_alloc();
        AVFrame  *rgbFrame = av_frame_alloc();
        const int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, m_dstW, m_dstH, 1);
        auto *buffer = static_cast<uint8_t *>(av_malloc(numBytes));
        av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, buffer,
                             AV_PIX_FMT_RGB24, m_dstW, m_dstH, 1);

        int frameCount = 0;
        qint64 fpsTimer = QDateTime::currentMSecsSinceEpoch();
        QElapsedTimer decodeTimer;
        QElapsedTimer scaleTimer;
        double totalDecodeMs = 0.0;
        double totalScaleMs = 0.0;
        int latencySampleCount = 0;

        while (!m_stop.load()) {
            if (m_urlChanged.load()) {
                m_urlChanged.store(false);
                emit logLine("RTSP", QString("CH%1 프로파일 변경 요청 수신 — 새 스트림으로 즉시 전환합니다").arg(m_channel));
                break;
            }
            m_deadlineMs = QDateTime::currentMSecsSinceEpoch() + kReadTimeoutMs;
            int ret = av_read_frame(m_fmt, pkt);
            if (ret < 0) {
                if (m_urlChanged.load()) {
                    m_urlChanged.store(false);
                    emit logLine("RTSP", QString("CH%1 프로파일 변경 요청 수신 — 새 스트림으로 즉시 전환합니다").arg(m_channel));
                } else {
                    emit logLine("RTSP", QString("CH%1 스트림 끊김 (read error)").arg(m_channel));
                }
                break;
            }

            // 1) 비-비디오 스트림(메타데이터 트랙) 패킷 처리
            if (pkt->stream_index != m_videoStreamIndex) {
                if (pkt->data && pkt->size > 0) {
                    const QByteArray payload(reinterpret_cast<const char *>(pkt->data), pkt->size);
                    emit metadataReady(m_channel, payload);
                }
            }

            // 2) 비디오 스트림 패킷 디코딩 및 인밴드 메타데이터 검사
            if (pkt->stream_index == m_videoStreamIndex) {
                if (pkt->data && pkt->size > 20) {
                    const QByteArray raw(reinterpret_cast<const char *>(pkt->data), pkt->size);
                    if (raw.contains("BoundingBox") || raw.contains("boundingbox") || raw.contains("ObjectId")) {
                        emit metadataReady(m_channel, raw);
                    }
                }

                decodeTimer.start();
                ret = avcodec_send_packet(m_codec, pkt);
                if (ret >= 0) {
                    while (avcodec_receive_frame(m_codec, frame) >= 0) {
                        const double decodeMs = decodeTimer.nsecsElapsed() / 1.0e6;

                        AVFrame *displayFrame = frame;
                        if (frame->format == m_hwPixFmt) {
                            if (!m_swFrame) m_swFrame = av_frame_alloc();
                            if (av_hwframe_transfer_data(m_swFrame, frame, 0) < 0) {
                                continue;
                            }
                            displayFrame = m_swFrame;
                        }

                        // 해상도 모드(1채널 확대 시 원본 vs 4분할 시 다운샘플) 동적 전환 검사
                        const bool reqFull = m_fullResolution.load();
                        const int targetW = reqFull ? m_codec->width : qMin(kMaxDecodeWidth, m_codec->width);
                        const int targetH = reqFull ? m_codec->height : (m_codec->width > 0 ? targetW * m_codec->height / m_codec->width : m_codec->height);

                        if (m_dstW != targetW || m_dstH != targetH || !m_sws) {
                            if (m_sws) { sws_freeContext(m_sws); m_sws = nullptr; }
                            m_dstW = targetW;
                            m_dstH = targetH;
                            const auto pixFmt = static_cast<enum AVPixelFormat>(displayFrame->format);
                            m_sws = sws_getContext(m_codec->width, m_codec->height, pixFmt,
                                                   m_dstW, m_dstH, AV_PIX_FMT_RGB24,
                                                   SWS_BILINEAR, nullptr, nullptr, nullptr);

                            av_free(buffer);
                            const int newBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, m_dstW, m_dstH, 1);
                            buffer = static_cast<uint8_t *>(av_malloc(newBytes));
                            av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, buffer,
                                                 AV_PIX_FMT_RGB24, m_dstW, m_dstH, 1);

                            emit logLine("RTSP", QString("CH%1 해상도 전환 — %2x%3 (%4)")
                                                     .arg(m_channel).arg(m_dstW).arg(m_dstH)
                                                     .arg(reqFull ? QStringLiteral("원본 해상도") : QStringLiteral("다운샘플링")));
                        }

                        scaleTimer.start();
                        sws_scale(m_sws, displayFrame->data, displayFrame->linesize, 0, m_codec->height,
                                  rgbFrame->data, rgbFrame->linesize);
                        const double scaleMs = scaleTimer.nsecsElapsed() / 1.0e6;

                        totalDecodeMs += decodeMs;
                        totalScaleMs += scaleMs;
                        ++latencySampleCount;

                        QImage img(rgbFrame->data[0], m_dstW, m_dstH, rgbFrame->linesize[0], QImage::Format_RGB888);
                        emit frameReady(m_channel, img.copy());

                        ++frameCount;
                        const qint64 now = QDateTime::currentMSecsSinceEpoch();
                        if (now - fpsTimer >= 1000) {
                            const double fps = frameCount * 1000.0 / (now - fpsTimer);
                            const double avgDecodeMs = (latencySampleCount > 0) ? (totalDecodeMs / latencySampleCount) : 0.0;
                            const double avgScaleMs = (latencySampleCount > 0) ? (totalScaleMs / latencySampleCount) : 0.0;
                            const double avgTotalMs = avgDecodeMs + avgScaleMs;

                            emit statusChanged(m_channel, true, fps);
                            emit logLine("LATENCY", QString("CH%1 | 디코딩: %2ms, 스케일링: %3ms, 총처리: %4ms (%5x%6 @ %7fps)")
                                                        .arg(m_channel)
                                                        .arg(avgDecodeMs, 0, 'f', 2)
                                                        .arg(avgScaleMs, 0, 'f', 2)
                                                        .arg(avgTotalMs, 0, 'f', 2)
                                                        .arg(m_dstW).arg(m_dstH)
                                                        .arg(fps, 0, 'f', 1));

                            frameCount = 0;
                            totalDecodeMs = 0.0;
                            totalScaleMs = 0.0;
                            latencySampleCount = 0;
                            fpsTimer = now;
                        }
                    }
                }
            }
            av_packet_unref(pkt);
        }

        av_free(buffer);
        av_frame_free(&rgbFrame);
        av_frame_free(&frame);
        av_packet_free(&pkt);
        closeStream();

        if (m_stop.load()) break;
        if (!waitBeforeRetry(1)) return;
    }
    emit statusChanged(m_channel, false, 0.0);
#else
    emit logLine("RTSP", QString("FFmpeg 미내장 — CH%1 RTSP 비활성 (데모/정적 모드 동작)").arg(m_channel));
    emit statusChanged(m_channel, false, 0.0);
#endif
}
