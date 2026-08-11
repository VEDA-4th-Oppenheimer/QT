#include "TopViewWidget.h"
#include "Theme.h"
#include <QPainter>
#include <QPainterPath>
#include <cmath>

namespace {

// 포인트클라우드 높이 램프. 상태색(Ok/Warn/Danger)과 섞이면 "초록 점 = 정상"
// 처럼 읽히므로 일부러 다른 계열을 쓴다. 값은 웹 뷰어(SCAN INSPECT)와 동일 —
// 같은 스캔을 두 도구로 봐도 색이 어긋나지 않게 맞춰둔 것이다.
// t=0 이 높은 쪽(따뜻함), t=1 이 낮은 쪽(깊은 청색).
QColor cloudRamp(double t, bool dark) {
    static const double kDark[5][3] = {
        {1.00, 0.94, 0.76}, {1.00, 0.77, 0.42}, {0.34, 0.78, 0.72},
        {0.17, 0.51, 0.69}, {0.10, 0.23, 0.45}
    };
    static const double kLight[5][3] = {
        {0.85, 0.63, 0.13}, {0.88, 0.48, 0.16}, {0.08, 0.57, 0.53},
        {0.10, 0.42, 0.61}, {0.09, 0.19, 0.42}
    };
    const auto &S = dark ? kDark : kLight;
    const double u = qBound(0.0, t, 1.0) * 4.0;
    const int i = qMin(3, static_cast<int>(u));
    const double f = u - i;
    return QColor::fromRgbF(S[i][0] + (S[i + 1][0] - S[i][0]) * f,
                            S[i][1] + (S[i + 1][1] - S[i][1]) * f,
                            S[i][2] + (S[i + 1][2] - S[i][2]) * f);
}

constexpr int kCloudBands = 6;   // 밴드 수 — 점마다 펜을 바꾸는 대신 묶어서 그린다

}   // namespace

TopViewWidget::TopViewWidget(QWidget *parent) : QWidget(parent) {
    // 세로가 모자랄 때 잘려나가는 건 아래쪽 통계바다. 지도가 먼저 양보하도록
    // 하한을 낮게 잡는다 — 200 이면 방 외곽과 점군 분포는 읽힌다.
    setMinimumSize(320, 200);
}

void TopViewWidget::setRoomSize(double w, double d) { m_roomW = w; m_roomD = d; update(); }
void TopViewWidget::setEdges(const QVector<MapEdge> &e) { m_edges = e; update(); }
void TopViewWidget::setObjects(const QVector<SpatialObject> &o) { m_objects = o; update(); }

void TopViewWidget::setScanCloud(const ScanCloud &c) { m_cloud = c; update(); }
void TopViewWidget::clearScanCloud() { m_cloud = ScanCloud(); update(); }

QRectF TopViewWidget::viewRectM() const {
    QRectF r(-m_roomW / 2.0, -m_roomD / 2.0, m_roomW, m_roomD);
    if (!m_cloud.isEmpty()) {
        // 스캔은 방보다 넓게 찍히는 일이 흔하다(문 밖까지 보임). 방에 고정하면
        // 바깥 점이 잘려 "스캔이 덜 됐다"처럼 보인다. 반대로 점군만 담으면 킷
        // 사분면(FOV)이 잘려나가므로 둘의 합집합을 쓴다.
        const QRectF cloud(m_cloud.xMin - 0.3, m_cloud.zMin - 0.3,
                           (m_cloud.xMax - m_cloud.xMin) + 0.6,
                           (m_cloud.zMax - m_cloud.zMin) + 0.6);
        r = r.united(cloud);
    }
    return r;
}

double TopViewWidget::pxPerM() const {
    const double margin = 28.0;
    const QRectF r = viewRectM();
    // x·y 배율은 반드시 같아야 한다 — 축마다 다르게 늘리면 방 모양이 왜곡돼
    // 지도가 거짓 정보를 준다. 그래서 두 축 중 빡빡한 쪽에 맞춘다.
    return qMin((width() - 2 * margin) / qMax(0.1, r.width()),
                (height() - 2 * margin) / qMax(0.1, r.height()));
}

QPointF TopViewWidget::toPx(const QPointF &m) const {
    // +x = 동쪽(오른쪽), +y = 북쪽(위). 위젯 중앙 = 뷰 영역의 중심이다 —
    // 점군이 원점 기준으로 치우쳐 있으면 킷은 중앙에서 벗어난다. 그게 맞다:
    // 킷을 억지로 가운데 두면 반대쪽 여백만 늘고 배율이 깎인다.
    const double s = pxPerM();
    const QPointF c = viewRectM().center();
    return QPointF(width() / 2.0 + (m.x() - c.x()) * s,
                   height() / 2.0 - (m.y() - c.y()) * s);
}

