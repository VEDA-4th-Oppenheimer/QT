# SPATIAL·VMS — ADTS 1D LiDAR Pan-Tilt 스캐너 킷 Qt 관제 (VEDA-4th-Oppenheimer)

Hanwha Vision **PNM-C16083RVQ** 멀티센서 카메라 + **TOFSense-F2D** 1D LiDAR pan-tilt
스캐너로 사람 표적 없이(targetless) camera-LiDAR 외부 파라미터(extrinsic)를 자동
산출하는 킷의 Qt 데스크톱 관제 UI. **Qt 담당: 송영빈** — RPi 데몬과의 통신은
`MQTT_INTERFACE_CONTRACT.md` v1.0(RPi 저장소 `docs/`, 데몬=이현우/브로커·인증서=이광진
서명)을 그대로 구현한다.

- **UI**: Qt6 Widgets, 다크 관제실 테마 (`src/Theme.h`)
- **CCTV 영상**: RTSP 직접 연결(MQTT 경유 아님). `src/RtspDecoder`가 FFmpeg
  (libavformat/avcodec/swscale)로 채널별 RTSP 스트림을 백그라운드 스레드에서
  디코딩해 `CameraTile`에 공급한다 (`config/cameras.json`에 설정된 채널만 — 없으면
  해당 채널은 Demo/Live 상태를 그대로 따른다).
- **MQTT**: Eclipse Paho MQTT C++ (`src/MqttBridge`) — 스캔 제어/상태 전용
  (`adts/...` 토픽). 브로커는 **RPi 에 상주**(Mosquitto)하며 Qt·카메라 단·통합
  데몬이 모두 이 브로커의 클라이언트다. **포트 8883 + mTLS**. 인증서가 없으면
  평문 `tcp://` 로 degraded 접속하지만, 브로커가 TLS 전용이라 실제로는 실패한다 —
  로컬에 평문 브로커를 따로 띄운 경우에만 의미가 있다.
- **데모 모드**: 실제 브로커/킷이 없어도 상단 메뉴 `모드 → Demo Mode` 토글로 계약서의
  실제 세션 흐름(`cmd/scan` → `state=SCANNING` → `event/progress` 2Hz → `state=EXPORT`
  + `state/scan` → `state=IDLE`)과 IMU 드리프트를 재생한다 (기본값 꺼짐, `src/DemoBridge`).
  등록되지 않은 상태로 앱을 띄우면 자동으로 이 모드로 들어간다.
  RTSP는 이 토글과 무관하게 카메라 설정이 있으면 항상 동작한다.

## 참고 문서

1. `MQTT_INTERFACE_CONTRACT.md` v1.0 (RPi 저장소 `docs/`) — **이 앱의 MQTT 부분은
   전적으로 이 문서를 따른다.** 토픽/페이로드/QoS/retain 을 바꾸려면 이 문서를 먼저
   고쳐야 한다.
2. *Device 파트 아키텍처 및 역할 분담 V2* (Confluence) — 전체 시스템 아키텍처, RTSP 경로 확인용.
3. *01/02. Point Cloud 생성·Camera Automatic Calibration 계획* (Confluence) — 카메라
   단 캘리브 결과(NCC/edge_rmse/extrinsic) 스키마. **이 MQTT 계약과는 별개**이며
   발행 토픽이 아직 정해지지 않았다(계약 §9 미결) — 그래서 이 Qt 앱은 캘리브
   품질/RT 를 아직 표시하지 않는다.

## 의존성 설치 (macOS / Homebrew)

```bash
brew install qt paho-mqtt-c paho-mqtt-cpp ffmpeg pkg-config
# 실제 브로커 연동 테스트용 (선택)
brew install mosquitto
```

---

# 사용법

접속에 필요한 것(인증서·브로커 주소·카메라 URL)은 전부 비밀정보라 저장소에도
배포본에도 들어 있지 않다. 그래서 **처음 쓰는 사람**과 **개발하는 사람**의 설정
방법이 다르다.

## A. 배포본을 받아 쓰는 경우 (일반 사용자)

앱을 실행하면 **등록 화면**이 뜬다. 관리자에게 받은 **1회용 토큰**을 입력하면
인증서와 카메라 설정을 한 번에 받아온다. 한 번만 하면 되고, 이후에는 묻지 않는다.

| 입력란 | 값 |
|---|---|
| 발급 서버 주소 | RPi IP (예: `172.20.32.110`) |
| 포트 | `8443` (기본값) |
| 토큰 | 관리자에게 받은 1회용 문자열 |
| 기기 이름 | 자동으로 호스트명이 채워진다 — 브로커 로그에서 누구 것인지 구분용 |

