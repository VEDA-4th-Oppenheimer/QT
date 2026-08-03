#!/usr/bin/env bash
# Homebrew 없는 고객 macOS에서도 뜨는 배포용 spatial_vms.app + .dmg 를 만든다.
#
# 사용법:
#   cmake -S . -B build && cmake --build build   # 먼저 빌드
#   ./scripts/package_macos.sh                   # 그 다음 이 스크립트
#
# 결과: build/spatial_vms.app (self-contained), build/SPATIAL-VMS.dmg
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
APP="$BUILD_DIR/spatial_vms.app"

if [ ! -d "$APP" ]; then
    echo "spatial_vms.app 없음 — 먼저 cmake --build build 실행" >&2
    exit 1
fi

command -v macdeployqt >/dev/null || { echo "macdeployqt 없음 (brew install qt)" >&2; exit 1; }

echo "== macdeployqt: Qt 프레임워크 + 감지된 Homebrew dylib 번들링 =="
macdeployqt "$APP" -verbose=1 || true
# 위 명령은 이 앱이 실제로 쓰지 않는 Qt 플러그인(QtPdf/QtSvg/QtVirtualKeyboard 의존)의
# 프레임워크 경로를 못 찾아 ERROR를 내지만 치명적이지 않다 — 아래에서 해당 플러그인 자체를 제거한다.

echo "== 미사용 플러그인 제거 (QtPdf/QtSvg/QtVirtualKeyboard 의존성 없음) =="
PLUGINS="$APP/Contents/PlugIns"
rm -f "$PLUGINS/imageformats/libqpdf.dylib" \
      "$PLUGINS/iconengines/libqsvgicon.dylib" \
      "$PLUGINS/platforminputcontexts/libqtvirtualkeyboardplugin.dylib"

echo "== 잔여 절대경로(Homebrew) 참조 스캔 =="
FOUND=0
while IFS= read -r -d '' f; do
    if file "$f" | grep -q "Mach-O"; then
        # 자기 자신의 id(install name)는 먼저 고쳐서 -L 의존성 스캔에 안 걸리게 한다
        id_ref=$(otool -D "$f" 2>/dev/null | tail -n +2 | grep -i "/opt/homebrew\|/usr/local" || true)
        if [ -n "$id_ref" ]; then
            newid="@executable_path/../Frameworks/$(basename "$f")"
            echo "   -> id 재기록: $(basename "$f") -> $newid"
            install_name_tool -id "$newid" "$f"
        fi
        # 첫 줄(자기 id)을 제외한 실제 의존성만 검사
        refs=$(otool -L "$f" 2>/dev/null | tail -n +2 | grep -i "/opt/homebrew\|/usr/local" || true)
        if [ -n "$refs" ]; then
            echo "!! $f 는 아직 Homebrew 절대경로 의존성을 가짐:"
            echo "$refs"
            FOUND=1
        fi
    fi
done < <(find "$APP" -type f \( -name "*.dylib" -o -perm +111 \) -print0)

if [ "$FOUND" = "1" ]; then
    echo "경고: 위 dylib들은 여전히 Homebrew 경로를 의존성으로 참조한다 — 새 브루 패키지가" \
         "추가되면 이 스크립트의 스캔 결과를 다시 확인할 것." >&2
fi

echo "== 코드사인 (ad-hoc, 배포 전 실제 서명 필요시 -s '<식별자>' 로 교체) =="
codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict "$APP"

echo "== DMG 생성 =="
DMG="$BUILD_DIR/SPATIAL-VMS.dmg"
rm -f "$DMG"
hdiutil create -volname "SPATIAL-VMS" -srcfolder "$APP" -ov -format UDZO "$DMG"

echo "== 완료: $APP , $DMG =="
echo "검증: cd /tmp && env -i '$APP/Contents/MacOS/spatial_vms' 로 Homebrew 환경 없이도 뜨는지 확인 가능"
