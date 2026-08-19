#include "SettingsTab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QCheckBox>
#include <QScrollArea>

namespace {

QLabel *sectionTitle(QWidget *parent, const QString &text) {
    auto *l = new QLabel(text, parent);
    l->setStyleSheet(Theme::mono(10, 700) +
                     QString("color:%1;letter-spacing:1.4px;").arg(Theme::TextFaint.name()));
    return l;
}

// 한 줄 = 제목 + 설명 + 오른쪽 조작부. 값을 보여줄 수 있는 것이 메뉴와의 차이라
// 조작부 왼쪽에 현재 값을 놓을 수 있게 valueOut 을 돌려준다.
QFrame *row(QWidget *parent, const QString &title, const QString &desc,
            QWidget *control, QLabel **valueOut = nullptr) {
    auto *card = new QFrame(parent);
    card->setObjectName("card");
    auto *h = new QHBoxLayout(card);
    h->setContentsMargins(13, 11, 13, 11);
    h->setSpacing(14);

    auto *left = new QVBoxLayout;
    left->setSpacing(3);
    auto *t = new QLabel(title, card);
    t->setStyleSheet(QString("color:%1;font-size:13px;font-weight:600;").arg(Theme::Text2.name()));
    left->addWidget(t);
    auto *d = new QLabel(desc, card);
    d->setStyleSheet(QString("color:%1;font-size:11px;").arg(Theme::TextMuted.name()));
    d->setWordWrap(true);
    left->addWidget(d);
    h->addLayout(left, 1);

    if (valueOut) {
        *valueOut = new QLabel(card);
        (*valueOut)->setStyleSheet(Theme::mono(12) + QString("color:%1;").arg(Theme::Text3.name()));
        (*valueOut)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        h->addWidget(*valueOut);
    }
    if (control) h->addWidget(control);
    return card;
}

QPushButton *button(QWidget *parent, const QString &text, bool danger = false) {
    auto *b = new QPushButton(text, parent);
    b->setCursor(Qt::PointingHandCursor);
    b->setMinimumWidth(112);
    if (danger) {
        b->setStyleSheet(QString(
            "QPushButton{background:%1;color:%2;border:1px solid %3;border-radius:3px;"
            "padding:6px 12px;font-size:12px;}"
            "QPushButton:hover{background:%4;}")
            .arg(Theme::DangerBg.name(), Theme::DangerText.name(),
                 Theme::DangerBorder.name(), Theme::Danger.name()));
    }
    return b;
}

} // namespace

