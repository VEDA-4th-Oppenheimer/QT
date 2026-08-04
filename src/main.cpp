#include <QApplication>
#include <QStyleFactory>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("SPATIAL-VMS");
    // macOS 네이티브 스타일(QMacStyle)은 QSS로 지정한 버튼 테두리 밑에 자체
    // 베벨/그림자를 살짝 겹쳐 그려서, 색 있는 border(SCAN/DISARM 등)의 아래쪽
    // 변만 다른 색으로 보이는 문제가 있다. Fusion으로 바꾸면 QSS를 있는 그대로
    // 그린다.
    app.setStyle(QStyleFactory::create("Fusion"));
    MainWindow w;
    w.show();
    return app.exec();
}
