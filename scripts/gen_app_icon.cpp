// SPATIAL-VMS 앱 아이콘(resources/AppIcon.icns, resources/AppIcon.ico) 생성기.
// 디자인을 바꾸고 싶을 때만 다시 실행하면 된다 — 평소 빌드에는 안 쓰인다.
//
// 사용법 (macOS):
//   QT=/opt/homebrew/opt/qtbase
//   clang++ -std=c++17 -F"$QT/lib" \
//     -I"$QT/lib/QtCore.framework/Headers" -I"$QT/lib/QtGui.framework/Headers" \
//     scripts/gen_app_icon.cpp -o /tmp/gen_app_icon \
//     -framework QtCore -framework QtGui
//   QT_QPA_PLATFORM=offscreen /tmp/gen_app_icon /tmp/AppIcon.iconset
//   iconutil -c icns /tmp/AppIcon.iconset -o resources/AppIcon.icns
//   # .ico(Windows)는 /tmp/AppIcon.iconset/../AppIcon_master_1024.png를
//   # sips로 16/32/48/64/128/256에 리사이즈한 뒤 PNG를 그대로 ICO 컨테이너에 담으면 된다
//   # (Vista 이후 PNG-in-ICO 포맷 지원 — PIL/ImageMagick 없이 Python struct만으로 조립 가능).
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QDir>

static QImage drawIcon(int S) {
    QImage img(S, S, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 배경: 둥근 사각형(macOS Big Sur 스타일), 짙은 관제실 톤 세로 그라디언트
    const qreal radius = S * 0.176;
    QPainterPath bgPath;
    bgPath.addRoundedRect(QRectF(0, 0, S, S), radius, radius);
    QLinearGradient bgGrad(0, 0, 0, S);
    bgGrad.setColorAt(0.0, QColor("#141b21"));
    bgGrad.setColorAt(1.0, QColor("#0b0e11"));
    p.fillPath(bgPath, bgGrad);

    // 은은한 상단 하이라이트 테두리
    p.setPen(QPen(QColor(255, 255, 255, 18), 3));
    p.drawPath(bgPath);

    const QColor accent("#3fbfcc");
    const QColor accentBright("#8fd9e2");
    const QColor danger("#e0574a");
    const QColor bg("#0b0e11");

    p.setPen(Qt::NoPen);

    // --- CCTV 불릿 카메라 실루엣 (단색 accent 글리프) ---

    // 천장 마운트 플레이트
    QPainterPath mount;
    mount.addRoundedRect(QRectF(300, 300, 190, 46), 14, 14);
    p.fillPath(mount, accent);

    // 마운트 기둥
    QPainterPath stem;
    stem.addRoundedRect(QRectF(350, 336, 70, 90), 12, 12);
    p.fillPath(stem, accent);

    // 카메라 몸체 (캡슐)
    QPainterPath body;
    body.addRoundedRect(QRectF(250, 400, 400, 200), 100, 100);
    p.fillPath(body, accent);

    // 렌즈 (몸체 오른쪽 끝, 큰 원 — 몸체와 하나로 이어지는 실루엣)
    QPainterPath lensOuter;
    lensOuter.addEllipse(QPointF(650, 500), 145, 145);
    p.fillPath(lensOuter, accent);

    // 렌즈 링(구멍) — 배경색으로 펀치
    QPainterPath lensHole;
    lensHole.addEllipse(QPointF(650, 500), 96, 96);
    p.fillPath(lensHole, bg);

    // 렌즈 유리 하이라이트
    QRadialGradient glassGrad(QPointF(668, 480), 70);
    glassGrad.setColorAt(0.0, accentBright);
    glassGrad.setColorAt(1.0, accent.darker(160));
    QPainterPath glass;
    glass.addEllipse(QPointF(668, 480), 56, 56);
    p.fillPath(glass, glassGrad);

    // 렌즈 중앙 반사광
    QPainterPath glint;
    glint.addEllipse(QPointF(650, 462), 16, 16);
    p.fillPath(glint, QColor(255, 255, 255, 230));

    // REC 표시등 (몸체 위, 빨간 점)
    QPainterPath rec;
    rec.addEllipse(QPointF(500, 322), 28, 28);
    p.fillPath(rec, danger);
    QPainterPath recGlow;
    recGlow.addEllipse(QPointF(500, 322), 28, 28);
    p.setPen(QPen(danger.lighter(140), 6));
    p.setBrush(Qt::NoBrush);
    p.drawPath(recGlow);

    p.end();
    return img;
}

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);

    const QImage master = drawIcon(1024);

    QDir outDir(argc > 1 ? argv[1] : "AppIcon.iconset");
    outDir.mkpath(".");

    struct Entry { const char *name; int px; };
    const Entry entries[] = {
        {"icon_16x16.png", 16},
        {"icon_16x16@2x.png", 32},
        {"icon_32x32.png", 32},
        {"icon_32x32@2x.png", 64},
        {"icon_128x128.png", 128},
        {"icon_128x128@2x.png", 256},
        {"icon_256x256.png", 256},
        {"icon_256x256@2x.png", 512},
        {"icon_512x512.png", 512},
        {"icon_512x512@2x.png", 1024},
    };
    for (const auto &e : entries) {
        const QImage scaled = master.scaled(e.px, e.px, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaled.save(outDir.filePath(e.name), "PNG");
    }
    master.save(outDir.filePath("../AppIcon_master_1024.png"), "PNG");
    return 0;
}
