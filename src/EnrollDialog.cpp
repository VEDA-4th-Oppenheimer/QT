#include "EnrollDialog.h"
#include "ConfigPath.h"

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
#include <QSslCertificate>
#include <QSslConfiguration>

namespace {
constexpr int kDefaultPort   = 8443;
constexpr int kTimeoutMs     = 15000;

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

    auto *form = new QFormLayout;
    form->addRow(QString::fromUtf8("발급 서버 주소"), m_host);
    form->addRow(QString::fromUtf8("포트"),           m_port);
    form->addRow(QString::fromUtf8("토큰"),           m_token);
    form->addRow(QString::fromUtf8("기기 이름"),       m_device);

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
        // 연결 자체가 안 된 경우 — 주소/포트 오류, 서버 미기동, 서버 인증서 검증 실패
        showError(QString::fromUtf8("서버에 연결하지 못했습니다: %1\n\n"
                                    "주소·포트를 확인하고, 발급 서비스가 실행 중인지 확인하세요.")
                      .arg(reply->errorString()));
        return;
    }

    if (http != 200) {
        const QString msg = obj.value(QStringLiteral("error")).toString();
        showError(msg.isEmpty()
                      ? QString::fromUtf8("발급 실패 (HTTP %1)").arg(http)
                      : QString::fromUtf8("발급 실패: %1").arg(msg));
        return;
    }

    QString err;
    if (!installBundle(obj, &err)) { showError(err); return; }

    QMessageBox::information(
        this, QString::fromUtf8("설정 완료"),
        QString::fromUtf8("인증서와 카메라 설정을 받았습니다.\n\n발급된 이름: %1")
            .arg(obj.value(QStringLiteral("cn")).toString(QStringLiteral("(없음)"))));
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

    // cert_dir 은 상대경로로 둔다 — resolveConfigPath 가 사용자 데이터 디렉터리
    // 기준으로 찾아준다. 절대경로로 굳히면 홈 경로가 바뀔 때 깨진다.
    const QJsonObject mqtt = o.value(QStringLiteral("mqtt")).toObject();
    QJsonObject mqttOut;
    mqttOut["host"]     = mqtt.value(QStringLiteral("host")).toString();
    mqttOut["port"]     = mqtt.value(QStringLiteral("port")).toInt(8883);
    mqttOut["cert_dir"] = QStringLiteral("certs");
    if (mqttOut["host"].toString().isEmpty()) {
        *err = QString::fromUtf8("서버 응답에 mqtt.host 가 없습니다.");
        return false;
    }
    if (!writeText(cfgDir + QStringLiteral("/mqtt.json"),
                   QString::fromUtf8(QJsonDocument(mqttOut).toJson(QJsonDocument::Indented)), false)) {
        *err = QString::fromUtf8("mqtt.json 을 쓰지 못했습니다.");
        return false;
    }

    // 카메라 설정은 없어도 진행한다 — MQTT 는 되고 영상만 안 나오는 상태가 된다.
    const QJsonObject cams = o.value(QStringLiteral("cameras")).toObject();
    if (!cams.isEmpty()) {
        if (!writeText(cfgDir + QStringLiteral("/cameras.json"),
                       QString::fromUtf8(QJsonDocument(cams).toJson(QJsonDocument::Indented)), true)) {
            *err = QString::fromUtf8("cameras.json 을 쓰지 못했습니다.");
            return false;
        }
    }
    return true;
}
