#include "EnrollDialog.h"
#include "ConfigPath.h"
#include "CameraConfig.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QSysInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHostAddress>
#include <QSslCertificate>
#include <QSslConfiguration>

namespace {
constexpr int kDefaultPort   = 8443;
constexpr int kTimeoutMs     = 15000;

// 서버 인증서에 들어 있는 이름. gen-certs.sh 가 CN=$(hostname) 로 발급하므로
// RPi 기본 호스트명이 그대로 들어간다. mqtt.json 의 server_name 기본값과 같다.
constexpr char kCertHostName[] = "raspberrypi";

// 실행파일에 박아둔 사설 CA. 이것만 신뢰하도록 갈아끼운다(시스템 CA 는 쓰지 않는다).
QList<QSslCertificate> bundledCa() {
    QFile f(QStringLiteral(":/certs/ca.crt"));
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QSslCertificate::fromData(f.readAll(), QSsl::Pem);
}
}

EnrollDialog::EnrollDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(QString::fromUtf8("SPATIAL-VMS 최초 설정"));
    setModal(true);

    auto *intro = new QLabel(QString::fromUtf8(
        "관리자에게 받은 <b>1회용 토큰</b>을 입력하면 인증서와 카메라 설정을 자동으로 받아옵니다.<br>"
        "한 번만 하면 되고, 다음 실행부터는 묻지 않습니다."), this);
    intro->setWordWrap(true);

    m_host = new QLineEdit(this);
    m_host->setPlaceholderText(QString::fromUtf8("예: 172.20.32.110"));

    m_port = new QSpinBox(this);
    m_port->setRange(1, 65535);
    m_port->setValue(kDefaultPort);

    m_token = new QLineEdit(this);
    m_token->setPlaceholderText(QString::fromUtf8("관리자에게 받은 토큰"));

    m_device = new QLineEdit(this);
    m_device->setText(QSysInfo::machineHostName());   // 브로커 로그에서 누구 것인지 구분용

    // 카메라는 RPi 와 물리적으로 떨어져 있고 데몬은 카메라를 건드리지 않는다.
    // RTSP 는 이 앱이 카메라로 직접 연결하므로 여기서 같이 받는다.
    m_camHost = new QLineEdit(this);
    m_camHost->setPlaceholderText(QString::fromUtf8("예: 172.20.33.8 (비우면 서버 설정 사용)"));
    m_camUser = new QLineEdit(this);
    m_camUser->setPlaceholderText(QString::fromUtf8("예: admin"));
    m_camPass = new QLineEdit(this);
    m_camPass->setEchoMode(QLineEdit::Password);

    auto *form = new QFormLayout;
    form->addRow(QString::fromUtf8("발급 서버 주소"), m_host);
    form->addRow(QString::fromUtf8("포트"),           m_port);
    form->addRow(QString::fromUtf8("토큰"),           m_token);
    form->addRow(QString::fromUtf8("기기 이름"),       m_device);

    auto *camLabel = new QLabel(QString::fromUtf8("<b>카메라 (CCTV)</b>"), this);
    form->addRow(camLabel);
    form->addRow(QString::fromUtf8("카메라 IP"),      m_camHost);
    form->addRow(QString::fromUtf8("카메라 계정"),     m_camUser);
    form->addRow(QString::fromUtf8("카메라 비밀번호"), m_camPass);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);

    m_submit = new QPushButton(QString::fromUtf8("발급받기"), this);
    m_submit->setDefault(true);
    auto *cancel = new QPushButton(QString::fromUtf8("나중에"), this);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(cancel);
    buttons->addWidget(m_submit);

    auto *root = new QVBoxLayout(this);
    root->addWidget(intro);
    root->addSpacing(8);
    root->addLayout(form);
    root->addWidget(m_status);
    root->addLayout(buttons);

    connect(m_submit, &QPushButton::clicked, this, &EnrollDialog::submit);
    connect(cancel,   &QPushButton::clicked, this, &QDialog::reject);

    m_net = new QNetworkAccessManager(this);
}

