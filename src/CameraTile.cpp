#include "CameraTile.h"
#include "Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QTimer>
#include <QDateTime>

namespace {
class VideoView : public QWidget {
public:
    explicit VideoView(QWidget *p = nullptr) : QWidget(p) {}
    QImage  frame;
    QString topic;
    QString meta;
    bool    online = false;
    double  fps = 0.0;

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        if (!frame.isNull()) {
            // 예전 코드는 frame.scaled(size(), KeepAspectRatio, Smooth) 로 프레임마다
            // 전체 크기 QImage 를 새로 할당한 뒤, 그걸 다시 drawImage(rect(), ...) 로
            // 늘려 그렸다. 스케일을 두 번 하는 데다(4채널 × 초당 수십 프레임이라
            // 부담이 크다) KeepAspectRatio 로 맞춰둔 결과를 rect() 에 늘려버려
            // 종횡비가 도로 깨지는 문제까지 있었다.
            // 여기서는 종횡비를 지킨 목적 사각형을 직접 구해 한 번에 그린다 —
            // 중간 QImage 할당이 사라지고 스케일도 1회로 줄어든다.
            QRect target(QPoint(0, 0), frame.size().scaled(size(), Qt::KeepAspectRatio));
            target.moveCenter(rect().center());
            if (target != rect()) p.fillRect(rect(), Theme::Panel);   // 레터박스 여백
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.drawImage(target, frame);
            return;
        }
        // 스트림 없음: 시안의 45° 스트라이프 플레이스홀더
        p.fillRect(rect(), Theme::Panel);
        p.setPen(QPen(Theme::PanelHead, 9));
        for (int x = -height(); x < width(); x += 18)
            p.drawLine(x, height(), x + height(), 0);

        QFont f("JetBrains Mono");
        f.setPixelSize(10);
        p.setFont(f);
        p.setPen(Theme::TextFaint2);
        p.drawText(QRect(0, height() / 2 - 16, width(), 14), Qt::AlignCenter, topic);

        f.setPixelSize(9);
        p.setFont(f);
        p.setPen(Theme::TextGhost);
        p.drawText(QRect(0, height() / 2, width(), 14), Qt::AlignCenter, meta);

        f.setPixelSize(10);
        p.setFont(f);
        p.setPen(Theme::TextFaint2);
        if (online)
            p.drawText(QRect(8, height() - 18, width() / 2, 14), Qt::AlignLeft | Qt::AlignVCenter,
                       QDateTime::currentDateTime().toString("HH:mm:ss"));
        const QString fpsText = fps > 0 ? QString::number(fps, 'f', 1) + " fps" : QString::fromUtf8("— fps");
        p.drawText(QRect(width() / 2, height() - 18, width() / 2 - 8, 14), Qt::AlignRight | Qt::AlignVCenter, fpsText);
    }
};
}

CameraTile::CameraTile(const ChannelState &state, QWidget *parent)
    : QFrame(parent), m_state(state) {
    setObjectName("panel");

    auto *head = new QFrame(this);
    head->setObjectName("panelHead");
    head->setFixedHeight(30);
    auto *hl = new QHBoxLayout(head);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(9);

    auto *no = new QLabel(QString("CH%1").arg(state.no), head);
    no->setStyleSheet(Theme::mono(10, 700) + QString("color:%1;letter-spacing:1px;").arg(Theme::TextFaint.name()));
    m_name = new QLabel(state.name, head);
    m_name->setStyleSheet(QString("font-size:12px;color:%1;").arg(Theme::Text2.name()));
    m_name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_status = new QLabel(head);

    hl->addWidget(no);
    hl->addWidget(m_name, 1);
    hl->addWidget(m_status);

    auto *view = new VideoView(this);
    static_cast<VideoView *>(view)->topic = state.topic;
    static_cast<VideoView *>(view)->meta  = state.meta;
    m_view = view;

    auto *vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
    vl->addWidget(head);
    vl->addWidget(view, 1);

    auto *clock = new QTimer(this);
    connect(clock, &QTimer::timeout, this, [this] {
        // 이 타이머는 플레이스홀더의 시계/fps 표시를 갱신하려고 있는 것이다.
        // 영상이 들어오는 동안에는 setFrame() 이 프레임마다 update() 를 부르고
        // 시계도 그려지지 않으므로, 여기서 또 다시 그리면 스케일 연산만 낭비된다.
        if (static_cast<VideoView *>(m_view)->frame.isNull()) m_view->update();
    });
    clock->start(1000);

    setOnline(state.online);
    setFps(state.fps);
}

void CameraTile::setFrame(const QImage &img) {
    static_cast<VideoView *>(m_view)->frame = img;
    m_view->update();
}

void CameraTile::setOnline(bool online) {
    m_state.online = online;
    static_cast<VideoView *>(m_view)->online = online;
    m_status->setText(online ? QString::fromUtf8("● LIVE") : QString::fromUtf8("● DISCONNECTED"));
    m_status->setStyleSheet(Theme::mono(10, 500) +
        QString("color:%1;letter-spacing:1px;").arg(online ? Theme::Ok.name() : Theme::DangerText.name()));
    if (!online) { static_cast<VideoView *>(m_view)->frame = QImage(); }
    m_view->update();
}

void CameraTile::setFps(double fps) {
    m_state.fps = fps;
    static_cast<VideoView *>(m_view)->fps = fps;
    m_view->update();
}
