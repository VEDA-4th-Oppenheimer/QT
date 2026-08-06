#include "ScanFetcher.h"
#include "ConfigPath.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSet>
#include <QSslKey>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <algorithm>

namespace {
constexpr int kTimeoutMs = 20000;

// 실행파일에 박아둔 사설 CA — 시스템 CA 는 쓰지 않는다(EnrollDialog 와 동일).
QList<QSslCertificate> bundledCa() {
    QFile f(QStringLiteral(":/certs/ca.crt"));
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QSslCertificate::fromData(f.readAll(), QSsl::Pem);
}
}   // namespace

ScanFetcher::ScanFetcher(QObject *parent)
    : QObject(parent), m_net(new QNetworkAccessManager(this)) {}

void ScanFetcher::setServer(const QString &host, quint16 port) {
    m_host = host;
    m_port = port;
}

void ScanFetcher::setClientCert(const QString &certPath, const QString &keyPath) {
    m_certPath = certPath;
    m_keyPath = keyPath;
}

// 데몬 경로의 파일명이 개발 트리/사용자 데이터의 scans/ 아래에 이미 있는지 본다.
QString ScanFetcher::localCandidate(const QString &fileName) const {
    const QString rel = QStringLiteral("scans/") + fileName;
    const QString resolved = resolveConfigPath(rel);
    return QFileInfo::exists(resolved) ? resolved : QString();
}

void ScanFetcher::loadLocal(const QString &path) {
    ScanCloud cloud;
    QString err;
    if (!loadPcdAscii(path, &cloud, &err)) {
        emit failed(QStringLiteral("%1 — %2").arg(QFileInfo(path).fileName(), err));
        return;
    }
    emit progress(QStringLiteral("로컬 파일 %1 (%2점)")
                      .arg(QFileInfo(path).fileName()).arg(cloud.count()));
    emit cloudReady(cloud);
}

QSslConfiguration ScanFetcher::sslConfig(bool *ok) const {
    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
    const QList<QSslCertificate> ca = bundledCa();
    if (ca.isEmpty()) { if (ok) *ok = false; return ssl; }
    ssl.setCaCertificates(ca);

    if (!m_certPath.isEmpty() && !m_keyPath.isEmpty()) {
        QFile cf(m_certPath), kf(m_keyPath);
        if (cf.open(QIODevice::ReadOnly) && kf.open(QIODevice::ReadOnly)) {
            const QSslCertificate cert(cf.readAll(), QSsl::Pem);
            const QSslKey key(kf.readAll(), QSsl::Rsa, QSsl::Pem);
            if (!cert.isNull() && !key.isNull()) {
                ssl.setLocalCertificate(cert);
                ssl.setPrivateKey(key);
            }
        }
    }
    if (ok) *ok = true;
    return ssl;
}

// 로컬 scans/ 에 이미 받아둔 파일들.
QVector<ScanEntry> ScanFetcher::localEntries() const {
    QVector<ScanEntry> out;
    const QString dirPath = resolveConfigPath(QStringLiteral("scans"));
    QDir dir(dirPath);
    if (!dir.exists()) return out;
    const QFileInfoList files = dir.entryInfoList(QStringList{QStringLiteral("*.pcd")},
                                                  QDir::Files, QDir::Name);
    for (const QFileInfo &fi : files) {
        ScanEntry e;
        e.name = fi.fileName();
        e.localPath = fi.absoluteFilePath();
        e.size = fi.size();
        e.mtime = fi.lastModified();
        out.push_back(e);
    }
    return out;
}

void ScanFetcher::refreshList() {
    const QVector<ScanEntry> local = localEntries();

    if (m_host.isEmpty()) {
        emit listReady(local, QStringLiteral("로컬 %1건 · 서버 주소 없음").arg(local.size()));
        return;
    }
    bool ok = false;
    QSslConfiguration ssl = sslConfig(&ok);
    if (!ok) {
        emit listReady(local, QStringLiteral("로컬 %1건 · 내장 CA 없음").arg(local.size()));
        return;
    }

    QNetworkRequest req(QUrl(QStringLiteral("https://%1:%2/scans").arg(m_host).arg(m_port)));
    req.setSslConfiguration(ssl);
    req.setTransferTimeout(kTimeoutMs);
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] { handleList(reply); });
}

