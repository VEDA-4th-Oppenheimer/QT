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
  (`adts/kit1/...` 토픽). 브로커는 **RPi 에 상주**(Mosquitto)하며 Qt·카메라 단·통합
  데몬이 모두 이 브로커의 클라이언트다. 포트 8883 + mTLS 가 기본이고, 인증서가 아직
  없으면(`config/mqtt.json` 의 `cert_dir` 에 `ca.crt`/`qt-console.crt`/`qt-console.key`
  가 없으면) 평문 1883 으로 degraded 접속한다 — 로컬 개발용.
- **데모 모드**: 실제 브로커/킷이 없어도 상단 메뉴 `모드 → Demo Mode` 토글로 계약서의
  실제 세션 흐름(`cmd/scan` → `state=SCANNING` → `event/progress` 2Hz → `state=EXPORT`
  + `state/scan` → `state=IDLE`)과 IMU 드리프트를 재생한다 (기본값 켜짐, `src/DemoBridge`).
  RTSP는 이 토글과 무관하게 `config/cameras.json`이 있으면 항상 동작한다.

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

## RTSP 카메라 설정

`config/cameras.example.json`을 `config/cameras.json`으로 복사한 뒤 실제 IP/계정정보를
채운다 (이 파일은 `.gitignore`로 커밋 제외됨 — 절대 커밋하지 말 것).

```bash
cp config/cameras.example.json config/cameras.json
```

PNM-C16083RVQ(4MP × 4ch 멀티센서) 기준 URL 형식:

```
rtsp://USER:PASSWORD@CAMERA_IP:554/<0~3>/profile2/media.smp
```

센서(채널) 번호 0~3이 CH1~CH4에 대응한다. `profile2`는 서브스트림, `profile1`은 고해상도
메인스트림. MVP 범위는 대표 1채널(CH1)이지만 하드웨어가 4채널 모두 지원해 4개 다 붙였다.

## MQTT 브로커 설정

`config/mqtt.example.json`을 `config/mqtt.json`으로 복사해 RPi IP/포트/인증서 경로를
채운다 (gitignore 대상). 인증서 3개(`ca.crt`/`qt-console.crt`/`qt-console.key`)는
`cert_dir` 아래 두면 자동으로 `ssl://`(mTLS)를 쓰고, 없으면 `tcp://` 평문으로
degraded 접속한다. 개인키(`.key`)는 어떤 경우에도 저장소에 커밋하지 않는다.

CLion에서 실행할 경우 Run/Debug configuration의 working directory를 프로젝트
루트로 맞춰야 `config/` 파일들을 찾는다.

## 빌드

```bash
cmake -S . -B build
cmake --build build
./build/spatial_vms
```

## 화면 구성

5개 탭: `메인 대시보드`(기본) / `CALIBRATION` / `DEVICES / MQTT` / `RGB-D DATASET` / `EVENT LOG`.
TopBar 버튼(HOME/SCAN/STOP/DISARM)의 활성화는 계약서 §5 상태-버튼 매핑을 따른다 —
DISARM 만 상태와 무관하게 항상 활성(비상정지).

## MQTT 토픽 (MQTT_INTERFACE_CONTRACT.md v1.0, 전부 확정)

| 토픽 | 방향 | QoS | Retained | 내용 |
|---|---|---|---|---|
| `adts/kit1/cmd/scan` | 발행 | 1 | **금지** | 스캔 시작 — `{req_id, pan_ddeg:[a,b], tilt_ddeg:[a,b], step_ddeg, sensor_height_mm}` |
| `adts/kit1/cmd/stop` | 발행 | 1 | 금지 | 스캔 중단 — `{req_id}` |
| `adts/kit1/cmd/home` | 발행 | 1 | 금지 | 홈만 수행 — `{req_id}` (데몬 쪽 아직 미지원, TODO) |
| `adts/kit1/cmd/disarm` | 발행 | 1 | 금지 | 안전정지 — `{req_id}` |
| `adts/kit1/state/daemon` | 구독 | 1 | 예 | FSM/링크/IMU. LWT 로 데몬 사망 시 `state:"OFFLINE"` 자동 수신 |
| `adts/kit1/state/scan` | 구독 | 1 | 예 | 스캔 결과 — 파일 경로만(점 데이터 없음) |
| `adts/kit1/event/progress` | 구독 | 0 | 아니오 | 진행률 ~2Hz, 유실 가정(완료 판정은 state 로) |
| `adts/kit1/event/error` | 구독 | 1 | 아니오 | 오류 코드/메시지 |

req_id 는 Qt 가 명령마다 생성(`MqttBridge::newReqId`)하고, 자신이 보낸 req_id 가
아닌 응답은 무시한다(`acceptsReqId`, 계약 §4).

## 구조

```
src/
├── main.cpp / MainWindow      # 5탭 셸, 시그널 배선, Demo/Live 모드 전환
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
```

## 남은 TODO

1. ~~영상 디코딩~~ — RTSP+FFmpeg로 완료 (`src/RtspDecoder`).
2. ~~MQTT 프로토콜 구현~~ — `MQTT_INTERFACE_CONTRACT.md` v1.0 그대로 구현 완료.
   실제 인증서(`config/mqtt.json`)와 RPi 데몬이 준비되면 바로 붙는다.
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