QPointF TopViewWidget::toM(const QPointF &px) const {
    const double s = pxPerM();
    const QPointF c = viewRectM().center();
    return QPointF(c.x() + (px.x() - width() / 2.0) / s,
                   c.y() - (px.y() - height() / 2.0) / s);
}

void TopViewWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Theme::MapBg);

    const double s = pxPerM();
    const QPointF origin = toPx(QPointF(0, 0));

    // 1 m 격자 — 뷰 영역이 아니라 "위젯 전체"를 덮는다. 배율을 두 축에 같이
    // 맞추면 남는 쪽에 슬랙이 생기는데, 격자를 뷰 영역까지만 그리면 그 슬랙이
    // 빈 띠로 남아 잘린 것처럼 보인다.
    const QPointF mTL = toM(QPointF(0, 0));
    const QPointF mBR = toM(QPointF(width(), height()));
    const double gx0 = std::floor(mTL.x()), gx1 = std::ceil(mBR.x());
    const double gy0 = std::floor(mBR.y()), gy1 = std::ceil(mTL.y());
    p.setPen(QPen(Theme::Grid, 1));
    for (double x = gx0; x <= gx1 + 0.01; x += 1.0) {
        p.drawLine(toPx({x, gy0}), toPx({x, gy1}));
    }
    for (double y = gy0; y <= gy1 + 0.01; y += 1.0) {
        p.drawLine(toPx({gx0, y}), toPx({gx1, y}));
    }

    // 채널별 FOV 사분면(CH1=북 … CH4=서)은 그리지 않는다. 4채널이 실제로 어느
    // 방향을 보는지가 설치마다 다른데, 사분면과 라벨은 그것이 확정된 것처럼
    // 읽힌다 — 지도 위의 방위와 채널 번호를 잘못 짝지어 해석하게 된다.
    // 킷 자체의 위치(아래 원점 마커)는 천장 중앙으로 고정이라 그대로 표시한다.

    // 실측 포인트클라우드 (에지·킷보다 아래에 깔린다)
    paintCloud(p);

    // LiDAR 벽/기둥 에지
    p.setPen(QPen(Theme::Wall, 2));
    for (const auto &e : m_edges) p.drawLine(toPx(e.a), toPx(e.b));
    if (m_edges.isEmpty() && m_cloud.isEmpty()) {   // 스캔 전: 방 외곽만
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

    // 지도 위에는 글자를 얹지 않는다. 킷 라벨("4CH CAM + PAN-TILT" / 천장고),
    // 감지 객체의 거리 라벨, 스케일 바 수치를 모두 뺐다 — 점군이 깔리면 라벨이
    // 묻혀서 뒤에 판까지 깔아야 했고, 그렇게 겹쳐 놓아도 읽히지 않았다.
    // 형태(킷 마커·객체 점·스케일 바)만 남긴다.

    // WiseAI 감지 객체
    for (const auto &o : m_objects) {
        const QPointF c = toPx(o.posM);
        const QColor col = (o.cls == "VEHICLE") ? Theme::Ok : Theme::Warn;
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawEllipse(c, 6, 6);
    }
    p.setBrush(Qt::NoBrush);

    // 스케일 바도 뺐다 — 수치를 지운 뒤로는 길이를 읽을 근거가 없어서 선만 남으면
    // 아무 뜻도 없다. 거리 감각은 1 m 간격 격자가 대신한다.
}

void TopViewWidget::paintCloud(QPainter &p) const {
    if (m_cloud.isEmpty()) return;

    const bool dark = (Theme::CurrentMode == Theme::Mode::Developer);
    const double h0 = m_cloud.hMin, h1 = m_cloud.hMax;
    const double inv = (h1 - h0) > 1e-6 ? 1.0 / (h1 - h0) : 0.0;
    const int n = m_cloud.floor.size();

    // 높이 밴드로 묶어 밴드당 drawPoints 한 번씩만 부른다. 점마다 setPen 하면
    // 3만 점에서 프레임이 눈에 띄게 끊긴다.
    m_pxScratch.resize(n);
    for (int band = 0; band < kCloudBands; ++band) {
        int k = 0;
        for (int i = 0; i < n; ++i) {
            const double t = (m_cloud.height[i] - h0) * inv;
            const int b = qBound(0, static_cast<int>(t * kCloudBands), kCloudBands - 1);
            if (b != band) continue;
            m_pxScratch[k++] = toPx(m_cloud.floor[i]);
        }
        if (k == 0) continue;
        // 밴드 인덱스가 클수록 높은 쪽 → 램프는 t=0 이 높은 쪽이라 뒤집어 넣는다.
        const double rampT = 1.0 - static_cast<double>(band) / (kCloudBands - 1);
        QColor c = cloudRamp(rampT, dark);
        c.setAlphaF(0.85);
        p.setPen(QPen(c, 1.6));
        p.drawPoints(m_pxScratch.constData(), k);
    }
}
