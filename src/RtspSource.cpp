#include "RtspSource.h"
#include "RtspDecoder.h"
#include "ConfigPath.h"

extern "C" {
#include <libavutil/log.h>
}

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <utility>

RtspSource::RtspSource(QObject *parent) : QObject(parent) {
    av_log_set_level(AV_LOG_ERROR);   // FFmpeg 내부 경고 로그로 콘솔이 시끄러워지지 않도록
}

RtspSource::~RtspSource() {
    stopAll();
}

void RtspSource::stopAll() {
    for (auto *d : std::as_const(m_decoders)) d->stop();
    for (auto *d : std::as_const(m_decoders)) { d->wait(3000); delete d; }
    m_decoders.clear();
}

void RtspSource::loadConfigAndStart(const QString &path) {
    QFile f(resolveConfigPath(path));
    if (!f.open(QIODevice::ReadOnly)) {
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
    // retained 메시지는 재접속할 때마다 다시 온다. 같은 내용에 스트림을 끊고
    // 다시 붙이면 화면만 깜빡이므로 그냥 무시한다.
    if (channels == m_applied) return;

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
        connect(dec, &RtspDecoder::statusChanged, this, &RtspSource::channelStatusChanged);
        connect(dec, &RtspDecoder::logLine, this, &RtspSource::logLine);
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
