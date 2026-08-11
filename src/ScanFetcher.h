#pragma once
#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVector>
#include "ScanCloud.h"

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class QSslConfiguration;
class QUrl;

// state/scan 이 준 .pcd 경로를 실제 파일로 바꿔 온다.
//
// 계약 §9(파일 전달 방식)가 미결이라 데몬은 "경로만" 보낸다. 그 경로는 RPi 의
// 데몬 작업 디렉터리 기준이라 Qt 가 도는 PC 에는 없다. 그래서 두 갈래로 찾는다.
//
//   1) 로컬에 이미 있으면 그대로 읽는다 — 개발 중 scp 로 받아둔 경우, 또는
//      나중에 공유 폴더가 정해졌을 때.
//   2) 없으면 발급 서비스(8443)의 GET /scan/<파일명> 으로 받아온다. mTLS 로
//      붙으며 클라이언트 인증서가 신원이 된다(브로커 ACL 과 같은 CN).
//
// 어느 쪽도 안 되면 실패 사유를 그대로 올려보낸다 — 조용히 빈 화면이 되는 것보다
// "왜 안 왔는지"가 보여야 한다.
// 목록에 뜨는 스캔 하나.
struct ScanEntry {
    QString   name;        // 파일명
    QString   localPath;   // 로컬에 있으면 실제 경로, 없으면 빈 문자열
    qint64    size = 0;
    QDateTime mtime;
    bool isLocal() const { return !localPath.isEmpty(); }
};

class ScanFetcher : public QObject {
    Q_OBJECT
public:
    explicit ScanFetcher(QObject *parent = nullptr);

    // 브로커와 같은 호스트를 쓴다(발급 서비스가 같은 RPi 에 있다).
    void setServer(const QString &host, quint16 port);
    void setClientCert(const QString &certPath, const QString &keyPath);

    // TLS 호스트명 검증에 쓸 이름. 접속은 setServer 의 host(보통 IP)로 하되
    // 인증서는 이 이름으로 맞춰본다 — 아래 주석 참고. 비우면 host 를 그대로 쓴다.
    void setPeerVerifyName(const QString &name);

    // pcdPath 는 데몬이 보낸 경로("./scans/xxx.pcd"). 파일명만 뽑아서 쓴다.
    void fetch(const QString &pcdPath);

    // 사용자가 직접 고른 로컬 파일.
    void loadLocal(const QString &path);

    // 로컬 scans/ 와 서버(GET /scans)를 합쳐 목록을 만든다. 같은 파일명이면
    // 로컬을 우선한다 — 이미 받아둔 걸 다시 내려받을 이유가 없다.
    void refreshList();

signals:
    void cloudReady(const ScanCloud &cloud);
    void failed(const QString &reason);
    void progress(const QString &message);   // 로그로 흘려보낼 진행 상황
    void listReady(const QVector<ScanEntry> &entries, const QString &note);

private:
    void handleReply(QNetworkReply *reply, const QString &label);
    void handleList(QNetworkReply *reply);
    QNetworkRequest makeRequest(const QUrl &url, const QSslConfiguration &ssl) const;
    QVector<ScanEntry> localEntries() const;
    QSslConfiguration sslConfig(bool *ok) const;
    QString localCandidate(const QString &fileName) const;

    QNetworkAccessManager *m_net = nullptr;
    QString  m_host;
    quint16  m_port = 8443;
    QString  m_certPath, m_keyPath;
    QString  m_verifyName;
};
