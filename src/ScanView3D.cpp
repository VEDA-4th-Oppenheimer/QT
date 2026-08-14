#include "ScanView3D.h"
#include "Theme.h"

#include <QMatrix4x4>
#include <QMouseEvent>
#include <QPainter>
#include <QVector3D>
#include <QVector4D>
#include <QWheelEvent>
#include <cmath>

namespace {

// ScanCloud 의 평면 좌표(floor.y = PCD +z, 전방/북)를 GL 월드 z 로 옮긴다.
//
// 부호를 뒤집는 이유 — 점군 프레임은 x=오른쪽(동) / z=전방(북) / 위=+ 이고,
// 이걸 GL 에 (x, 위, z) 로 그대로 얹으면 (동, 위, 북) 이 된다. 실세계에서
// 동×위 = -북 이므로 이 조합은 **왼손 좌표계**다. OpenGL 은 오른손 기준이라
// 그대로 그리면 화면이 좌우로 뒤집혀 나온다(방위각 0°에서 북쪽에 서서 남쪽을
// 보는데 동쪽이 오른편에 보이는 식).
//
// z 를 남쪽으로 잡으면 (동, 위, 남) 이 되어 동×위 = 남 = +z, 오른손계가 된다.
// Top-View(2D)는 자체 좌표로 그리므로 영향받지 않는다.
inline float worldZ(double cloudZ) { return static_cast<float>(-cloudZ); }

// GLSL 120 — QOpenGLWidget 기본 표면(호환 프로파일 2.1)에서 그대로 돈다.
// macOS 에서 코어 프로파일을 요구하면 3.2+ 셰이더로 갈아야 하는데, 점만 찍는
// 데는 이득이 없어서 호환 쪽에 맞췄다.
const char *kVert = R"(#version 120
attribute vec3 aPos;
attribute float aT;
uniform mat4 uMV, uProj;
uniform float uSize;
uniform vec3 uR0, uR1, uR2, uR3, uR4;
varying vec3 vCol;

vec3 rampc(float t) {
    float u = clamp(t, 0.0, 1.0) * 4.0;
    if (u < 1.0) return mix(uR0, uR1, u);
    if (u < 2.0) return mix(uR1, uR2, u - 1.0);
    if (u < 3.0) return mix(uR2, uR3, u - 2.0);
    return mix(uR3, uR4, u - 3.0);
}

void main() {
    vec4 mv = uMV * vec4(aPos, 1.0);
    gl_Position = uProj * mv;
    vCol = rampc(aT);
    // 가까울수록 크게 — 원근감 없이 균일하게 찍으면 깊이가 안 읽힌다.
    gl_PointSize = uSize * (2.6 / max(0.35, -mv.z));
}
)";

const char *kFrag = R"(#version 120
varying vec3 vCol;
void main() {
    vec2 d = gl_PointCoord - vec2(0.5);
    if (dot(d, d) > 0.25) discard;   // 사각형 대신 동그란 점
    gl_FragColor = vec4(vCol, 1.0);
}
)";

// TopViewWidget::cloudRamp 와 같은 값 — 2D 지도, 3D 뷰, 웹 뷰어가 모두 같은 색이다.
struct Ramp { float v[5][3]; };
Ramp rampFor(bool dark) {
    if (dark) {
        return Ramp{{{1.00f,0.94f,0.76f},{1.00f,0.77f,0.42f},{0.34f,0.78f,0.72f},
                     {0.17f,0.51f,0.69f},{0.10f,0.23f,0.45f}}};
    }
    return Ramp{{{0.85f,0.63f,0.13f},{0.88f,0.48f,0.16f},{0.08f,0.57f,0.53f},
                 {0.10f,0.42f,0.61f},{0.09f,0.19f,0.42f}}};
}

const char *kLineVert = R"(#version 120
attribute vec3 aPos;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(aPos, 1.0); }
)";

const char *kLineFrag = R"(#version 120
uniform vec4 uColor;
void main() { gl_FragColor = uColor; }
)";

constexpr float kPointSize = 2.2f;

}   // namespace

ScanView3D::ScanView3D(QWidget *parent) : QOpenGLWidget(parent) {
    setMinimumHeight(200);   // TopViewWidget 과 같은 이유 — 좁으면 뷰가 먼저 양보한다
    setFocusPolicy(Qt::StrongFocus);
}

ScanView3D::~ScanView3D() {
    if (!m_glReady) return;
    makeCurrent();
    m_vbo.destroy();
    doneCurrent();
}