void EnrollDialog::setBusy(bool busy) {
    m_submit->setEnabled(!busy);
    m_host->setEnabled(!busy);
    m_port->setEnabled(!busy);
    m_token->setEnabled(!busy);
    m_device->setEnabled(!busy);
    m_camHost->setEnabled(!busy);
    m_camUser->setEnabled(!busy);
    m_camPass->setEnabled(!busy);
}

void EnrollDialog::showError(const QString &msg) {
    m_status->setText(QStringLiteral("<span style='color:#c0392b'>%1</span>").arg(msg.toHtmlEscaped()));
}

void EnrollDialog::submit() {
    const QString host  = m_host->text().trimmed();
    const QString token = m_token->text().trimmed();
    if (host.isEmpty())  { showError(QString::fromUtf8("발급 서버 주소를 입력하세요.")); return; }
    if (token.isEmpty()) { showError(QString::fromUtf8("토큰을 입력하세요.")); return; }

    const QList<QSslCertificate> ca = bundledCa();
    if (ca.isEmpty()) {
        showError(QString::fromUtf8("내장 CA 인증서를 읽지 못했습니다. 배포본이 손상되었을 수 있습니다."));
        return;
    }

    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
    ssl.setCaCertificates(ca);   // 사설 CA 만 신뢰

    QNetworkRequest req(QUrl(QStringLiteral("https://%1:%2/enroll").arg(host).arg(m_port->value())));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setSslConfiguration(ssl);
    req.setTransferTimeout(kTimeoutMs);

    // 주소를 IP 로 입력하면 호스트명 검증이 걸린다. 서버 인증서 SAN 에는 발급
    // 당시의 IP 와 호스트명만 들어 있어서, RPi 가 DHCP 로 다른 주소를 받으면
    // "The host name did not match any of the valid hosts for this certificate"
    // 로 핸드셰이크 단계에서 끊긴다 — 서버는 멀쩡한데 연결 실패로만 보인다.
    //
    // 그래서 IP 로 들어온 경우에만 검증 이름을 인증서상의 이름으로 맞춘다.
    // 사설 CA 체인 검증은 그대로라, 이 CA 가 서명하지 않은 서버는 여전히 막힌다.
    // 호스트명을 직접 입력했다면 그 이름으로 검증한다(우회하지 않는다).
    if (!QHostAddress(host).isNull()) {
        req.setPeerVerifyName(QLatin1String(kCertHostName));
    }

    QJsonObject body;
    body["token"]       = token;
    body["device_name"] = m_device->text().trimmed();

    setBusy(true);
    m_status->setText(QString::fromUtf8("발급 요청 중…"));

    QNetworkReply *reply = m_net->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply] { handleReply(reply); });
}

void EnrollDialog::handleReply(QNetworkReply *reply) {
    reply->deleteLater();
    setBusy(false);

    const QByteArray raw = reply->readAll();
    const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();

    if (reply->error() != QNetworkReply::NoError && http == 0) {
        // 연결 자체가 안 된 경우 — 주소/포트 오류, 서버 미기동, 서버 인증서 검증 실패.
        // 이 실패는 간헐적으로 나온다(첫 요청만 실패하고 다시 누르면 되는 식).
        // errorString() 만으로는 어느 단계에서 끊겼는지 구분이 안 돼서, 다음에
        // 재현됐을 때 바로 짚을 수 있게 QNetworkReply::NetworkError 코드를 같이
        // 남긴다 — 6=SslHandshakeFailed, 3=HostNotFound, 4=Timeout 처럼 갈린다.
        showError(QString::fromUtf8("서버에 연결하지 못했습니다: %1 (code %2)\n\n"
                                    "주소·포트를 확인하고, 발급 서비스가 실행 중인지 확인하세요.\n"
                                    "다시 눌러 되는 경우가 있다면 이 code 를 알려주세요.")
                      .arg(reply->errorString())
                      .arg(int(reply->error())));
        return;
    }

    if (http != 200) {
        const QString msg = obj.value(QStringLiteral("error")).toString();
        showError(msg.isEmpty()
                      ? QString::fromUtf8("발급 실패 (HTTP %1)").arg(http)
                      : QString::fromUtf8("발급 실패: %1 (HTTP %2)").arg(msg).arg(http));
        return;
    }

    // HTTP 200 인데 전송 도중 끊긴 경우. 그냥 두면 잘린 본문이 JSON 파싱에
    // 실패해 "서버 응답에 ca_crt 이(가) 없습니다" 처럼 엉뚱한 원인으로 보인다.
    if (reply->error() != QNetworkReply::NoError) {
        showError(QString::fromUtf8("응답을 끝까지 받지 못했습니다: %1 (code %2)")
                      .arg(reply->errorString())
                      .arg(int(reply->error())));
        return;
    }

    if (obj.isEmpty()) {
        showError(QString::fromUtf8("서버 응답을 해석하지 못했습니다 (JSON 아님, %1 바이트).")
                      .arg(raw.size()));
        return;
    }

    QString err;
    if (!installBundle(obj, &err)) { showError(err); return; }

    const bool haveCam = !m_camHost->text().trimmed().isEmpty()
                         || !obj.value(QStringLiteral("cameras")).toObject().isEmpty();
    QMessageBox::information(
        this, QString::fromUtf8("설정 완료"),
        QString::fromUtf8("인증서를 받았습니다.\n\n발급된 이름: %1\n카메라: %2")
            .arg(obj.value(QStringLiteral("cn")).toString(QStringLiteral("(없음)")),
                 haveCam ? QString::fromUtf8("설정됨")
                         : QString::fromUtf8("설정 없음 — 영상이 나오지 않습니다.\n"
                                             "  메뉴의 '카메라 설정'에서 IP 를 입력하세요.")));
    accept();
}

