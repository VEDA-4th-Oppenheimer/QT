#include "TopViewWidget.h"
#include "Theme.h"
#include <QPainter>
#include <QPainterPath>

TopViewWidget::TopViewWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(360, 300);
}

void TopViewWidget::setRoomSize(double w, double d) { m_roomW = w; m_roomD = d; update(); }
void TopViewWidget::setEdges(const QVector<MapEdge> &e) { m_edges = e; update(); }
void TopViewWidget::setObjects(const QVector<SpatialObject> &o) { m_objects = o; update(); }

double TopViewWidget::pxPerM() const {
    const double margin = 28.0;
    return qMin((width() - 2 * margin) / m_roomW, (height() - 2 * margin) / m_roomD);
}

QPointF TopViewWidget::toPx(const QPointF &m) const {
    // +x = 동쪽(오른쪽), +y = 북쪽(위). 위젯 중앙이 킷 원점.
    const double s = pxPerM();
    return QPointF(width() / 2.0 + m.x() * s, height() / 2.0 - m.y() * s);
}

void TopViewWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Theme::MapBg);

    const double s = pxPerM();
    const QPointF origin = toPx(QPointF(0, 0));

    // 1 m 격자
    p.setPen(QPen(QColor("#151d23"), 1));
    for (double x = -m_roomW / 2; x <= m_roomW / 2 + 0.01; x += 1.0) {
        p.drawLine(toPx({x, -m_roomD / 2}), toPx({x, m_roomD / 2}));
    }
    for (double y = -m_roomD / 2; y <= m_roomD / 2 + 0.01; y += 1.0) {
        p.drawLine(toPx({-m_roomW / 2, y}), toPx({m_roomW / 2, y}));
    }

    // 4채널 FOV 사분면
    const QPointF corners[4] = {
        toPx({-m_roomW / 2,  m_roomD / 2}), toPx({ m_roomW / 2,  m_roomD / 2}),
        toPx({ m_roomW / 2, -m_roomD / 2}), toPx({-m_roomW / 2, -m_roomD / 2})
    };
    const double alpha[4] = {0.07, 0.05, 0.07, 0.03};   // CH1(N) CH2(E) CH3(S) CH4(W)
    const char *fovLabel[4] = {"CH1 FOV", "CH2 FOV", "CH3 FOV", "CH4 FOV"};
    QFont fovFont("JetBrains Mono"); fovFont.setPixelSize(9);
    p.setFont(fovFont);
    for (int i = 0; i < 4; ++i) {
        QPainterPath wedge(origin);
        wedge.lineTo(corners[i]);
        wedge.lineTo(corners[(i + 1) % 4]);
        wedge.closeSubpath();
        QColor c = Theme::Accent; c.setAlphaF(alpha[i]);
        p.fillPath(wedge, c);
        p.setPen(QPen(QColor("#2a5f68"), 1, Qt::DashLine));
        p.drawLine(origin, corners[i]);

        const QPointF mid = (corners[i] + corners[(i + 1) % 4]) / 2.0;
        p.setPen(QColor("#3d6470"));
        p.drawText(QRectF(mid.x() - 40, mid.y() - 7, 80, 14), Qt::AlignCenter, fovLabel[i]);
    }

    // LiDAR 벽/기둥 에지
    p.setPen(QPen(Theme::Wall, 2));
    for (const auto &e : m_edges) p.drawLine(toPx(e.a), toPx(e.b));
    if (m_edges.isEmpty()) {   // 스캔 전: 방 외곽만
        const QRectF room(toPx({-m_roomW / 2, m_roomD / 2}), toPx({m_roomW / 2, -m_roomD / 2}));
        p.drawRect(room);
    }

    // 킷 (천장 중앙: 4CH 카메라 + Pan-Tilt LiDAR)
    p.setPen(Qt::NoPen);
    p.setBrush(Theme::Accent);
    p.drawEllipse(origin, 8, 8);
    p.setBrush(Qt::NoBrush);
    QColor ring1 = Theme::Accent; ring1.setAlphaF(0.45);
    p.setPen(QPen(ring1, 1));
    p.drawEllipse(origin, 17, 17);
    QColor ring2 = Theme::Accent; ring2.setAlphaF(0.18);
    p.setPen(QPen(ring2, 1));
    p.drawEllipse(origin, 30, 30);

    QFont mono("JetBrains Mono"); mono.setPixelSize(9); p.setFont(mono);
    p.setPen(Theme::TextDim);
    p.drawText(QRectF(origin.x() - 90, origin.y() - 34, 180, 14),
               Qt::AlignCenter, QStringLiteral("4CH CAM + PAN-TILT"));
    mono.setPixelSize(8); p.setFont(mono);
    p.setPen(Theme::TextFaint2);
    p.drawText(QRectF(origin.x() - 90, origin.y() + 20, 180, 12),
               Qt::AlignCenter, QStringLiteral("CEILING H 2.85 m"));

    // WiseAI 감지 객체 (실거리 라벨)
    mono.setPixelSize(10); p.setFont(mono);
    for (const auto &o : m_objects) {
        const QPointF c = toPx(o.posM);
        const QColor col = (o.cls == "VEHICLE") ? Theme::Ok : Theme::Warn;
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawEllipse(c, 6, 6);
        p.setPen(col);
        p.drawText(c + QPointF(12, 4), QString("%1 %2 m").arg(o.cls).arg(o.distM, 0, 'f', 1));
    }

    // 좌하단 스케일 바
    p.setPen(QPen(Theme::TextFaint, 2));
    p.drawLine(QPointF(20, height() - 18), QPointF(20 + 2 * s, height() - 18));
    p.setPen(Theme::TextFaint);
    p.drawText(QPointF(24 + 2 * s, height() - 14),
               QString("2 m · ROOM %1 × %2 m").arg(m_roomW, 0, 'f', 1).arg(m_roomD, 0, 'f', 1));
}
