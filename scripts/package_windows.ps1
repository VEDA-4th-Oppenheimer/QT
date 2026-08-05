# =============================================================================
#  package_windows.ps1 — Qt/Paho/FFmpeg 없는 고객 Windows 에서도 뜨는 배포본 생성
# -----------------------------------------------------------------------------
#  사용법 (PowerShell, 저장소 루트에서):
#      cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#      cmake --build build --config Release
#      .\scripts\package_windows.ps1
#
#  DLL 을 자동으로 못 찾으면 경로를 직접 준다:
#      .\scripts\package_windows.ps1 -ExtraDllDirs "C:\vcpkg\installed\x64-windows\bin"
#
#  결과: build\dist\SPATIAL-VMS\        (그대로 복사해 쓰는 폴더)
#        build\SPATIAL-VMS-windows.zip  (배포용 압축)
#
#  ※ 인증서(certs\)와 설정(config\*.json)은 **일부러 담지 않는다**.
#    qt-console 인증서는 adts/cmd/# 쓰기 권한이 있어 장비를 움직일 수 있고,
#    cameras.json 에는 카메라 admin 비밀번호가 URL 에 박혀 있다. 배포본에 넣으면
#    받은 사람 전원이 그 권한을 갖는다. 설정은 별도로 개별 전달하고, 앱이 최초
#    실행 때 폴더를 선택받아 %APPDATA%\VEDA4th\SPATIAL-VMS 로 복사한다.
# =============================================================================
[CmdletBinding()]
param(
    [string]   $BuildDir      = "build",
    [string]   $Config        = "Release",
    [string]   $WinDeployQt   = "",          # 비우면 PATH 에서 찾는다
    [string[]] $ExtraDllDirs  = @()          # vcpkg/MSYS2 등 DLL 이 있는 폴더
)

$ErrorActionPreference = "Stop"

$RootDir  = Split-Path -Parent $PSScriptRoot
$BuildAbs = Join-Path $RootDir $BuildDir
$DistRoot = Join-Path $BuildAbs "dist"
$AppDir   = Join-Path $DistRoot "SPATIAL-VMS"

# ── 1. 빌드 산출물 찾기 ──────────────────────────────────────────────────────
# 제너레이터에 따라 build\spatial_vms.exe (Ninja/MinGW) 또는
# build\Release\spatial_vms.exe (Visual Studio 멀티컨피그) 에 나온다.
$exeCandidates = @(
    (Join-Path $BuildAbs "spatial_vms.exe"),
    (Join-Path $BuildAbs "$Config\spatial_vms.exe")
)
$Exe = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Exe) {
    Write-Error ("spatial_vms.exe 를 찾지 못했습니다. 먼저 빌드하십시오.`n" +
                 "확인한 경로:`n  " + ($exeCandidates -join "`n  "))
}
Write-Host "== 실행파일: $Exe"

# ── 2. 스테이징 디렉터리 초기화 ──────────────────────────────────────────────
if (Test-Path $AppDir) { Remove-Item $AppDir -Recurse -Force }
New-Item -ItemType Directory -Path $AppDir -Force | Out-Null
Copy-Item $Exe $AppDir

# ── 3. windeployqt — Qt DLL·플러그인·MSVC 런타임 ─────────────────────────────
if (-not $WinDeployQt) {
    $cmd = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
    if (-not $cmd) { $cmd = Get-Command windeployqt6.exe -ErrorAction SilentlyContinue }
    if (-not $cmd) {
        Write-Error ("windeployqt 를 PATH 에서 찾지 못했습니다.`n" +
                     "Qt 개발자 명령 프롬프트에서 실행하거나 -WinDeployQt 로 경로를 주십시오.")
    }
    $WinDeployQt = $cmd.Source
}
Write-Host "== windeployqt: $WinDeployQt"