void ScanView3D::setScanCloud(const ScanCloud &c) {
    m_cloud = c;
    m_pending.clear();
    m_count = 0;

    if (!c.isEmpty()) {
        const int n = c.floor.size();
        m_pending.reserve(n * 4);
        const double h0 = c.hMin, h1 = c.hMax;
        const double hInv = (h1 - h0) > 1e-6 ? 1.0 / (h1 - h0) : 0.0;

        double dMax = 1e-6;
        if (m_colorBy == ColorBy::Distance) {
            for (int i = 0; i < n; ++i) {
                const double d = std::sqrt(c.floor[i].x() * c.floor[i].x() +
                                           c.height[i] * c.height[i] +
                                           c.floor[i].y() * c.floor[i].y());
                if (d > dMax) dMax = d;
            }
        }

        for (int i = 0; i < n; ++i) {
            const float x = static_cast<float>(c.floor[i].x());
            const float y = c.height[i];
            const float z = worldZ(c.floor[i].y());
            float t;
            if (m_colorBy == ColorBy::Height) {
                // 램프는 t=0 이 높은 쪽이라 뒤집는다.
                t = static_cast<float>(1.0 - (y - h0) * hInv);
            } else {
                t = static_cast<float>(std::sqrt(x * x + y * y + z * z) / dMax);
            }
            m_pending << x << y << z << t;
        }
        m_count = n;
    }

    m_dirty = true;
    resetView();
    update();
}

void ScanView3D::setColorBy(ColorBy mode) {
    if (m_colorBy == mode) return;
    m_colorBy = mode;
    setScanCloud(m_cloud);   // t 값을 다시 계산해 올린다
}

void ScanView3D::resetView() {
    m_az = -35.0f;
    m_el = 26.0f;
    if (m_cloud.isEmpty()) {
        m_radius = 12.0f;
        m_target = QVector3D(0, 0, 0);
        return;
    }
    m_target = QVector3D(static_cast<float>((m_cloud.xMin + m_cloud.xMax) / 2.0),
                         static_cast<float>((m_cloud.hMin + m_cloud.hMax) / 2.0),
                         worldZ((m_cloud.zMin + m_cloud.zMax) / 2.0));
    // 바운딩 스피어를 세로 화각(50°)에 맞춘다. 1.55배는 빈 모서리까지 감싸느라
    // 실제 점군이 화면의 절반 남짓밖에 안 찼다 — 조금 잘리더라도 크게 보는 편이
    // 스캔 확인에는 낫다.
    const double sx = m_cloud.xMax - m_cloud.xMin;
    const double sy = m_cloud.hMax - m_cloud.hMin;
    const double sz = m_cloud.zMax - m_cloud.zMin;
    const double sphere = 0.5 * std::sqrt(sx * sx + sy * sy + sz * sz);
    m_radius = static_cast<float>(qMax(1.0, sphere / std::sin(qDegreesToRadians(25.0)) * 0.72));
}

void ScanView3D::initializeGL() {
    initializeOpenGLFunctions();
    m_glReady = true;

    // 이 함수는 한 번만 도는 게 아니다 — 위젯이 다른 창으로 옮겨가면(TOP-VIEW
    // 전체화면) 이전 GL 컨텍스트가 버려지고 새 컨텍스트로 다시 불린다. 셰이더
    // 프로그램은 만들어진 컨텍스트에 묶여 있어서 재사용하면
    // "Program and shader are not associated with same context" 로 링크가 깨지고
    // (그냥 다시 add 하면 vertex/fragment 가 두 벌 붙어 "duplicate definition of
    // function 'main'" 이 된다) 3D 뷰가 빈 화면이 된다. 컨텍스트마다 새로 만든다.
    m_prog = std::make_unique<QOpenGLShaderProgram>();
    m_lineProg = std::make_unique<QOpenGLShaderProgram>();
    // 버퍼도 같은 이유로 갈아끼운다. 점군 원본은 m_pending 에 남아 있어서
    // 아래 m_dirty = true 로 다음 paintGL 때 새 버퍼에 다시 올라간다.
    if (m_vbo.isCreated()) m_vbo.destroy();
    if (m_gridVbo.isCreated()) m_gridVbo.destroy();

    if (!m_prog->addShaderFromSourceCode(QOpenGLShader::Vertex, kVert) ||
        !m_prog->addShaderFromSourceCode(QOpenGLShader::Fragment, kFrag) ||
        !m_prog->link()) {
        qWarning("ScanView3D: 셰이더 준비 실패 — %s", qPrintable(m_prog->log()));
        return;
    }

    if (!m_lineProg->addShaderFromSourceCode(QOpenGLShader::Vertex, kLineVert) ||
        !m_lineProg->addShaderFromSourceCode(QOpenGLShader::Fragment, kLineFrag) ||
        !m_lineProg->link()) {
        qWarning("ScanView3D: 라인 셰이더 준비 실패 — %s", qPrintable(m_lineProg->log()));
    }

    m_vbo.create();
    m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_gridVbo.create();
    m_gridVbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    buildGrid();
    glEnable(GL_DEPTH_TEST);
    glEnable(0x8642 /* GL_PROGRAM_POINT_SIZE */);
    // 호환 프로파일(2.1)에서는 GL_POINT_SPRITE 를 켜야 프래그먼트 셰이더의
    // gl_PointCoord 가 채워지는 드라이버가 있다(특히 인텔 통합 GPU) — 안 켜면
    // gl_PointCoord 가 항상 (0,0)으로 나와 프래그먼트 셰이더의 원형 discard
    // 조건(dot(d,d) > 0.25)에 모든 점이 걸려 점이 하나도 안 그려진다.
    glEnable(0x8861 /* GL_POINT_SPRITE */);
    m_dirty = true;
}

