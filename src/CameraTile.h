#pragma once
#include <QFrame>
#include <QImage>
#include "Models.h"

class QLabel;
class QTimer;

// 2x2 그리드 한 칸: 헤더(채널번호/이름/연결상태) + 영상 표시 영역(플레이스홀더)
class CameraTile : public QFrame {
    Q_OBJECT
public:
    explicit CameraTile(const ChannelState &state, QWidget *parent = nullptr);

    void setFrame(const QImage &img);
    void setOnline(bool online);
    void setFps(double fps);

private:
    ChannelState m_state;
    QLabel  *m_name, *m_status;
    QWidget *m_view;
};
