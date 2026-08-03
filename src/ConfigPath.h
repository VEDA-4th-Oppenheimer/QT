#pragma once
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

// config/*.json 을 찾는다. 개발 중(터미널에서 ./build/spatial_vms 실행)에는 현재
// 작업 디렉터리가 프로젝트 루트라 상대경로가 바로 맞는다. 하지만 .app 번들을
// Finder에서 더블클릭하거나 배포된 실행파일을 실행하면 작업 디렉터리가 달라지므로,
// 실행파일 위치에서 위로 올라가며 config/ 를 찾는다(개발 트리 기준 최대 6단계).
inline QString resolveConfigPath(const QString &relativeName) {
    if (QFileInfo::exists(relativeName)) return relativeName;

    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        const QString candidate = dir.filePath(relativeName);
        if (QFileInfo::exists(candidate)) return candidate;
        if (!dir.cdUp()) break;
    }
    return relativeName;   // 못 찾으면 원래 상대경로 그대로 반환(호출부가 "없음"으로 처리)
}
