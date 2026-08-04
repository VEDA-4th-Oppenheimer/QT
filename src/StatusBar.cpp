#include "StatusBar.h"
#include "Theme.h"
#include <QHBoxLayout>
#include <QLabel>

StatusBar::StatusBar(QWidget *parent) : QFrame(parent) {
    setFixedHeight(26);
    setStyleSheet(QString("background:%1;border:none;border-top:1px solid %2;")
        .arg(Theme::BarBg.name(), Theme::Border.name()));

    auto *l = new QHBoxLayout(this);
    l->setContentsMargins(14, 0, 14, 0);
    l->setSpacing(16);

    auto mk = [this](const QString &text) {
        auto *lb = new QLabel(text, this);
        lb->setStyleSheet(Theme::mono(10) + QString("color:%1;").arg(Theme::TextFaint.name()));
        return lb;
    };

    m_state = mk("KIT OFFLINE");
    l->addWidget(m_state);
    m_link = mk("STM32 link —");
    l->addWidget(m_link);
    l->addStretch(1);
    l->addWidget(mk("Qt 6 · CLion Build Debug-x64"));
}

void StatusBar::setDaemonState(const DaemonState &s) {
    m_state->setText(QString("KIT %1").arg(s.state));
    const QString color =
        (s.state == "IDLE" || s.state == "SCANNING") ? Theme::OkBright.name() :
        (s.state == "DISARM" || s.state == "OFFLINE") ? Theme::DangerText.name() : Theme::Warn.name();
    m_state->setStyleSheet(Theme::mono(10) + QString("color:%1;").arg(color));

    m_link->setText(QString("STM32 link %1").arg(s.linkAlive ? "OK" : "DOWN"));
    m_link->setStyleSheet(Theme::mono(10) + QString("color:%1;")
        .arg((s.linkAlive ? Theme::TextFaint : Theme::DangerText).name()));
}
