#pragma once
#include <QDialog>

class QLineEdit;
class QSpinBox;
class QLabel;
class QPushButton;
class QNetworkAccessManager;
class QNetworkReply;
class QJsonObject;

// 최초 실행 등록 마법사.
//
// 배포본에는 인증서도 카메라 설정도 들어 있지 않다(둘 다 비밀정보라 배포 금지).
// 대신 사용자가 1회용 토큰을 입력하면 RPi 의 발급 서비스에서 다음을 한 번에 받아
// 사용자 데이터 디렉터리에 깔고, 그 뒤로는 아무것도 묻지 않는다.
//
//   POST https://<host>:<port>/enroll   {"token","device_name"}
//   200  {"cn","ca_crt","client_crt","client_key","mqtt":{...},"cameras":{...}}
//
// 서버 신원은 실행파일에 박아둔 :/certs/ca.crt 로 검증한다 — 아직 받은 인증서가
// 없는 시점이라 이것 말고는 검증 근거가 없다.
//
// 받은 파일은 개발 트리와 같은 배치로 저장한다(resolveConfigPath 가 그대로 찾는다):
//   <userDataRoot>/config/mqtt.json, config/cameras.json
//   <userDataRoot>/certs/{ca.crt, qt-console.crt, qt-console-trad.key}
// ※ 파일 이름은 CN 이 사람마다 달라져도 그대로 둔다. MqttBridge 가 이 이름으로
//   찾기 때문이다(CN 과 파일명은 무관하다).
class EnrollDialog : public QDialog {
    Q_OBJECT
public:
    explicit EnrollDialog(QWidget *parent = nullptr);

private:
    void submit();
    void handleReply(QNetworkReply *reply);
    // 응답 JSON 을 사용자 데이터 디렉터리에 푼다. 실패하면 false 와 사유.
    bool installBundle(const QJsonObject &o, QString *err);
    void setBusy(bool busy);
    void showError(const QString &msg);

    QLineEdit   *m_host   = nullptr;
    QSpinBox    *m_port   = nullptr;
    QLineEdit   *m_token  = nullptr;
    QLineEdit   *m_device = nullptr;
    QLineEdit   *m_camHost = nullptr;
    QLineEdit   *m_camUser = nullptr;
    QLineEdit   *m_camPass = nullptr;
    QLabel      *m_status = nullptr;
    QPushButton *m_submit = nullptr;

    QNetworkAccessManager *m_net = nullptr;
};