받은 파일은 아래에 저장된다. 앱을 지우거나 다시 설치해도 남는다.

| OS | 위치 |
|---|---|
| macOS | `~/Library/Application Support/VEDA4th/SPATIAL-VMS/` |
| Windows | `C:\Users\<사용자>\AppData\Roaming\VEDA4th\SPATIAL-VMS\` |
| Linux | `~/.local/share/VEDA4th/SPATIAL-VMS/` |

**토큰이 없거나 나중에 하려면** `나중에`를 누른다. Demo Mode 로 떠서 화면 구성은
볼 수 있고, 실제 장비에는 붙지 않는다.

**로그아웃**: 상단 메뉴 `모드 → 로그아웃`. 이 기기에 저장된 인증서와 설정을 지우고
앱을 닫는다. 다시 쓰려면 새 토큰을 발급받아야 한다. (이미 발급된 인증서 자체의
무효화(CRL)는 아직 없다 — 기기 분실 대응이 필요하면 브로커에 CRL 을 걸어야 한다)

### 발급 서버 계약

RPi 쪽 발급 서비스가 지켜야 하는 형식. 클라이언트는 이대로 구현돼 있다.

```
POST https://<host>:<port>/enroll
    {"token": "...", "device_name": "..."}

200 {"cn":"qt-console-<사용자>",
     "ca_crt":"-----BEGIN CERTIFICATE-----\n...",
     "client_crt":"-----BEGIN CERTIFICATE-----\n...",
     "client_key":"-----BEGIN RSA PRIVATE KEY-----\n...",
     "mqtt":{"host":"...","port":8883},
     "cameras":{"channels":{"1":"rtsp://...", ...}}}

