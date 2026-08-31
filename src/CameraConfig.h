#pragma once
#include <QJsonObject>
#include <QString>
#include <QUrl>

#include <QRegularExpression>

#ifdef USE_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
}
#endif

// Hanwha PNM 시리즈 멀티센서 카메라의 채널 URL 및 가속 프로파일을 관리한다.
//
//   rtsp://<계정>:<비번>@<IP>:554/<0~3>/profile<N>/media.smp
//
// 센서 0~3 이 CH1~CH4 에 대응한다.
//   - H.265: profile5 (800x448 4분할 서브) / profile3 (2592x1520 1채널 메인)
//   - H.264: profile4 (800x448 4분할 서브) / profile2 (2592x1520 1채널 메인)
namespace CameraConfig {

enum class DecoderMode {
    GpuHevc,   // H.265 GPU 가속 (profile5 4분할 / profile3 1채널)
    GpuH264,   // H.264 GPU 가속 (profile4 4분할 / profile2 1채널)
    CpuH264    // H.264 CPU 소프트웨어 (profile4 4분할 / profile2 1채널)
};

struct HwCapability {
    DecoderMode mode = DecoderMode::CpuH264;
    QString hwDeviceName = QStringLiteral("none"); // "d3d11va", "dxva2", "none"
    QString codecName = QStringLiteral("h264");    // "hevc" or "h264"
    int gridProfile = 4;                           // 5 (H.265) or 4 (H.264)
    int soloProfile = 2;                           // 3 (H.265) or 2 (H.264)
    QString description;                           // 상세 설명
};

inline HwCapability detectHwCapability() {
    HwCapability cap;
#ifdef USE_FFMPEG
    // 우선순위 1: D3D11VA, 우선순위 2: DXVA2
    const char *preferredHwTypes[] = {"d3d11va", "dxva2", nullptr};

    for (int i = 0; preferredHwTypes[i] != nullptr; ++i) {
        const char *hwName = preferredHwTypes[i];
        enum AVHWDeviceType hwType = av_hwdevice_find_type_by_name(hwName);
        if (hwType == AV_HWDEVICE_TYPE_NONE) continue;

        AVBufferRef *hwCtx = nullptr;
        if (av_hwdevice_ctx_create(&hwCtx, hwType, nullptr, nullptr, 0) < 0) {
            continue;
        }

        // 1. HEVC (H.265) 지원 여부 확인 (최우선)
        const AVCodec *hevcDec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
        bool hevcSupported = false;
        if (hevcDec) {
            for (int k = 0;; ++k) {
                const AVCodecHWConfig *cfg = avcodec_get_hw_config(hevcDec, k);
                if (!cfg) break;
                if ((cfg->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) && cfg->device_type == hwType) {
                    hevcSupported = true;
                    break;
                }
            }
        }

        if (hevcSupported) {
            av_buffer_unref(&hwCtx);
            cap.mode = DecoderMode::GpuHevc;
            cap.hwDeviceName = QString::fromLatin1(hwName);
            cap.codecName = QStringLiteral("hevc");
            cap.gridProfile = 5; // H.265 800x448
            cap.soloProfile = 3; // H.265 2592x1520
            cap.description = QStringLiteral("GPU 가속 모드 (H.265 HEVC, %1)").arg(cap.hwDeviceName.toUpper());
            return cap;
        }

        // 2. H.264 지원 여부 확인 (차선)
        const AVCodec *h264Dec = avcodec_find_decoder(AV_CODEC_ID_H264);
        bool h264Supported = false;
        if (h264Dec) {
            for (int k = 0;; ++k) {
                const AVCodecHWConfig *cfg = avcodec_get_hw_config(h264Dec, k);
                if (!cfg) break;
                if ((cfg->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) && cfg->device_type == hwType) {
                    h264Supported = true;
                    break;
                }
            }
        }

        if (h264Supported) {
            av_buffer_unref(&hwCtx);
            cap.mode = DecoderMode::GpuH264;
            cap.hwDeviceName = QString::fromLatin1(hwName);
            cap.codecName = QStringLiteral("h264");
            cap.gridProfile = 4; // H.264 800x448
            cap.soloProfile = 2; // H.264 2592x1520
            cap.description = QStringLiteral("GPU 가속 모드 (H.264 AVC, %1)").arg(cap.hwDeviceName.toUpper());
            return cap;
        }

        av_buffer_unref(&hwCtx);
    }
#endif
    // 3. Fallback: CPU H.264
    cap.mode = DecoderMode::CpuH264;
    cap.hwDeviceName = QStringLiteral("none");
    cap.codecName = QStringLiteral("h264");
    cap.gridProfile = 4;
    cap.soloProfile = 2;
    cap.description = QStringLiteral("CPU 소프트웨어 디코딩 (H.264 AVC)");
    return cap;
}

inline QString setUrlProfile(const QString &url, int profileNum) {
    static const QRegularExpression re(QStringLiteral("/profile\\d+/"));
    QString res = url;
    res.replace(re, QStringLiteral("/profile%1/").arg(profileNum));
    return res;
}

inline QJsonObject buildChannels(const QString &host,
                                 const QString &user,
                                 const QString &password,
                                 int port = 554,
                                 int defaultProfile = 4)
{
    QJsonObject channels;
    if (host.trimmed().isEmpty()) return channels;

    // 계정·비밀번호에 @ : / 같은 문자가 있으면 URL 이 깨진다. 퍼센트 인코딩한다.
    const QString u = QUrl::toPercentEncoding(user).constData();
    const QString p = QUrl::toPercentEncoding(password).constData();
    const QString cred = u.isEmpty() ? QString() : QStringLiteral("%1:%2@").arg(u, p);

    // ⚠️ 여기서 QString::arg() 를 쓰면 안 된다. cred 에 퍼센트 인코딩 결과(%40, %2F …)가
    //   들어 있는데, 뒤이은 arg() 가 그 %2/%3 을 **자리표시자로 착각해 치환**한다.
    //   비밀번호에 / 나 : 가 있으면 URL 이 통째로 망가진다(실제로 겪음). 그래서 이어붙인다.
    const QString base = QStringLiteral("rtsp://") + cred + host.trimmed()
                         + QLatin1Char(':') + QString::number(port) + QLatin1Char('/');
    for (int sensor = 0; sensor < 4; ++sensor) {
        channels.insert(QString::number(sensor + 1),
                        base + QString::number(sensor) + QStringLiteral("/profile%1/media.smp").arg(defaultProfile));
    }
    return channels;
}

}  // namespace CameraConfig
