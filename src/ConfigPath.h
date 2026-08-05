#pragma once
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

// 배포본이 설정·인증서를 두는 사용자 데이터 디렉터리.
//   macOS   ~/Library/Application Support/<Org>/<App>
//   Windows C:/Users/<사용자>/AppData/Roaming/<Org>/<App>
//   Linux   ~/.local/share/<Org>/<App>
// 안에는 개발 트리와 **같은 구조**를 그대로 둔다 — config/*.json, certs/*.
// 그래야 resolveConfigPath 가 경로 이름을 바꿔가며 매핑할 필요가 없다.
inline QString userDataRoot() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

// config/*.json, certs/ 같은 상대경로를 실제 위치로 바꾼다.
//
// 탐색 순서 — 개발 트리를 먼저 본다. 개발 중에는 프로젝트 안의 파일을 고쳐서
// 바로 확인할 수 있어야 하고, 배포본에는 개발 트리가 없어서 자연히 3)으로
// 떨어지기 때문이다. (사용자 데이터를 먼저 보면, 개발자가 최초 설정을 한 번
// 돌린 뒤로 프로젝트 파일 수정이 조용히 무시돼 헷갈린다)
//   1) CWD 상대            — 터미널에서 프로젝트 루트에서 실행
//   2) 실행파일 위치→상위   — 개발 트리의 .app 번들을 Finder/IDE 로 실행
//   3) 사용자 데이터 디렉터리 — 배포본
inline QString resolveConfigPath(const QString &relativeName) {
    if (QFileInfo::exists(relativeName)) return relativeName;

    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        const QString candidate = dir.filePath(relativeName);
        if (QFileInfo::exists(candidate)) return candidate;
        if (!dir.cdUp()) break;
    }

    const QString userPath = userDataRoot() + QStringLiteral("/") + relativeName;
    if (QFileInfo::exists(userPath)) return userPath;

    return relativeName;   // 못 찾으면 원래 상대경로 그대로(호출부가 "없음"으로 처리)
}

// 접속 설정이 갖춰졌는지. mqtt.json 이 어디에도 없으면 최초 실행으로 본다.
inline bool configReady() {
    return QFileInfo::exists(resolveConfigPath(QStringLiteral("config/mqtt.json")));
}