# --compiler-runtime : MSVC 재배포 런타임 동봉 (없는 PC 대응)
# --no-translations  : Qt 번역 리소스 제외 (이 앱은 문자열을 직접 박아 씀)
& $WinDeployQt --release --compiler-runtime --no-translations `
    (Join-Path $AppDir "spatial_vms.exe")
if ($LASTEXITCODE -ne 0) { Write-Error "windeployqt 실패 (exit $LASTEXITCODE)" }

# ── 4. Qt 가 아닌 의존 DLL 복사 ──────────────────────────────────────────────
# windeployqt 는 Qt 것만 처리한다. Paho·FFmpeg·OpenSSL 은 직접 챙겨야 한다.
# 버전이 파일명에 붙으므로(avcodec-61.dll 등) 와일드카드로 찾는다.
$patterns = @(
    "paho-mqtt3as*.dll",   # Paho C (async + SSL)
    "paho-mqttpp3*.dll",   # Paho C++ 래퍼
    "avformat*.dll", "avcodec*.dll", "avutil*.dll",
    "swscale*.dll", "swresample*.dll",
    "libssl*.dll", "libcrypto*.dll",   # OpenSSL — Paho TLS 에 필요
    "zlib*.dll"
)

# 탐색 위치: 사용자가 준 경로 → 실행파일 폴더 → PATH
$searchDirs = @()
$searchDirs += $ExtraDllDirs
$searchDirs += (Split-Path -Parent $Exe)
$searchDirs += ($env:PATH -split ';' | Where-Object { $_ -and (Test-Path $_) })
$searchDirs = $searchDirs | Select-Object -Unique

$missing = @()
foreach ($pat in $patterns) {
    $hit = $null
    foreach ($d in $searchDirs) {
        $found = Get-ChildItem -Path $d -Filter $pat -File -ErrorAction SilentlyContinue |
                 Select-Object -First 1
        if ($found) { $hit = $found; break }
    }
    if ($hit) {
        Copy-Item $hit.FullName $AppDir -Force
        Write-Host ("   + " + $hit.Name)
    } else {
        $missing += $pat
    }
}

# zlib 은 FFmpeg 빌드에 따라 정적으로 들어가 없을 수 있다 — 없어도 치명적이지 않다.
$critical = $missing | Where-Object { $_ -notlike "zlib*" -and $_ -notlike "swresample*" }
if ($critical) {
    Write-Warning ("다음 DLL 을 찾지 못했습니다. 배포본이 다른 PC 에서 실행되지 않을 수 있습니다:`n  " +
                   ($critical -join "`n  ") +
                   "`n-ExtraDllDirs 로 vcpkg/MSYS2 의 bin 폴더를 지정해 보십시오.")
}

# ── 5. 비밀정보 혼입 검사 ────────────────────────────────────────────────────
# 실수로 인증서나 실제 설정이 스테이징에 들어가면 여기서 멈춘다.
$leak = Get-ChildItem -Path $AppDir -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -in @(".key", ".crt", ".pem") -or
                       $_.Name -in @("mqtt.json", "cameras.json") }
if ($leak) {
    Write-Error ("배포본에 비밀정보가 들어 있습니다. 중단합니다:`n  " +
                 (($leak | ForEach-Object { $_.FullName }) -join "`n  "))
}

# ── 6. 압축 ──────────────────────────────────────────────────────────────────
$Zip = Join-Path $BuildAbs "SPATIAL-VMS-windows.zip"
if (Test-Path $Zip) { Remove-Item $Zip -Force }
Compress-Archive -Path $AppDir -DestinationPath $Zip

Write-Host ""
Write-Host "== 완료 =="
Write-Host "   폴더: $AppDir"
Write-Host "   압축: $Zip"
Write-Host ""
Write-Host "검증 방법: Qt/FFmpeg 가 설치되지 않은 PC 에 압축을 풀고 spatial_vms.exe 실행."
Write-Host "          최초 실행 시 설정 폴더를 묻는 창이 뜨면 정상입니다."