// 바닥 기준 격자 1 m. 높이는 헤더의 sensor_height_m 이 아니라 "실제로 찍힌
// 가장 낮은 점"에 맞춘다 — 이 킷은 두 값이 어긋나 있어서(센서고 0.47 m 인데
// 2.47 m 아래까지 찍힘) 메타데이터를 믿으면 격자가 점군 한가운데를 뚫는다.
void ScanView3D::buildGrid() {
    QVector<float> v;
    if (!m_cloud.isEmpty()) {
        const float y = static_cast<float>(m_cloud.hMin);
        const int x0 = static_cast<int>(std::floor(m_cloud.xMin)) - 1;
        const int x1 = static_cast<int>(std::ceil(m_cloud.xMax)) + 1;
        // 점군과 같은 축으로 깔아야 격자가 어긋나지 않는다(worldZ 주석 참고).
        const int z0 = static_cast<int>(std::floor(worldZ(m_cloud.zMax))) - 1;
        const int z1 = static_cast<int>(std::ceil(worldZ(m_cloud.zMin))) + 1;
        for (int x = x0; x <= x1; ++x) {
            v << static_cast<float>(x) << y << static_cast<float>(z0)
              << static_cast<float>(x) << y << static_cast<float>(z1);
        }
        for (int z = z0; z <= z1; ++z) {
            v << static_cast<float>(x0) << y << static_cast<float>(z)
              << static_cast<float>(x1) << y << static_cast<float>(z);
        }
    }
    m_gridCount = v.size() / 3;
    if (m_gridVbo.isCreated() && m_gridCount > 0) {
        m_gridVbo.bind();
        m_gridVbo.allocate(v.constData(), static_cast<int>(v.size() * sizeof(float)));
        m_gridVbo.release();
    }
}

void ScanView3D::uploadCloud() {
    if (!m_dirty || !m_vbo.isCreated()) return;
    buildGrid();
    m_vbo.bind();
    m_vbo.allocate(m_pending.constData(), static_cast<int>(m_pending.size() * sizeof(float)));
    m_vbo.release();
    m_dirty = false;
}

void ScanView3D::resizeGL(int, int) {}