SettingsTab::SettingsTab(const State &s, QWidget *parent)
    : QWidget(parent), m_theme(s.theme), m_topViewDetached(s.topViewDetached) {

    // 항목이 늘어나도 잘리지 않게 스크롤 안에 담는다.
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto *page = new QWidget(scroll);
    scroll->setWidget(page);
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(9);

    // ---- 화면 -------------------------------------------------------------
    root->addWidget(sectionTitle(page, QString::fromUtf8("화면")));

    auto *themeBox = new QWidget(page);
    auto *themeLay = new QHBoxLayout(themeBox);
    themeLay->setContentsMargins(0, 0, 0, 0);
    themeLay->setSpacing(6);
    m_devTheme  = button(themeBox, QString::fromUtf8("다크"));
    m_userTheme = button(themeBox, QString::fromUtf8("라이트"));
    m_devTheme->setCheckable(true);
    m_userTheme->setCheckable(true);
    themeLay->addWidget(m_devTheme);
    themeLay->addWidget(m_userTheme);
    root->addWidget(row(page, QString::fromUtf8("테마"),
                        QString::fromUtf8("전환하면 화면을 다시 그립니다. 진행 중인 스캔에는 영향이 없습니다."),
                        themeBox));
    refreshThemeButtons();
    connect(m_devTheme, &QPushButton::clicked, this, [this] {
        m_theme = Theme::Mode::Developer;
        refreshThemeButtons();
        emit themeChangeRequested(Theme::Mode::Developer);
    });
    connect(m_userTheme, &QPushButton::clicked, this, [this] {
        m_theme = Theme::Mode::User;
        refreshThemeButtons();
        emit themeChangeRequested(Theme::Mode::User);
    });

    m_topView = button(page, QString());
    refreshTopViewButton();
    connect(m_topView, &QPushButton::clicked, this, &SettingsTab::topViewFullScreenToggled);
    root->addWidget(row(page, QString::fromUtf8("TOP-VIEW 전체화면"),
                        QString::fromUtf8("TOP-VIEW 를 별도 창으로 띄워 모니터 하나를 통째로 씁니다."),
                        m_topView));

    // ---- 연결 -------------------------------------------------------------
    root->addWidget(sectionTitle(page, QString::fromUtf8("연결")));

    auto *camBtn = button(page, QString::fromUtf8("변경…"));
    connect(camBtn, &QPushButton::clicked, this, &SettingsTab::cameraSettingsRequested);
    root->addWidget(row(page, QString::fromUtf8("카메라"),
                        QString::fromUtf8("CCTV 주소와 계정. 영상은 이 앱이 카메라에 직접 연결합니다(RPi 경유 아님)."),
                        camBtn, &m_cameraValue));
    setCameraSummary(s.cameraHost, s.cameraChannels);

    auto *retryBtn = button(page, QString::fromUtf8("재연결"));
    connect(retryBtn, &QPushButton::clicked, this, &SettingsTab::cameraReconnectRequested);
    root->addWidget(row(page, QString::fromUtf8("CCTV 재연결"),
                        QString::fromUtf8("연결이 끊긴 뒤 자동 재시도를 멈춘 상태에서 다시 붙습니다."),
                        retryBtn));

    // ---- 스캔 -------------------------------------------------------------
    root->addWidget(sectionTitle(page, QString::fromUtf8("스캔")));

    auto *heightBtn = button(page, QString::fromUtf8("변경…"));
    connect(heightBtn, &QPushButton::clicked, this, &SettingsTab::sensorHeightRequested);
    root->addWidget(row(page, QString::fromUtf8("센서 높이"),
                        QString::fromUtf8("바닥에서 pan/tilt 축까지의 높이. 스캔 명령에 실려 나갑니다."),
                        heightBtn, &m_heightValue));
    setSensorHeight(s.sensorHeightMm);

    auto *openBtn = button(page, QString::fromUtf8("열기…"));
    connect(openBtn, &QPushButton::clicked, this, &SettingsTab::openScanFileRequested);
    root->addWidget(row(page, QString::fromUtf8("스캔 파일 열기"),
                        QString::fromUtf8("저장해 둔 .pcd 를 TOP-VIEW 에 올립니다. 브로커 연결 없이도 됩니다."),
                        openBtn));

    // ---- 실행 모드 ---------------------------------------------------------
    root->addWidget(sectionTitle(page, QString::fromUtf8("실행 모드")));

    m_demo = new QCheckBox(page);
    m_demo->setChecked(s.demoMode);
    m_demo->setCursor(Qt::PointingHandCursor);
    connect(m_demo, &QCheckBox::toggled, this, &SettingsTab::demoModeToggled);
    root->addWidget(row(page, QString::fromUtf8("Demo 모드"),
                        QString::fromUtf8("브로커 없이 가짜 데이터로 화면을 채웁니다. 장비에는 아무 명령도 나가지 않습니다."),
                        m_demo));

    // ---- 계정 -------------------------------------------------------------
    root->addWidget(sectionTitle(page, QString::fromUtf8("계정")));

    auto *outBtn = button(page, QString::fromUtf8("로그아웃"), /*danger=*/true);
    connect(outBtn, &QPushButton::clicked, this, &SettingsTab::logoutRequested);
    root->addWidget(row(page, QString::fromUtf8("로그아웃"),
                        QString::fromUtf8("이 기기에 저장된 인증서와 접속 설정을 지웁니다. 다시 쓰려면 재발급이 필요합니다."),
                        outBtn));

    root->addStretch(1);
}

void SettingsTab::refreshThemeButtons() {
    const bool dev = (m_theme == Theme::Mode::Developer);
    const QString on = QString(
        "QPushButton{background:%1;color:%2;border:1px solid %1;border-radius:3px;"
        "padding:6px 12px;font-size:12px;font-weight:600;}")
        .arg(Theme::Accent.name(), Theme::Bg.name());
    const QString off = QString(
        "QPushButton{background:transparent;color:%1;border:1px solid %2;border-radius:3px;"
        "padding:6px 12px;font-size:12px;}"
        "QPushButton:hover{color:%3;border-color:%4;}")
        .arg(Theme::TextMuted.name(), Theme::Border.name(),
             Theme::Text2.name(), Theme::Accent.name());
    m_devTheme->setStyleSheet(dev ? on : off);
    m_userTheme->setStyleSheet(dev ? off : on);
    m_devTheme->setChecked(dev);
    m_userTheme->setChecked(!dev);
}

void SettingsTab::refreshTopViewButton() {
    m_topView->setText(m_topViewDetached ? QString::fromUtf8("원래대로")
                                         : QString::fromUtf8("전체화면"));
}

void SettingsTab::setSensorHeight(int mm) {
    if (m_heightValue)
        m_heightValue->setText(QString("%1 mm").arg(mm));
}

void SettingsTab::setCameraSummary(const QString &host, int channels) {
    if (!m_cameraValue) return;
    if (host.isEmpty()) {
        m_cameraValue->setText(QString::fromUtf8("미설정"));
        m_cameraValue->setStyleSheet(Theme::mono(12) + QString("color:%1;").arg(Theme::Warn.name()));
    } else {
        m_cameraValue->setText(channels > 0 ? QString("%1 · %2ch").arg(host).arg(channels) : host);
        m_cameraValue->setStyleSheet(Theme::mono(12) + QString("color:%1;").arg(Theme::Text3.name()));
    }
}

void SettingsTab::setTopViewDetached(bool detached) {
    m_topViewDetached = detached;
    if (m_topView) refreshTopViewButton();
}

void SettingsTab::setDemoMode(bool demo) {
    if (m_demo && m_demo->isChecked() != demo) {
        QSignalBlocker b(m_demo);
        m_demo->setChecked(demo);
    }
}
