#pragma once
#include <QFrame>
#include <QImage>
#include <QVector>
#include "Models.h"

class QLabel;
class QTimer;
class QMouseEvent;

// 2x2 그리드 한 칸: 헤더(채널번호/이름/연결상태/사람감지) + 영상 표시 영역 + BBox 오버레이
class CameraTile : public QFrame {
    Q_OBJECT
public:
    explicit CameraTile(const ChannelState &state, QWidget *parent = nullptr);

    void setFrame(const QImage &img);
    void setOnline(bool online);
    void setFps(double fps);
    void setDetectedObjects(const QVector<SpatialObject> &objects);

    int channel() const { return m_state.no; }

    // 이 타일만 크게 보고 있는 상태인지 표시한다(헤더에 "1채널" 배지).
    void setSolo(bool solo);

signals:
    // 타일 클릭 — MainWindow 가 이 채널만 남기고 나머지를 감춘다(다시 누르면 4분할).
    void channelClicked(int channel);

protected:
    void mouseReleaseEvent(QMouseEvent *ev) override;

private:
    void updateHeaderStatus();

    ChannelState m_state;
    QLabel  *m_noLabel;
    QLabel  *m_name;
    QLabel  *m_status;
    QWidget *m_view;
    QLabel  *m_solo = nullptr;
    bool     m_hasPerson = false;
    int      m_personCount = 0;
};