bool EnrollDialog::installBundle(const QJsonObject &o, QString *err) {
    const QString root    = userDataRoot();
    const QString cfgDir  = root + QStringLiteral("/config");
    const QString certDir = root + QStringLiteral("/certs");

    if (!QDir().mkpath(cfgDir) || !QDir().mkpath(certDir)) {
        *err = QString::fromUtf8("설정 폴더를 만들지 못했습니다: %1").arg(root);
        return false;
    }

    // secret=true 면 소유자만 읽도록 좁힌다. Windows 는 POSIX 권한이 없어 건너뛴다.
    auto writeText = [&](const QString &path, const QString &text, bool secret) -> bool {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        const QByteArray data = text.toUtf8();
        if (f.write(data) != data.size()) return false;
        f.close();
#ifndef Q_OS_WIN
        if (secret) QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
#endif
        return true;
    };

    // 재등록이면 이전 발급물이 남아 있다. 특히 qt-console.key(PKCS#8)가
    // 문제인데, 서버는 그 이름으로 내려주지 않으므로 여기서 덮이지 않고
    // 살아남는다. 그런데 MqttBridge 는 **그 파일이 있으면 우선 선택**한다
    // (qt-console-trad.key 는 폴백이다). 결과적으로
    //     qt-console.crt      새 인증서
    //     qt-console.key      옛 개인키    ← 선택됨
    // 짝이 안 맞는 조합이 되어, Paho 가 TCP 연결 전 SSL 컨텍스트를 만들 때
    // 키/인증서 불일치로 실패한다. 소켓을 아예 안 열기 때문에 브로커 로그에는
    // 접속 시도조차 남지 않아, 네트워크 문제로 오해하기 쉽다. 실기에서 겪었다.
    //
    // 그래서 새로 쓰기 전에 이전 발급물을 지운다. 남겨서 얻을 것이 없다.
    for (const QString &stale : { QStringLiteral("/qt-console.key"),
                                  QStringLiteral("/qt-console-trad.key"),
                                  QStringLiteral("/qt-console.crt"),
                                  QStringLiteral("/ca.crt") }) {
        QFile::remove(certDir + stale);
    }

    // 쓰기 전에 응답에 필요한 값이 다 있는지 먼저 본다. 예전에는 쓰다가
    // 중간에 실패하면 이미 쓴 파일이 그대로 남았다 — 특히 mqtt.json 이 남으면
    // configReady() 가 true 가 되어, 다이얼로그는 "발급 실패"라고 말하는데
    // 앱은 로그인된 상태가 됐다. 아래에서 mqtt.json 을 맨 마지막에 쓰는 것도
    // 같은 이유다: 게이트가 되는 파일은 나머지가 다 성공한 뒤에만 생긴다.
    const QJsonObject mqtt = o.value(QStringLiteral("mqtt")).toObject();
    if (mqtt.value(QStringLiteral("host")).toString().isEmpty()) {
        *err = QString::fromUtf8("서버 응답에 mqtt.host 가 없습니다.");
        return false;
    }

    struct Item { const char *key; QString path; bool secret; };
    const QList<Item> certs = {
        { "ca_crt",     certDir + QStringLiteral("/ca.crt"),              false },
        { "client_crt", certDir + QStringLiteral("/qt-console.crt"),      false },
        // MqttBridge 가 PKCS#8(qt-console.key)을 먼저 찾고 없으면 이 이름으로 폴백한다.
        // 서버는 Qt 가 읽을 수 있는 전통 RSA 포맷을 내려주기로 계약했다.
        { "client_key", certDir + QStringLiteral("/qt-console-trad.key"), true  },
    };
    for (const Item &it : certs) {
        const QString pem = o.value(QLatin1String(it.key)).toString();
        if (pem.isEmpty()) {
            *err = QString::fromUtf8("서버 응답에 %1 이(가) 없습니다.").arg(QLatin1String(it.key));
            return false;
        }
        if (!writeText(it.path, pem, it.secret)) {
            *err = QString::fromUtf8("파일을 쓰지 못했습니다: %1").arg(it.path);
            return false;
        }
    }

    // 카메라: 사용자가 IP 를 입력했으면 그것이 이긴다.
    //   카메라는 RPi 와 물리적으로 떨어져 있고 데몬은 카메라를 건드리지 않는다.
    //   RTSP 는 이 앱이 카메라로 직접 연결하므로, 굳이 서버를 경유할 이유가 없다.
    //   서버 응답의 cameras 는 입력을 비웠을 때만 폴백으로 쓴다.
    QJsonObject cams;
    const QString camHost = m_camHost->text().trimmed();
    if (!camHost.isEmpty()) {
        cams.insert(QStringLiteral("channels"),
                    CameraConfig::buildChannels(camHost,
                                                m_camUser->text().trimmed(),
                                                m_camPass->text()));
    } else {
        cams = o.value(QStringLiteral("cameras")).toObject();
    }

    // 없어도 진행한다 — MQTT 는 되고 영상만 안 나오는 상태가 된다.
    if (!cams.isEmpty()) {
        if (!writeText(cfgDir + QStringLiteral("/cameras.json"),
                       QString::fromUtf8(QJsonDocument(cams).toJson(QJsonDocument::Indented)), true)) {
            *err = QString::fromUtf8("cameras.json 을 쓰지 못했습니다.");
            return false;
        }
    }

    // 마지막: configReady() 가 보는 파일. 여기까지 왔다는 건 인증서와 카메라
    // 설정이 모두 자리에 있다는 뜻이라, 이 파일이 생기는 순간 = 발급 완료다.
    //
    // cert_dir 은 상대경로로 둔다 — resolveConfigPath 가 사용자 데이터 디렉터리
    // 기준으로 찾아준다. 절대경로로 굳히면 홈 경로가 바뀔 때 깨진다.
    QJsonObject mqttOut;
    mqttOut["host"]     = mqtt.value(QStringLiteral("host")).toString();
    mqttOut["port"]     = mqtt.value(QStringLiteral("port")).toInt(8883);
    mqttOut["cert_dir"] = QStringLiteral("certs");
    // 스캔 파일(8443)도 같은 이유로 호스트명 검증이 걸린다 — ScanFetcher 가 이
    // 값으로 검증 이름을 맞춘다. 서버 인증서를 현재 주소로 재발급했다면 ""로
    // 바꾸면 host 검증으로 돌아간다.
    mqttOut["server_name"] = QLatin1String(kCertHostName);
    if (!writeText(cfgDir + QStringLiteral("/mqtt.json"),
                   QString::fromUtf8(QJsonDocument(mqttOut).toJson(QJsonDocument::Indented)), false)) {
        *err = QString::fromUtf8("mqtt.json 을 쓰지 못했습니다.");
        return false;
    }
    return true;
}
