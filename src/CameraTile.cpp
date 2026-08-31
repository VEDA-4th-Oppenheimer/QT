#include "CameraTile.h"
#include "Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QTimer>
#include <QDateTime>
#include <QMouseEvent>

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
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        if (!frame.isNull()) {
            QRect target(QPoint(0, 0), frame.size().scaled(size(), Qt::KeepAspectRatio));
            target.moveCenter(rect().center());
            if (target != rect()) p.fillRect(rect(), Theme::Panel);   // 레터박스 여백
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

    m_noLabel = new QLabel(QString("CH%1").arg(state.no), head);
    m_noLabel->setStyleSheet(Theme::mono(10, 700) + QString("color:%1;letter-spacing:1px;").arg(Theme::TextFaint.name()));
    m_name = new QLabel(state.name, head);
    m_name->setStyleSheet(QString("font-size:12px;color:%1;").arg(Theme::Text2.name()));
    m_name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_status = new QLabel(head);

    // 평소엔 숨어 있다가 1채널로 키웠을 때만 뜬다 — 지금 보고 있는 게 4분할이
    // 아니라는 걸 알려주고, 되돌리는 법도 같이 적어 둔다.
    m_solo = new QLabel(QString::fromUtf8("1채널"), head);
    m_solo->setStyleSheet(Theme::mono(9, 700) + QString(
        "color:%1;background:%2;border-radius:3px;padding:1px 5px;letter-spacing:1px;")
        .arg(Theme::AccentBright.name(), Theme::AccentBg.name()));
    m_solo->hide();

    // 현재 수신 중인 실시간 해상도 배지 (800x448 서브 vs 2592x1520 원본 4MP)
    m_resBadge = new QLabel(head);
    m_resBadge->setStyleSheet(Theme::mono(9, 600) + QString(
        "color:%1;background:#2a2b36;border:1px solid #3a3b48;border-radius:3px;padding:1px 5px;")
        .arg(Theme::TextFaint.name()));
    m_resBadge->hide();

    hl->addWidget(m_noLabel);
    hl->addWidget(m_name, 1);
    hl->addWidget(m_resBadge);
    hl->addWidget(m_solo);
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
        if (static_cast<VideoView *>(m_view)->frame.isNull()) m_view->update();
    });
    clock->start(1000);

    setCursor(Qt::PointingHandCursor);
    setToolTip(QString::fromUtf8("클릭하면 CH%1 만 크게 봅니다 (다시 클릭하면 4분할)").arg(state.no));

    setOnline(state.online);
    setFps(state.fps);
}

void CameraTile::setSolo(bool solo) {
    if (m_solo != nullptr) m_solo->setVisible(solo);
    setToolTip(solo
        ? QString::fromUtf8("클릭하면 4분할로 돌아갑니다")
        : QString::fromUtf8("클릭하면 CH%1 만 크게 봅니다 (다시 클릭하면 4분할)").arg(m_state.no));
}

void CameraTile::mouseReleaseEvent(QMouseEvent *ev) {
    // 누른 자리에서 뗀 것만 클릭으로 친다 — 스플리터를 끌다 손을 뗀 게 채널
    // 전환으로 오인되지 않게.
    if (ev->button() == Qt::LeftButton && rect().contains(ev->pos())) {
        emit channelClicked(m_state.no);
    }
    QFrame::mouseReleaseEvent(ev);
}

void CameraTile::setFrame(const QImage &img) {
    if (!img.isNull() && img.size() != m_currentFrameSize) {
        m_currentFrameSize = img.size();
        if (m_resBadge) {
            const bool isFull = (m_currentFrameSize.width() >= 1920);
            if (isFull) {
                m_resBadge->setText(QString("%1x%2 (4MP 원본)").arg(m_currentFrameSize.width()).arg(m_currentFrameSize.height()));
                m_resBadge->setStyleSheet(Theme::mono(9, 700) + QString(
                    "color:#4ade80;background:#064e3b;border:1px solid #059669;border-radius:3px;padding:1px 5px;"));
            } else {
                m_resBadge->setText(QString("%1x%2 (서브)").arg(m_currentFrameSize.width()).arg(m_currentFrameSize.height()));
                m_resBadge->setStyleSheet(Theme::mono(9, 600) + QString(
                    "color:%1;background:#2a2b36;border:1px solid #3a3b48;border-radius:3px;padding:1px 5px;")
                    .arg(Theme::TextFaint.name()));
            }
            m_resBadge->show();
        }
    }
    static_cast<VideoView *>(m_view)->frame = img;
    m_view->update();
}

void CameraTile::setOnline(bool online, bool keepFrame) {
    m_state.online = online;
    static_cast<VideoView *>(m_view)->online = online;
    updateHeaderStatus();
    if (!online && !keepFrame) {
        static_cast<VideoView *>(m_view)->frame = QImage();
        setDetectedObjects({});
    }
    m_view->update();
}

void CameraTile::setFps(double fps) {
    m_state.fps = fps;
    static_cast<VideoView *>(m_view)->fps = fps;
    m_view->update();
}

void CameraTile::setDetectedObjects(const QVector<SpatialObject> &objects) {
    int persons = 0;
    for (const auto &obj : objects) {
        if (obj.channel == m_state.no || obj.channel <= 0) {
            if (obj.cls.contains("PERSON", Qt::CaseInsensitive) || obj.cls.contains("HUMAN", Qt::CaseInsensitive)) {
                ++persons;
            }
        }
    }

    m_personCount = persons;
    m_hasPerson = (persons > 0);

    // 테두리 강조 효과: 사람이 인식되면 선명한 빨간색 테두리 적용
    if (m_hasPerson) {
        setStyleSheet(QString("QFrame#panel { border: 2.5px solid %1; border-radius: 6px; } "
                              "QFrame#panelHead { background: %2; border-top-left-radius: 4px; border-top-right-radius: 4px; }")
                      .arg(Theme::Danger.name(), Theme::DangerBg.name()));
        m_noLabel->setStyleSheet(Theme::mono(10, 700) + QString("color:%1;").arg(Theme::DangerText.name()));
    } else {
        setStyleSheet(QString());   // 원래 기본 QSS 테마로 복원
        m_noLabel->setStyleSheet(Theme::mono(10, 700) + QString("color:%1;letter-spacing:1px;").arg(Theme::TextFaint.name()));
    }

    updateHeaderStatus();
}

void CameraTile::updateHeaderStatus() {
    if (!m_state.online) {
        m_status->setText(QString::fromUtf8("● DISCONNECTED"));
        m_status->setStyleSheet(Theme::mono(10, 500) + QString("color:%1;letter-spacing:1px;").arg(Theme::DangerText.name()));
        return;
    }

    if (m_hasPerson) {
        m_status->setText(QString::fromUtf8("● PERSON (%1)").arg(m_personCount));
        m_status->setStyleSheet(Theme::mono(10, 700) + QString("color:%1;").arg(Theme::Danger.name()));
    } else {
        m_status->setText(QString::fromUtf8("● LIVE"));
        m_status->setStyleSheet(Theme::mono(10, 500) + QString("color:%1;").arg(Theme::Ok.name()));
    }
}
