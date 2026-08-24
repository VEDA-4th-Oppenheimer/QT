#pragma once
#include <QFrame>
#include <QImage>
#include <QVector>
#include "Models.h"

class QLabel;
class QTimer;

// 2x2 그리드 한 칸: 헤더(채널번호/이름/연결상태/사람감지) + 영상 표시 영역 + BBox 오버레이
class CameraTile : public QFrame {
    Q_OBJECT
public:
    explicit CameraTile(const ChannelState &state, QWidget *parent = nullptr);

    void setFrame(const QImage &img);
    void setOnline(bool online);
    void setFps(double fps);
    void setDetectedObjects(const QVector<SpatialObject> &objects);

private:
    void updateHeaderStatus();

    ChannelState m_state;
    QLabel  *m_noLabel;
    QLabel  *m_name;
    QLabel  *m_status;
    QWidget *m_view;
    bool     m_hasPerson = false;
    int      m_personCount = 0;
};
