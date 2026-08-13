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
    m_gaveUp.clear();
}

void RtspSource::reconnectAll() {
    if (m_applied.isEmpty()) {
        emit logLine("RTSP", QString::fromUtf8("설정된 카메라 채널이 없습니다 — '카메라 설정' 에서 IP 를 먼저 입력하세요."));
        return;
    }
    // 돌고 있는 스트림까지 포함해 통째로 다시 연다(사용자가 명시적으로 시킨 재연결).
    const QJsonObject cfg = m_applied;
    m_applied = QJsonObject();
    applyChannels(cfg, QString::fromUtf8("CCTV 재연결"));
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
    // 다시 붙이면 화면만 깜빡이므로 그냥 무시한다. 다만 연결을 포기한 채널이
    // 있으면 같은 설정이라도 다시 연다 — 사용자가 '카메라 설정' 에서 값을 그대로
    // 두고 확인만 눌렀을 때 재시도가 되게.
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
        connect(dec, &RtspDecoder::statusChanged, this, &RtspSource::channelStatusChanged);
        connect(dec, &RtspDecoder::logLine, this, &RtspSource::logLine);
        connect(dec, &RtspDecoder::gaveUp, this, [this](int c) { m_gaveUp.insert(c); });
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