401/409 {"error":"사유"}
```

서버 신원은 실행파일에 박아둔 `resources/ca.crt` 로만 검증한다(시스템 CA 는 쓰지
않는다). 발급 시점에는 아직 클라이언트 인증서가 없어 검증 근거가 이것뿐이다.
`ca.crt` 는 **공개** 인증서라 배포본에 들어가도 안전하다 — CA 를 재발급하면 이
파일도 같이 갱신해야 한다.

서버 구현 시 놓치기 쉬운 것:

- **발급할 때마다 브로커 ACL 에 CN 을 추가**해야 한다. mosquitto ACL 은
  `user <CN>` 정확 매칭이라 와일드카드가 없다. 빠뜨리면 인증서는 정상인데
  구독·발행이 조용히 막힌다.
- 인증서 서명에 **`-extensions v3_client`** 를 붙인다. 빠지면 mTLS 핸드셰이크에서
  거부된다.
- 클라이언트 키는 **전통 RSA 포맷**으로 내려준다. PKCS#8 이면 `QSslKey` 가 null 을
  반환하고 조용히 실패한다.
- 서버 인증서는 기존 `server.crt` 를 재사용할 수 있다 — SAN 에 IP 가 들어 있다.

## B. 저장소에서 직접 빌드하는 경우 (개발자)

```bash
cmake -S . -B build
cmake --build build
./build/spatial_vms.app/Contents/MacOS/spatial_vms   # macOS
```

개발 트리에서는 등록 마법사를 거치지 않고 **프로젝트 안의 설정 파일**을 그대로 쓴다.
example 을 복사해 실제 값을 채운다 (둘 다 gitignore 대상 — 절대 커밋하지 말 것).

```bash
cp config/cameras.example.json config/cameras.json
cp config/mqtt.example.json    config/mqtt.json
```

인증서 3개(`ca.crt` / `qt-console.crt` / `qt-console-trad.key`)는 `mqtt.json` 의
`cert_dir` 이 가리키는 폴더(기본 `certs/`)에 둔다. RPi 의 `/etc/adts/certs/` 에서
받아오면 된다. 개인키(`.key`)는 어떤 경우에도 커밋하지 않는다.

PNM-C16083RVQ(4MP × 4ch 멀티센서) RTSP URL 형식:

```
rtsp://USER:PASSWORD@CAMERA_IP:554/<0~3>/profile2/media.smp
```

센서(채널) 번호 0~3이 CH1~CH4에 대응한다. `profile2`는 서브스트림, `profile1`은 고해상도
메인스트림. MVP 범위는 대표 1채널(CH1)이지만 하드웨어가 4채널 모두 지원해 4개 다 붙였다.

### 설정 파일 탐색 순서

`src/ConfigPath.h` 의 `resolveConfigPath()` 가 이 순서로 찾는다.

1. **현재 작업 디렉터리** 기준 상대경로 — 터미널에서 프로젝트 루트에서 실행할 때
2. **실행파일 위치에서 위로 최대 6단계** — 개발 트리의 `.app` 을 Finder/IDE 로 실행할 때
3. **사용자 데이터 디렉터리** — 배포본

개발 트리를 먼저 보는 이유: 개발 중에는 프로젝트 파일을 고쳐서 바로 확인할 수 있어야
하고, 배포본에는 개발 트리가 없어 자연히 3번으로 떨어지기 때문이다. 반대로 하면
개발자가 등록을 한 번 한 뒤로 프로젝트 파일 수정이 조용히 무시돼 헷갈린다.

> ⚠️ `cert_dir` 이 `"certs"` 같은 상대경로일 때, 예전에는 프로세스 CWD 기준으로
> 찾아서 Finder 로 실행하면(CWD=`/`) 인증서를 못 찾고 평문으로 degraded 접속해
> **설정은 맞는데 안 붙는** 상태가 됐다. 지금은 `cert_dir` 도 위 순서로 해석한다.

## 동시 접속 (Client ID)

MQTT 는 Client ID 가 유일해야 하고, 같은 ID 로 두 번째가 붙으면 브로커가 첫 번째를
끊는다. 그래서 Client ID 를 `qt-console-<호스트명>-<난수>` 로 만든다 — 여러 명이
동시에 콘솔을 켜도 서로 끊기지 않는다.

권한은 Client ID 가 아니라 **인증서 CN** 으로 판정되므로(`use_identity_as_username
true`) ACL 과 인증서는 그대로 쓸 수 있다. 계약서 §1 의 "고정 `qt-console`" 문구는
다중 콘솔에서 성립하지 않아 확장했다.

---

## 배포용 패키징 (Qt/FFmpeg/Paho 미설치 고객 PC용)

`qt_add_executable`은 macOS에서 `.app` 번들을(APPLE), Windows에서는 콘솔창 없는
GUI `.exe`를(WIN32) 만든다 (`CMakeLists.txt` 참고). 다만 기본 빌드 결과물은 여전히
빌드 머신의 Homebrew 라이브러리를 동적으로 링크하고 있어 그 자체로는 배포할 수
없다 — 아래 절차로 의존성을 번들링해야 한다.

### 앱 아이콘

`resources/AppIcon.icns`(macOS)/`resources/AppIcon.ico`(Windows) — CCTV 불릿
카메라 실루엣 + REC 표시등, 관제실 다크 테마(`Theme.h`) 색상과 맞춰 그렸다.
`CMakeLists.txt`가 플랫폼별로 자동으로 번들에 넣는다(macOS는
`MACOSX_BUNDLE_ICON_FILE`+리소스 복사, Windows는 `resources/app.rc` 통해 `.exe`에
아이콘 리소스로 컴파일). 디자인을 바꾸려면 `scripts/gen_app_icon.cpp` 참고
(사용법은 파일 상단 주석).

### macOS (검증 완료)

```bash
cmake -S . -B build && cmake --build build
./scripts/package_macos.sh
```

`scripts/package_macos.sh`가 하는 일:
1. `macdeployqt`로 Qt 프레임워크 + 링크된 Homebrew dylib(FFmpeg/Paho/OpenSSL 등)을
   `spatial_vms.app/Contents/Frameworks`로 복사하고 install name을
   `@executable_path/../Frameworks/...`로 재기록.
2. 이 앱이 실제로 쓰지 않는 플러그인(`libqpdf`/`libqsvgicon`/
   `libqtvirtualkeyboardplugin` — QtPdf/QtSvg/QtVirtualKeyboard 프레임워크를
   요구하지만 이 앱은 해당 프레임워크를 링크하지 않음) 제거.
3. 번들 전체를 스캔해 남은 Homebrew(`/opt/homebrew`)/`/usr/local` 절대경로 참조가
   있으면 `install_name_tool`로 재기록.
4. `codesign --deep --sign -`로 ad-hoc 재서명 (배포 시 실제 서명이 필요하면
   `-s '<Developer ID>'`로 교체).
5. `hdiutil`로 `build/SPATIAL-VMS.dmg` 생성.

검증 방법(실제로 확인함): `env -i build/spatial_vms.app/Contents/MacOS/spatial_vms`
— `PATH`/`HOME` 등 환경변수를 전부 지운 상태로, `/tmp`(프로젝트와 무관한 작업
디렉터리)에서 실행해도 정상 기동하고 `config/cameras.json`을 찾는 것까지 확인.
Homebrew가 전혀 없는 macOS에서 도 이 `.dmg`를 열어 `.app`을 `Applications`로
드래그하면 바로 쓸 수 있다.

### Windows (미검증 — 팀원 테스트 필요)

이 개발 환경은 macOS라 아래 절차를 실제로 빌드/실행해보지 못했다. **스크립트는
구문 검사조차 하지 못했다**(이 장비에 PowerShell 없음). 팀원이 Windows 머신에서
검증하고 결과를 알려주면 맞춘다.

```powershell
# vcpkg로 의존성 설치 (1회)
vcpkg install qtbase[widgets] ffmpeg[avcodec,avformat,swscale] openssl --triplet x64-windows
# paho-mqtt-cpp는 vcpkg에 없을 수 있어 소스 빌드 필요 (paho.mqtt.c 먼저, paho.mqtt.cpp 그 다음)

cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release

.\scripts\package_windows.ps1
# DLL 을 자동으로 못 찾으면 경로를 직접 준다:
.\scripts\package_windows.ps1 -ExtraDllDirs "C:\vcpkg\installed\x64-windows\bin"
```

`scripts/package_windows.ps1`이 하는 일:

1. `windeployqt --release --compiler-runtime` 으로 Qt DLL·플러그인·MSVC 런타임 수집.
2. `windeployqt` 가 모르는 **Paho / FFmpeg / OpenSSL** DLL 을 와일드카드로 찾아 복사
   (`avcodec-61.dll` 처럼 버전이 파일명에 붙기 때문). 못 찾으면 목록을 경고로 알린다.
3. **비밀정보 혼입 검사** — 스테이징에 `.key`/`.crt`/`.pem` 이나 `mqtt.json`/
   `cameras.json` 이 들어 있으면 중단한다. 배포본에 인증서를 담으면 받은 사람
   전원이 장비 명령 권한(`adts/cmd/#` 쓰기)과 카메라 admin 비밀번호를 갖게 된다.
4. `build\SPATIAL-VMS-windows.zip` 생성.

압축을 풀어 `spatial_vms.exe` 를 실행하면 등록 화면이 뜬다. 설치 프로그램
(Inno Setup 등)은 아직 없다 — zip 배포로도 동작하므로 우선순위를 낮췄다.

## 화면 구성

5개 탭: `메인 대시보드`(기본) / `CALIBRATION` / `DEVICES / MQTT` / `RGB-D DATASET` / `EVENT LOG`.
TopBar 버튼(HOME/SCAN/STOP/DISARM)의 활성화는 계약서 §5 상태-버튼 매핑을 따른다 —
DISARM 만 상태와 무관하게 항상 활성(비상정지).

## MQTT 토픽

> ⚠️ 계약서 v1.0 은 토픽에 `kit1` 세그먼트를 넣지만, **RPi 데몬 실구현에는 없다**
> (`daemon/modules/mqtt/mqtt_module.c`). 이 앱은 실구현 쪽에 맞췄다 — 아래가 실제로
> 오가는 토픽이다. 계약서가 재확정되면 `src/MqttBridge.cpp` 상단 상수와 함께 고칠 것.

| 토픽 | 방향 | QoS | Retained | 내용 |
|---|---|---|---|---|
| `adts/cmd/scan` | 발행 | 1 | **금지** | 스캔 시작 — `{req_id, pan_ddeg:[a,b], tilt_ddeg:[a,b], step_ddeg, sensor_height_mm}` |
| `adts/cmd/stop` | 발행 | 1 | 금지 | 스캔 중단 — `{req_id}` |
| `adts/cmd/home` | 발행 | 1 | 금지 | 홈만 수행 — `{req_id}` (데몬 쪽 아직 미지원, TODO) |
| `adts/cmd/disarm` | 발행 | 1 | 금지 | 안전정지 — `{req_id}` |
| `adts/state/daemon` | 구독 | 1 | 예 | FSM/링크/IMU. LWT 로 데몬 사망 시 `state:"OFFLINE"` 자동 수신 |
| `adts/state/scan` | 구독 | 1 | 예 | 스캔 결과 — 파일 경로만(점 데이터 없음) |
| `adts/event/progress` | 구독 | 0 | 아니오 | 진행률 ~2Hz, 유실 가정(완료 판정은 state 로) |
| `adts/event/error` | 구독 | 1 | 아니오 | 오류 코드/메시지 |

