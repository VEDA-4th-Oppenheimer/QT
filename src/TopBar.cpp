#include "TopBar.h"
#include "Theme.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QDateTime>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

namespace {
QLabel *pulseDot(QWidget *parent, const QColor &color, int px = 7) {
    auto *dot = new QLabel(QChar(0x25CF), parent);   // ●
    dot->setStyleSheet(QString("color:%1;font-size:%2px;").arg(color.name()).arg(px));
    auto *effect = new QGraphicsOpacityEffect(dot);
    effect->setOpacity(1.0);
    dot->setGraphicsEffect(effect);
    auto *anim = new QPropertyAnimation(effect, "opacity", dot);
    anim->setDuration(2000);
    anim->setStartValue(1.0);
    anim->setKeyValueAt(0.5, 0.25);
    anim->setEndValue(1.0);
    anim->setLoopCount(-1);
    anim->start();
    return dot;
}

QFrame *chip(QWidget *parent, const QString &label, QLabel **value, QLabel **dot = nullptr) {
    auto *f = new QFrame(parent);
    f->setObjectName("chip");
    auto *l = new QHBoxLayout(f);
    l->setContentsMargins(10, 5, 10, 5);
    l->setSpacing(7);
    auto *k = new QLabel(label, f);
    k->setStyleSheet(Theme::mono(10, 500) + QString("color:%1;letter-spacing:1px;").arg(Theme::TextDim.name()));
    l->addWidget(k);
    if (dot) {
        *dot = pulseDot(f, Theme::TextFaint);
        l->addWidget(*dot);
    }
    *value = new QLabel(f);
    (*value)->setStyleSheet(Theme::mono(10, 500) + QString("color:%1;").arg(Theme::Text.name()));
    l->addWidget(*value);
    return f;
}
}

TopBar::TopBar(QWidget *parent) : QFrame(parent) {
    setFixedHeight(54);
    setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
        .arg(Theme::BarBg.name(), Theme::Border.name()));

    auto *l = new QHBoxLayout(this);
    l->setContentsMargins(18, 0, 18, 0);
    l->setSpacing(16);

    auto *title = new QLabel(QString::fromUtf8("SPATIAL·VMS"), this);
    title->setStyleSheet(Theme::mono(14, 700) + QString("letter-spacing:2px;color:%1;").arg(Theme::Text.name()));
    auto *sub = new QLabel("v1.0 / ADTS SCANNER KIT", this);
    sub->setStyleSheet(Theme::mono(10) + QString("letter-spacing:2px;color:%1;").arg(Theme::TextFaint.name()));

    l->addWidget(title);
    l->addWidget(sub);
    l->addWidget(chip(this, "MQTT",  &m_mqtt, &m_mqttDot));
    l->addWidget(chip(this, "IMU",   &m_imu));
    l->addWidget(chip(this, "STATE", &m_state));
    l->addStretch(1);

    m_clock = new QLabel(this);
    m_clock->setObjectName("mono");
    l->addWidget(m_clock);

    m_home   = new QPushButton("HOME", this);
    m_scan   = new QPushButton("SCAN", this);
    m_scan->setObjectName("accent");
    m_stop   = new QPushButton("STOP", this);
    m_disarm = new QPushButton("DISARM", this);
    m_disarm->setObjectName("powerOff");   // 항상 빨강 계열 — 비상정지

    connect(m_home,   &QPushButton::clicked, this, [this] {
        // DISARM 상태에선 이 버튼이 REARM 으로 바뀐다(계약 §5 "복구").
        // cmd/rearm 을 발행하고, 실제 IDLE 복귀는 데몬이 보내는 state/daemon 으로
        // 반영된다 — 로컬에서 먼저 바꾸지 않는다(거부될 수 있으므로).
        if (m_lastState == "DISARM") emit rearmRequested();
        else                          emit homeRequested();
    });
    connect(m_scan,   &QPushButton::clicked, this, &TopBar::scanRequested);
    connect(m_stop,   &QPushButton::clicked, this, &TopBar::stopRequested);
    connect(m_disarm, &QPushButton::clicked, this, &TopBar::disarmRequested);

    l->addWidget(m_home);
    l->addWidget(m_scan);
    l->addWidget(m_stop);
    l->addWidget(m_disarm);

    auto *t = new QTimer(this);
    connect(t, &QTimer::timeout, this, [this] {
        m_clock->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd  HH:mm:ss"));
    });
    t->start(1000);
    m_clock->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd  HH:mm:ss"));

    setBrokerConnected(false);
    setImu({});
    setDaemonState({});
}

void TopBar::setBrokerConnected(bool up) {
    m_mqtt->setText(up ? "CONNECTED" : "DISCONNECTED");
    m_mqtt->setStyleSheet(Theme::mono(10, 500) +
        QString("color:%1;").arg(up ? Theme::Ok.name() : Theme::DangerText.name()));
    m_mqttDot->setStyleSheet(QString("color:%1;font-size:7px;").arg(up ? Theme::Ok.name() : Theme::Danger.name()));
}

void TopBar::setImu(const ImuState &imu) {
    // 계약 §3.3: level.valid=false 면 IMU 미구현 — 값을 표시하지 않는다.
    if (!imu.valid) {
        m_imu->setText("N/A");
        m_imu->setStyleSheet(Theme::mono(10, 500) + QString("color:%1;").arg(Theme::TextFaint.name()));
        return;
    }
    m_imu->setText(QString("R %1° / P %2°").arg(imu.roll, 0, 'f', 1).arg(imu.pitch, 0, 'f', 1));
    m_imu->setStyleSheet(Theme::mono(10, 500) +
        QString("color:%1;").arg(imu.level() ? Theme::Ok.name() : Theme::DangerText.name()));
}

void TopBar::setDaemonState(const DaemonState &s) {
    m_state->setText(s.state);
    const QString color =
        (s.state == "IDLE")     ? Theme::Ok.name() :
        (s.state == "SCANNING") ? Theme::AccentBright.name() :
        (s.state == "EXPORT")   ? Theme::Warn.name() :
        (s.state == "DISARM")   ? Theme::DangerText.name() :
                                   Theme::TextFaint.name();   // OFFLINE
    m_state->setStyleSheet(Theme::mono(10, 700) + QString("color:%1;letter-spacing:1px;").arg(color));
    m_lastState = s.state;

    // 계약 §5 상태-버튼 매핑. DISARM 버튼만 예외적으로 항상 활성(비상정지).
    // DISARM 상태에선 HOME 버튼이 "복구(REARM)" 로 바뀌어 유일하게 활성화된다 —
    // 안 그러면 DISARM 이후 아무 버튼도 못 눌러 되돌아올 방법이 없다.
    if (s.state == "DISARM") {
        m_home->setText("REARM");
        m_home->setEnabled(true);
    } else {
        m_home->setText("HOME");
        m_home->setEnabled(s.state == "IDLE");
    }
    m_scan->setEnabled(s.state == "IDLE");
    m_stop->setEnabled(s.state == "SCANNING");
}
