#pragma once
#include <QFrame>
#include "Models.h"

class QLabel;

// 하단 상태 바 (26px): 데몬 상태(state/daemon) · 링크 · 빌드 정보
class StatusBar : public QFrame {
    Q_OBJECT
public:
    explicit StatusBar(QWidget *parent = nullptr);
    void setDaemonState(const DaemonState &s);

private:
    QLabel *m_state, *m_link;
};
