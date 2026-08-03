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
    k->setStyleSheet(Theme::mono(10, 500) + "color:#8e9aa5;letter-spacing:1px;");
    l->addWidget(k);
    if (dot) {
        *dot = pulseDot(f, Theme::TextFaint);
        l->addWidget(*dot);
    }
    *value = new QLabel(f);
    (*value)->setStyleSheet(Theme::mono(10, 500) + "color:#e4e9ee;");
    l->addWidget(*value);
    return f;
}
}

TopBar::TopBar(QWidget *parent) : QFrame(parent) {
    setFixedHeight(54);
    setStyleSheet("background:#11161b;border-bottom:1px solid #212a32;");

    auto *l = new QHBoxLayout(this);
    l->setContentsMargins(18, 0, 18, 0);
    l->setSpacing(16);

    auto *title = new QLabel(QString::fromUtf8("SPATIAL·VMS"), this);
    title->setStyleSheet(Theme::mono(14, 700) + "letter-spacing:2px;color:#e4e9ee;");
    auto *sub = new QLabel("v0.4 / INDOOR 3D MAPPING KIT", this);
    sub->setStyleSheet(Theme::mono(10) + "letter-spacing:2px;color:#5f6c78;");

    l->addWidget(title);
    l->addWidget(sub);
    l->addWidget(chip(this, "MQTT",  &m_mqtt, &m_mqttDot));
    l->addWidget(chip(this, "IMU",   &m_imu));
    l->addWidget(chip(this, "CALIB", &m_calib));
    l->addStretch(1);

    m_clock = new QLabel(this);
    m_clock->setObjectName("mono");
    l->addWidget(m_clock);

    auto *calibrate = new QPushButton("CALIBRATE", this);
    calibrate->setObjectName("accent");
    auto *rescan = new QPushButton("RE-SCAN", this);
    m_power = new QPushButton(this);

    connect(calibrate, &QPushButton::clicked, this, &TopBar::calibrateRequested);
    connect(rescan,    &QPushButton::clicked, this, &TopBar::rescanRequested);
    connect(m_power,   &QPushButton::clicked, this, [this] {
        setPower(!m_powerOn);
        emit powerToggled(m_powerOn);
    });

    l->addWidget(calibrate);
    l->addWidget(rescan);
    l->addWidget(m_power);

    auto *t = new QTimer(this);
    connect(t, &QTimer::timeout, this, [this] {
        m_clock->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd  HH:mm:ss"));
    });
    t->start(1000);
    m_clock->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd  HH:mm:ss"));

    setBrokerConnected(false);
    setImu({});
    setCalib({});
    setPower(true);
}

void TopBar::setBrokerConnected(bool up) {
    m_mqtt->setText(up ? "CONNECTED  192.168.0.42:1883" : "DISCONNECTED");
    m_mqtt->setStyleSheet(Theme::mono(10, 500) +
        QString("color:%1;").arg(up ? Theme::Ok.name() : Theme::DangerText.name()));
    m_mqttDot->setStyleSheet(QString("color:%1;font-size:7px;").arg(up ? Theme::Ok.name() : Theme::Danger.name()));
}

void TopBar::setImu(const ImuState &imu) {
    m_imu->setText(QString("R %1° / P %2°").arg(imu.roll, 0, 'f', 1).arg(imu.pitch, 0, 'f', 1));
    m_imu->setStyleSheet(Theme::mono(10, 500) +
        QString("color:%1;").arg(imu.level() ? Theme::Ok.name() : Theme::DangerText.name()));
}

void TopBar::setCalib(const CalibState &c) {
    m_calib->setText(QString("%1%  retry %2/%3").arg(c.progress).arg(c.retry).arg(c.maxRetry));
}

void TopBar::setPower(bool on) {
    m_powerOn = on;
    m_power->setText(on ? "POWER ON" : "POWER OFF");
    m_power->setObjectName(on ? "powerOn" : "powerOff");
    m_power->style()->unpolish(m_power);
    m_power->style()->polish(m_power);
}