void ScanView3D::paintGL() {
    const bool dark = (Theme::CurrentMode == Theme::Mode::Developer);
    const QColor bg = Theme::MapBg;
    glClearColor(static_cast<float>(bg.redF()), static_cast<float>(bg.greenF()),
                 static_cast<float>(bg.blueF()), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    uploadCloud();
    if (m_count > 0 && m_prog->isLinked()) {
        const float a = qDegreesToRadians(m_az), e = qDegreesToRadians(m_el);
        const float ce = std::cos(e);
        const QVector3D eye = m_target + QVector3D(m_radius * ce * std::sin(a),
                                                   m_radius * std::sin(e),
                                                   m_radius * ce * std::cos(a));
        QMatrix4x4 mv, proj;
        mv.lookAt(eye, m_target, QVector3D(0, 1, 0));
        proj.perspective(50.0f, width() / static_cast<float>(qMax(1, height())), 0.05f, 200.0f);

        if (m_gridCount > 0 && m_lineProg->isLinked()) {
            m_lineProg->bind();
            m_lineProg->setUniformValue("uMVP", proj * mv);
            const QColor gc = Theme::Grid;
            m_lineProg->setUniformValue("uColor", QVector4D(gc.redF(), gc.greenF(), gc.blueF(), 1.0f));
            m_gridVbo.bind();
            const int lp = m_lineProg->attributeLocation("aPos");
            m_lineProg->enableAttributeArray(lp);
            m_lineProg->setAttributeBuffer(lp, GL_FLOAT, 0, 3, 3 * sizeof(float));
            glDrawArrays(GL_LINES, 0, m_gridCount);
            m_lineProg->disableAttributeArray(lp);
            m_gridVbo.release();
            m_lineProg->release();
        }

        m_prog->bind();
        m_prog->setUniformValue("uMV", mv);
        m_prog->setUniformValue("uProj", proj);
        m_prog->setUniformValue("uSize", kPointSize * static_cast<float>(devicePixelRatioF()));
        const Ramp r = rampFor(dark);
        const char *names[5] = {"uR0", "uR1", "uR2", "uR3", "uR4"};
        for (int i = 0; i < 5; ++i) {
            m_prog->setUniformValue(names[i], QVector3D(r.v[i][0], r.v[i][1], r.v[i][2]));
        }

        m_vbo.bind();
        const int locPos = m_prog->attributeLocation("aPos");
        const int locT   = m_prog->attributeLocation("aT");
        m_prog->enableAttributeArray(locPos);
        m_prog->setAttributeBuffer(locPos, GL_FLOAT, 0, 3, 4 * sizeof(float));
        m_prog->enableAttributeArray(locT);
        m_prog->setAttributeBuffer(locT, GL_FLOAT, 3 * sizeof(float), 1, 4 * sizeof(float));
        glDrawArrays(GL_POINTS, 0, m_count);
        m_prog->disableAttributeArray(locPos);
        m_prog->disableAttributeArray(locT);
        m_vbo.release();
        m_prog->release();
    }

    drawOverlay();
}

// QPainter 로 위에 글자를 얹는다. QOpenGLWidget 은 paintGL 안에서 QPainter 를
// 함께 쓸 수 있다 — 라벨 하나 그리자고 텍스처 아틀라스를 만들 이유가 없다.
void ScanView3D::drawOverlay() {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QFont mono("JetBrains Mono");
    mono.setPixelSize(10);
    p.setFont(mono);

    if (m_count == 0) {
        p.setPen(Theme::TextFaint);
        p.drawText(rect(), Qt::AlignCenter,
                   QString::fromUtf8("스캔 대기 중 — state/scan 을 받으면 여기에 점군이 뜬다"));
        return;
    }

    p.setPen(Theme::TextFaint);
    p.drawText(QRectF(10, height() - 34, width() - 20, 14), Qt::AlignLeft,
               QString::fromUtf8("드래그 회전 · 휠 확대 · Shift+드래그 이동 · 더블클릭 초기화"));
    p.setPen(Theme::TextDim);
    p.drawText(QRectF(10, height() - 20, width() - 20, 14), Qt::AlignLeft,
               QString::fromUtf8("%1점 · %2 · 방위 %3° 고도 %4° 시거리 %5 m")
                   .arg(m_count)
                   .arg(m_colorBy == ColorBy::Height ? QString::fromUtf8("높이") : QString::fromUtf8("거리"))
                   .arg(static_cast<int>(std::fmod(m_az + 360.0f, 360.0f)))
                   .arg(static_cast<int>(m_el))
                   .arg(QString::number(m_radius, 'f', 1)));
}

void ScanView3D::mousePressEvent(QMouseEvent *ev) {
    m_lastPos = ev->pos();
    m_panning = (ev->modifiers() & Qt::ShiftModifier) != 0;
}

void ScanView3D::mouseMoveEvent(QMouseEvent *ev) {
    const QPoint d = ev->pos() - m_lastPos;
    m_lastPos = ev->pos();
    if (d.isNull()) return;

    if (m_panning) {
        const float a = qDegreesToRadians(m_az);
        const float s = m_radius * 0.0016f;
        m_target -= QVector3D(std::cos(a) * d.x() * s, -d.y() * s, -std::sin(a) * d.x() * s);
    } else {
        m_az -= d.x() * 0.4f;
        m_el = qBound(-89.0f, m_el + d.y() * 0.32f, 89.0f);
    }
    update();
}

void ScanView3D::mouseReleaseEvent(QMouseEvent *) { m_panning = false; }

void ScanView3D::mouseDoubleClickEvent(QMouseEvent *) { resetView(); update(); }

void ScanView3D::wheelEvent(QWheelEvent *ev) {
    m_radius = qBound(0.4f, m_radius * std::exp(ev->angleDelta().y() * -0.0012f), 80.0f);
    update();
}
