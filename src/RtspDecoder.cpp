#include "RtspDecoder.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

#include <QDateTime>

namespace {
constexpr int kReconnectDelayMs = 3000;
constexpr int kReadTimeoutMs = 8000;
constexpr int kMaxDecodeWidth = 960;   // 타일 표시용으로 다운스케일 (디코딩 자체는 원본 해상도로 수행)
}

RtspDecoder::RtspDecoder(int channel, QString url, QObject *parent)
    : QThread(parent), m_channel(channel), m_url(std::move(url)) {}

RtspDecoder::~RtspDecoder() {
    stop();
    wait(3000);
}

void RtspDecoder::stop() { m_stop.store(true); }

int RtspDecoder::interruptCallback(void *opaque) {
    auto *self = static_cast<RtspDecoder *>(opaque);
    return (self->m_stop.load() || QDateTime::currentMSecsSinceEpoch() > self->m_deadlineMs) ? 1 : 0;
}

bool RtspDecoder::openStream() {
    m_fmt = avformat_alloc_context();
    m_fmt->interrupt_callback.callback = &RtspDecoder::interruptCallback;
    m_fmt->interrupt_callback.opaque = this;
    m_deadlineMs = QDateTime::currentMSecsSinceEpoch() + kReadTimeoutMs;

    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);
    av_dict_set(&opts, "timeout", "8000000", 0);       // microseconds
    av_dict_set(&opts, "max_delay", "500000", 0);

    int ret = avformat_open_input(&m_fmt, m_url.toUtf8().constData(), nullptr, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        char buf[256];
        av_strerror(ret, buf, sizeof(buf));
        emit logLine("RTSP", QString("CH%1 연결 실패: %2").arg(m_channel).arg(buf));
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

    AVCodecParameters *params = m_fmt->streams[m_videoStreamIndex]->codecpar;
    const AVCodec *dec = avcodec_find_decoder(params->codec_id);
    if (!dec) {
        emit logLine("RTSP", QString("CH%1 디코더 없음 (codec_id=%2)").arg(m_channel).arg(params->codec_id));
        closeStream();
        return false;
    }
    m_codec = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(m_codec, params);
    if (avcodec_open2(m_codec, dec, nullptr) < 0) {
        emit logLine("RTSP", QString("CH%1 디코더 open 실패").arg(m_channel));
        closeStream();
        return false;
    }

    m_dstW = qMin(kMaxDecodeWidth, m_codec->width);
    m_dstH = m_codec->width > 0 ? m_dstW * m_codec->height / m_codec->width : m_codec->height;
    if (m_dstH < 1) m_dstH = 1;

    emit logLine("RTSP", QString("CH%1 연결됨 %2x%3 -> %4x%5")
                              .arg(m_channel).arg(m_codec->width).arg(m_codec->height).arg(m_dstW).arg(m_dstH));
    return true;
}

void RtspDecoder::closeStream() {
    if (m_sws) { sws_freeContext(m_sws); m_sws = nullptr; }
    if (m_codec) { avcodec_free_context(&m_codec); m_codec = nullptr; }
    if (m_fmt) { avformat_close_input(&m_fmt); m_fmt = nullptr; }
    m_videoStreamIndex = -1;
}

void RtspDecoder::run() {
    while (!m_stop.load()) {
        if (!openStream()) {
            emit statusChanged(m_channel, false, 0.0);
            for (int waited = 0; waited < kReconnectDelayMs && !m_stop.load(); waited += 100)
                QThread::msleep(100);
            continue;
        }
        emit statusChanged(m_channel, true, 0.0);

        AVPacket *pkt = av_packet_alloc();
        AVFrame  *frame = av_frame_alloc();

        int fpsCount = 0;
        qint64 fpsWindowStart = QDateTime::currentMSecsSinceEpoch();
        bool readError = false;

        while (!m_stop.load() && !readError) {
            m_deadlineMs = QDateTime::currentMSecsSinceEpoch() + kReadTimeoutMs;
            const int ret = av_read_frame(m_fmt, pkt);
            if (ret < 0) {
                if (!m_stop.load())
                    emit logLine("RTSP", QString("CH%1 스트림 끊김 — 재연결 시도").arg(m_channel));
                readError = true;
                break;
            }
            if (pkt->stream_index == m_videoStreamIndex && avcodec_send_packet(m_codec, pkt) == 0) {
                while (avcodec_receive_frame(m_codec, frame) == 0) {
                    if (!m_sws) {
                        m_sws = sws_getContext(frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
                                                m_dstW, m_dstH, AV_PIX_FMT_RGB24,
                                                SWS_BILINEAR, nullptr, nullptr, nullptr);
                    }
                    if (m_sws) {
                        QImage img(m_dstW, m_dstH, QImage::Format_RGB888);
                        uint8_t *dstData[1] = {img.bits()};
                        int dstStride[1] = {static_cast<int>(img.bytesPerLine())};
                        sws_scale(m_sws, frame->data, frame->linesize, 0, frame->height, dstData, dstStride);
                        emit frameReady(m_channel, img);
                        ++fpsCount;
                    }
                }
            }
            av_packet_unref(pkt);

            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (now - fpsWindowStart >= 1000) {
                emit statusChanged(m_channel, true, fpsCount * 1000.0 / double(now - fpsWindowStart));
                fpsCount = 0;
                fpsWindowStart = now;
            }
        }

        av_packet_free(&pkt);
        av_frame_free(&frame);
        closeStream();
        if (!m_stop.load()) emit statusChanged(m_channel, false, 0.0);
    }
}
