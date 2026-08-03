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
            p.drawImage(rect(), frame.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            return;
        }
        // 스트림 없음: 시안의 45° 스트라이프 플레이스홀더
        p.fillRect(rect(), QColor("#0f1418"));
        p.setPen(QPen(QColor("#12171b"), 9));
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
    no->setStyleSheet(Theme::mono(10, 700) + "color:#5f6c78;letter-spacing:1px;");
    m_name = new QLabel(state.name, head);
    m_name->setStyleSheet("font-size:12px;color:#dbe2e8;");
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
    connect(clock, &QTimer::timeout, this, [this] { m_view->update(); });
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
