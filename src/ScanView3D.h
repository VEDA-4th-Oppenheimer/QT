#pragma once
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QPoint>
#include <QMatrix4x4>
#include <memory>
#include "Models.h"
#include "ScanCloud.h"

// 스캔 포인트클라우드 3D 뷰.
//
// Top-View(2D 조감)로는 벽 높이나 천장이 안 보인다 — 바닥에 투영하면 벽과 바닥이
// 같은 점으로 겹치기 때문이다. 그래서 같은 데이터를 3D 로도 본다.
//
// 좌표는 ScanCloud 를 그대로 쓴다: floor=(x, z), height=위가 + 인 높이.
// 3D 로는 (x, height, z) 즉 Y-up 이 된다. 원점은 센서(=킷).
//
// 드래그 회전 / 휠 확대 / Shift+드래그 이동 / 더블클릭 초기화.
class ScanView3D : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    enum class ColorBy { Height, Distance };

    explicit ScanView3D(QWidget *parent = nullptr);
    ~ScanView3D() override;

    void setScanCloud(const ScanCloud &cloud);
    void setObjects(const QVector<SpatialObject> &objects);
    void setColorBy(ColorBy mode);
    void resetView();
    bool hasCloud() const { return m_count > 0; }

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;

private:
    void uploadCloud();
    void buildGrid();
    void drawOverlay();
    void drawObjectMarkers(const QMatrix4x4 &mvp);

    // 컨텍스트마다 새로 만든다 — 위젯이 다른 창으로 옮겨가면 GL 컨텍스트가
    // 바뀌는데, 셰이더 프로그램은 만들어진 컨텍스트에 묶여 있다(initializeGL 주석).
    std::unique_ptr<QOpenGLShaderProgram> m_prog{new QOpenGLShaderProgram};
    std::unique_ptr<QOpenGLShaderProgram> m_lineProg{new QOpenGLShaderProgram};
    QOpenGLBuffer m_vbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_gridVbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_objectVbo{QOpenGLBuffer::VertexBuffer};
    int m_gridCount = 0;
    int  m_count = 0;
    bool m_glReady = false;
    bool m_dirty = false;          // 새 점군이 들어왔는데 아직 GPU 로 안 올림

    QVector<float> m_pending;      // x, y(up), z, t(0~1 색상값) x N
    ScanCloud      m_cloud;
    QVector<SpatialObject> m_objects;
    ColorBy        m_colorBy = ColorBy::Height;

    // 궤도 카메라 — 웹 뷰어와 같은 규약(방위/고도/시거리).
    float m_az = -35.0f, m_el = 26.0f, m_radius = 12.0f;
    QVector3D m_target;
    QPoint m_lastPos;
    bool m_panning = false;
    QMatrix4x4 m_lastMvp;
};
