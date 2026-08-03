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
    for (auto *d : std::as_const(m_decoders)) d->stop();
    for (auto *d : std::as_const(m_decoders)) { d->wait(3000); delete d; }
}

void RtspSource::loadConfigAndStart(const QString &path) {
    QFile f(resolveConfigPath(path));
    if (!f.open(QIODevice::ReadOnly)) {
        emit logLine("RTSP", QString("%1 없음 — RTSP 비활성화 (기존 데모/라이브 채널 상태 유지)").arg(path));
        return;
    }
    const auto root = QJsonDocument::fromJson(f.readAll()).object();
    const auto channels = root.value("channels").toObject();
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
    if (m_decoders.isEmpty())
        emit logLine("RTSP", QString("%1 에 채널 설정 없음").arg(path));
}
