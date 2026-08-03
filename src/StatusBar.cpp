#include "StatusBar.h"
#include "Theme.h"
#include <QHBoxLayout>
#include <QLabel>

StatusBar::StatusBar(QWidget *parent) : QFrame(parent) {
    setFixedHeight(26);
    setStyleSheet("background:#0e1317;border:none;border-top:1px solid #212a32;");

    auto *l = new QHBoxLayout(this);
    l->setContentsMargins(14, 0, 14, 0);
    l->setSpacing(16);

    auto mk = [this](const QString &text) {
        auto *lb = new QLabel(text, this);
        lb->setStyleSheet(Theme::mono(10) + "color:#5f6c78;");
        return lb;
    };

    m_power = mk("KIT POWER ON");
    l->addWidget(m_power);
    l->addWidget(mk("STM32 UART 921600 · OK"));
    l->addWidget(mk("RPi4B 42.3 °C"));
    l->addStretch(1);
    l->addWidget(mk("Qt 6 · CLion Build Debug-x64"));
}

void StatusBar::setPower(bool on) {
    m_power->setText(on ? "KIT POWER ON" : "KIT POWER OFF");
    m_power->setStyleSheet(Theme::mono(10) + QString("color:%1;").arg(on ? "#6fdcab" : "#ff9a92"));
}