접속이 안 될 때 브로커에서 직접 들여다보면 어느 구간이 끊겼는지 빨리 갈린다:

```bash
mosquitto_sub -h <RPi IP> -p 8883 \
  --cafile certs/ca.crt --cert certs/qt-console.crt --key certs/qt-console-trad.key \
  -t 'adts/#' -v -i debug-$$      # -i: 앱과 Client ID 가 겹치지 않게
```

`adts/state/daemon` 이 `"online":false` 면 브로커는 살아 있고 **RPi 데몬이 죽은** 것이다.

req_id 는 Qt 가 명령마다 생성(`MqttBridge::newReqId`)하고, 자신이 보낸 req_id 가
아닌 응답은 무시한다(`acceptsReqId`, 계약 §4).

## 구조

```
src/
├── main.cpp / MainWindow      # 5탭 셸, 시그널 배선, Demo/Live 모드 전환, 로그아웃
├── ConfigPath.h               # 설정 파일 탐색(개발 트리 → 사용자 데이터 디렉터리)
├── EnrollDialog               # 최초 실행 등록 마법사 (1회용 토큰 → 인증서·설정 발급)
├── Theme.h / Models.h         # 디자인 토큰, 계약 스키마 데이터 모델
│                                 (DaemonState/ScanResult/ScanProgress/KitError)
├── DataBridge / MqttBridge / DemoBridge   # 계약 공용 시그널 인터페이스 + 구현체
├── RtspDecoder / RtspSource                # 채널별 RTSP 디코딩(FFmpeg) + config 로더
├── TopBar / TiltBanner / StatusBar        # 상단(HOME/SCAN/STOP/DISARM)/경고/하단 바
├── CameraTile / TopViewWidget / TopViewPanel  # 대시보드 좌(CCTV)/우(Top-View) 패널
└── CalibrationTab / DevicesTab / DatasetTab / EventLogTab   # 나머지 4개 탭

config/
├── cameras.example.json / cameras.json   # RTSP — 후자는 gitignore
└── mqtt.example.json / mqtt.json         # MQTT 브로커/인증서 경로 — 후자는 gitignore

certs/            # 개발용 인증서 — 통째로 gitignore
resources/
├── ca.crt / resources.qrc     # 사설 CA 공개 인증서 (발급 서버 검증용, 실행파일에 내장)
└── AppIcon.icns / AppIcon.ico / app.rc

scripts/
├── package_macos.sh           # .app 번들링 + .dmg (검증 완료)
└── package_windows.ps1        # DLL 수집 + zip (미검증)
```

## 남은 TODO

1. ~~영상 디코딩~~ — RTSP+FFmpeg로 완료 (`src/RtspDecoder`).
2. ~~MQTT 프로토콜 구현~~ — 완료. RPi 데몬·브로커와 실제 연동 확인함(mTLS 8883,
   `state/daemon` 하트비트·IMU 수신). 토픽은 계약서가 아닌 데몬 실구현 기준.
2-1. **발급 서버(`/enroll`) 미구현** — 클라이언트는 위 계약대로 준비돼 있다.
   RPi 쪽 서비스가 뜨면 실제 발급까지 연결해 확인해야 한다. (담당: 송영빈)
2-2. `scripts/package_windows.ps1` 미검증 — Windows 머신에서 1회 실행 필요.
3. `cmd/home` — 데몬 쪽에 "스캔 없이 홈만" 트리거하는 API가 아직 없다(코어 담당
   이현우 협의 필요). Qt 는 이미 발행하지만 데몬이 무시한다.
4. 카메라 단 캘리브 결과(NCC/edge_rmse/extrinsic) 표시 — 발행 토픽이 아직 없다
   (이영민 협의, 참고문서 3번 §9). 토픽이 정해지면 CALIBRATION 탭에 QUALITY
   패널을 추가한다.
5. `state/scan`이 포인트클라우드 "파일 경로"만 준다 — Top-View에 실제 라이다
   벽/에지를 그리려면 그 파일을 읽어오는 전달 방식(scp/http/공유폴더, 계약 §9
   미결)이 필요하다. 지금은 Demo Mode에서만 정적 방 외곽을 그린다.
6. `RGB-D DATASET` 탭은 `datasets/<set>/meta.json`을 스캔한다(없으면 예시로 대체) —
   실제 캡처 파이프라인 위치가 정해지면 맞춘다.
