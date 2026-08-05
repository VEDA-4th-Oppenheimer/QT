#pragma once
#include <QJsonObject>
#include <QString>
#include <QUrl>

// Hanwha PNM 시리즈 멀티센서 카메라의 채널 URL 을 만든다.
//
//   rtsp://<계정>:<비번>@<IP>:554/<0~3>/profile2/media.smp
//
// 센서 0~3 이 CH1~CH4 에 대응한다. profile2 는 서브스트림(profile1 은 고해상도
// 메인스트림) — 타일 4개를 동시에 디코딩하므로 서브스트림을 쓴다.
//
// 카메라는 RPi 와 물리적으로 떨어져 있고 데몬은 카메라를 건드리지 않는다.
// RTSP 는 Qt 가 카메라로 직접 연결하므로, 카메라 정보는 사용자가 등록할 때
// 입력받는다(관리자 서버를 거치지 않는다).
namespace CameraConfig {

inline QJsonObject buildChannels(const QString &host,
                                 const QString &user,
                                 const QString &password,
                                 int port = 554)
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
                        base + QString::number(sensor) + QStringLiteral("/profile2/media.smp"));
    }
    return channels;
}

}  // namespace CameraConfig
