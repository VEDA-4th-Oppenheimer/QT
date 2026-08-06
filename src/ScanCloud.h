#pragma once
#include <QPointF>
#include <QString>
#include <QVector>

// 데몬이 내보낸 포인트클라우드(.pcd)를 Top-View 가 쓸 형태로 담는다.
//
// 좌표 변환에 대해 — PCD 헤더가 선언하는 프레임은
//     +x 오른쪽 / +y 아래 / +z 전방,  원점 = 센서
// 이고 Top-View 위젯은
//     +x 오른쪽 / +y 위(북),        원점 = 킷
// 이다. 둘 다 킷이 원점이므로 평면 투영은 (x, z) 를 그대로 얹으면 된다.
// 높이는 화면 표시 관례에 맞춰 부호를 뒤집어(위가 +) 담는다 — 색상 구분에만
// 쓰고 좌표에는 반영하지 않는다.
struct ScanCloud {
    QVector<QPointF> floor;    // (x, z) 바닥 투영 [m]
    QVector<float>   height;   // 센서 기준 높이, 위가 + [m] — floor 와 같은 인덱스

    double xMin = 0.0, xMax = 0.0;     // floor.x 범위
    double zMin = 0.0, zMax = 0.0;     // floor.y(=원본 z) 범위
    double hMin = 0.0, hMax = 0.0;     // height 범위

    int gridTotal = 0;         // 격자 전체 점 수 (WIDTH x HEIGHT)
    int invalid   = 0;         // nan — 미반사
    int width = 0, height_ = 0;// 격자 크기(있으면)
    double sensorHeightM = 0.0;// 헤더 주석의 sensor_height_m (좌표 미적용)

    QString sourcePath;

    bool isEmpty() const { return floor.isEmpty(); }
    int  count() const { return floor.size(); }

    // 원점에서 가장 먼 점까지의 거리 — 뷰 배율 결정용
    double radiusM() const {
        const double a = qMax(qAbs(xMin), qAbs(xMax));
        const double b = qMax(qAbs(zMin), qAbs(zMax));
        return qMax(a, b);
    }
};

// ASCII PCD 를 읽는다. 데몬이 쓰는 형식(FIELDS x y z / TYPE F / DATA ascii)만
// 지원한다 — binary/binary_compressed 는 데몬이 만들지 않으므로 거부한다.
// 실패하면 false 를 돌려주고 err 에 사용자에게 보여줄 사유를 채운다.
bool loadPcdAscii(const QString &path, ScanCloud *out, QString *err);

// 이미 메모리에 있는 내용으로 파싱한다(HTTP 응답 등).
bool parsePcdAscii(const QByteArray &data, const QString &label,
                   ScanCloud *out, QString *err);