void ScanFetcher::handleList(QNetworkReply *reply) {
    reply->deleteLater();
    QVector<ScanEntry> entries = localEntries();
    QSet<QString> seen;
    for (const ScanEntry &e : entries) seen.insert(e.name);

    const QByteArray body = reply->readAll();
    const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        const QString why = (http == 0) ? reply->errorString()
                                        : QStringLiteral("HTTP %1").arg(http);
        emit listReady(entries, QStringLiteral("로컬 %1건 · 서버 목록 실패(%2)")
                                    .arg(entries.size()).arg(why));
        return;
    }

    const QJsonArray arr = QJsonDocument::fromJson(body).object().value("scans").toArray();
    int remote = 0;
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        const QString name = o.value("name").toString();
        if (name.isEmpty() || seen.contains(name)) continue;   // 로컬 우선
        ScanEntry e;
        e.name = name;
        e.size = static_cast<qint64>(o.value("size").toDouble());
        const qint64 mt = static_cast<qint64>(o.value("mtime").toDouble());
        if (mt > 0) e.mtime = QDateTime::fromSecsSinceEpoch(mt);
        entries.push_back(e);
        ++remote;
    }

    // 최신이 위로. mtime 이 없으면(로컬 목록엔 항상 있다) 이름 역순으로 떨어진다 —
    // 파일명이 calib-YYYYMMDD-HHMMSS 라 이름 정렬도 시간 정렬과 같다.
    std::sort(entries.begin(), entries.end(), [](const ScanEntry &a, const ScanEntry &b) {
        if (a.mtime.isValid() && b.mtime.isValid() && a.mtime != b.mtime) return a.mtime > b.mtime;
        return a.name > b.name;
    });
    emit listReady(entries, QStringLiteral("로컬 %1건 · 서버 %2건")
                                .arg(entries.size() - remote).arg(remote));
}

void ScanFetcher::fetch(const QString &pcdPath) {
    if (pcdPath.isEmpty()) return;

    // 데몬이 준 경로는 RPi 기준이다. 디렉터리는 버리고 파일명만 쓴다 —
    // 서버도 파일명만 받도록 되어 있고(경로 탈출 차단), 로컬 탐색도 파일명 기준이다.
    const QString fileName = QFileInfo(pcdPath).fileName();
    if (fileName.isEmpty()) {
        emit failed(QStringLiteral("스캔 경로에서 파일명을 얻지 못했다: %1").arg(pcdPath));
        return;
    }

    const QString local = localCandidate(fileName);
    if (!local.isEmpty()) {
        loadLocal(local);
        return;
    }

    if (m_host.isEmpty()) {
        emit failed(QStringLiteral("%1 — 로컬에 없고 발급 서버 주소도 모른다").arg(fileName));
        return;
    }

    bool sslOk = false;
    QSslConfiguration ssl = sslConfig(&sslOk);
    if (!sslOk) {
        emit failed(QStringLiteral("내장 CA 인증서를 읽지 못했다"));
        return;
    }

    const QUrl url(QStringLiteral("https://%1:%2/scan/%3")
                       .arg(m_host).arg(m_port).arg(QString::fromUtf8(QUrl::toPercentEncoding(fileName))));
    QNetworkRequest req(url);
    req.setSslConfiguration(ssl);
    req.setTransferTimeout(kTimeoutMs);

    emit progress(QStringLiteral("스캔 파일 요청 %1").arg(fileName));
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, fileName] {
        handleReply(reply, fileName);
    });
}

void ScanFetcher::handleReply(QNetworkReply *reply, const QString &label) {
    reply->deleteLater();
    const QByteArray body = reply->readAll();
    const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        // http==0 이면 연결 자체가 안 된 것(서비스 미기동/주소 오류/TLS 검증 실패).
        const QString why = (http == 0)
            ? reply->errorString()
            : QStringLiteral("HTTP %1 — %2").arg(http).arg(QString::fromUtf8(body.left(200)).trimmed());
        emit failed(QStringLiteral("%1 — %2").arg(label, why));
        return;
    }

    ScanCloud cloud;
    QString err;
    if (!parsePcdAscii(body, label, &cloud, &err)) {
        emit failed(QStringLiteral("%1 — %2").arg(label, err));
        return;
    }
    emit progress(QStringLiteral("스캔 파일 수신 %1 (%2점, %3 KB)")
                      .arg(label).arg(cloud.count()).arg(body.size() / 1024));
    emit cloudReady(cloud);
}
