# SPATIAL·VMS — 1D LiDAR Pan-Tilt 자동 캘리브레이션 킷 Qt 관제 (VEDA-4th-Oppenheimer)

Hanwha Vision **PNM-C16083RVQ** 멀티센서 카메라 + **TOFSense-F2D** 1D LiDAR pan-tilt
스캐너로 사람 표적 없이(targetless) camera-LiDAR 외부 파라미터(extrinsic)를 자동
산출하는 킷의 Qt 데스크톱 관제 UI. 이 프로젝트(`이광진` 담당)의 범위는 **Qt 관제
GUI + RTSP 4채널 스트리밍 + RPi Mosquitto 브로커 운영/MQTT 보안**이다.

- **UI**: Qt6 Widgets, 다크 관제실 테마 (`src/Theme.h`)
- **CCTV 영상**: RTSP 직접 연결(브로커 경유 아님). `src/RtspDecoder`가 FFmpeg
  (libavformat/avcodec/swscale)로 채널별 RTSP 스트림을 백그라운드 스레드에서
  디코딩해 `CameraTile`에 공급한다 (`config/cameras.json`에 설정된 채널만 — 없으면
  해당 채널은 Demo/Live 상태를 그대로 따른다).
- **MQTT**: Eclipse Paho MQTT C++ (`src/MqttBridge`) — 스캔 진행/캘리브 결과 텔레메트리용.
  브로커는 **RPi 에 상주**(Mosquitto, MQTT-over-TLS 8883)하며 Qt·카메라 단·통합
  데몬이 모두 이 브로커의 클라이언트다. Homebrew에 Qt MQTT 애드온이 없어 Paho를 사용한다.
- **데모 모드**: 실제 브로커/킷이 없어도 상단 메뉴 `모드 → Demo Mode` 토글로 스캔
  세션 전체 흐름(SCAN → POINT CLOUD → FEATURES → COARSE/FINE → QUALITY GATE →
  PASS)과 IMU 드리프트를 재생한다 (기본값 켜짐, `src/DemoBridge`). RTSP는 이
  토글과 무관하게 `config/cameras.json`이 있으면 항상 동작한다.

## 참고 문서 (Confluence, VPT space)

1. *1D LiDAR Pan-Tilt Actuator 기반 Camera Automatic Calibration 시스템 구축 계획* — 배경/타당성 검토
2. *01. Point Cloud 생성 및 인계 계획* — STM32/RPi 스캔·PointCloudPackage 계약
3. *02. Point Cloud 이후 Camera Automatic Calibration 상세 계획* — 캘리브 파이프라인/`extrinsic.yaml`·`quality.json` 스키마
4. *Device 파트 아키텍처 및 역할 분담 V2* — 전체 시스템 아키텍처, MQTT 브로커 구성, 담당자 배정

이 Qt 앱의 데이터 모델(`CalibState` 등)과 CALIBRATION 탭은 위 2/4번 문서의 실제
스키마를 따른다. **MQTT 토픽 스키마는 아직 팀 협의 중**이며(이현우·이광진·이영민
공통 과제), 아래 표의 `scan/*` 4개만 확정이고 나머지는 이 코드베이스의 placeholder다.

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
CLion에서 실행할 경우 Run/Debug configuration의 working directory를 프로젝트 루트로
맞춰야 `config/cameras.json`을 찾는다.

## 빌드

```bash
cmake -S . -B build
cmake --build build
./build/spatial_vms
```

## 화면 구성

5개 탭: `메인 대시보드`(기본) / `CALIBRATION` / `DEVICES / MQTT` / `RGB-D DATASET` / `EVENT LOG`.
시각 디자인(색상·간격·상태값 토큰)은 초기 UI 디자인 시안을 따르되(`src/Theme.h`),
CALIBRATION/DEVICES 탭의 데이터·용어는 위 참고 문서의 실제 시스템 스펙으로 교체했다.

## MQTT 토픽

| 토픽 | 방향 | 상태 | 페이로드 |
|---|---|---|---|
| `scan/start` | 발행 | **확정** | `{"pan_start_ddeg","pan_end_ddeg","tilt_start_ddeg","tilt_end_ddeg","step_ddeg","z_offset_mm"}` — CMD_SCAN_START 트리거 |
| `scan/stop` | 발행 | **확정** | 스캔 중단 |
| `scan/status` | 구독 | **확정** | `{"percent","points","expected","state"}` (수평 게이트 실패 시 `state:"tilt_ng"`) |
| `scan/done` | 구독 | **확정** | `{"path","point_count","stm_reported"}` — 포인트클라우드 파일 경로 (본문에 점 데이터 안 실림) |
| `calib/result` | 구독 | TODO(미정) | "02" 문서 §14.2 `quality`/`extrinsic` 스키마 가정 (`edge_rmse_px`,`inlier_ratio`,`translation_m`,`quaternion_xyzw`,...) |
| `calib/objects` | 구독 | TODO(미정) | WiseAI(Wisenet 네이티브 사람/차량) bbox → 실좌표 변환 결과 |
| `imu/level` | 구독 | TODO(미정) | `{"roll_deg","pitch_deg"}` — 상시 브로드캐스트 여부 자체가 미결 (현재는 스캔 전 1회 게이트 판정용) |

킷에는 "전원 ON/OFF" 개념이 없다(TopBar POWER 버튼은 실제 프로토콜과 무관 — CMD_HOME/
SCAN_START/STOP/DISARM 뿐). 영상(`cctv/chN/h264` 류)은 MQTT를 타지 않는다 — RTSP 직결.

## 구조

```
src/
├── main.cpp / MainWindow      # 5탭 셸, 시그널 배선, Demo/Live 모드 전환
├── Theme.h / Models.h         # 디자인 토큰, 공용 데이터 모델(CalibState 등 실제 스키마)
├── DataBridge / MqttBridge / DemoBridge   # 텔레메트리 공용 시그널 인터페이스 + 구현체
├── RtspDecoder / RtspSource                # 채널별 RTSP 디코딩(FFmpeg) + config 로더
├── TopBar / TiltBanner / StatusBar        # 상단/경고/하단 바
├── CameraTile / TopViewWidget / TopViewPanel  # 대시보드 좌(CCTV)/우(Top-View) 패널
└── CalibrationTab / DevicesTab / DatasetTab / EventLogTab   # 나머지 4개 탭

config/
├── cameras.example.json       # 커밋됨 — RTSP URL 형식 예시
└── cameras.json               # gitignore 대상 — 실제 카메라 IP/계정정보
```

## 남은 TODO

1. ~~영상 디코딩~~ — RTSP+FFmpeg로 완료 (`src/RtspDecoder`). VideoToolbox 하드웨어 가속은
   아직 미적용(소프트웨어 디코드); 채널 수/해상도가 늘면 고려.
2. **MQTT 토픽 스키마 확정** — `calib/result`·`calib/objects`·`imu/level`은 팀 협의 후
   `MqttBridge.cpp`의 토픽 상수만 바꾸면 된다(핸들러는 문서 스키마대로 이미 구현됨).
3. `scan/done`이 포인트클라우드 "파일 경로"만 준다 — Top-View에 실제 라이다 벽/에지를
   그리려면 그 파일(`organized_cloud.pcd`/`range_image`)을 읽어오는 전달 방식(scp/http/
   공유폴더, 아직 미정)이 필요하다. 지금은 Demo Mode에서만 정적 방 외곽을 그린다.
4. CALIBRATION 탭의 8단계 파이프라인 시각화는 "02" 문서 아키텍처를 따른 근사치다 —
   실제 코어(`calibration/core/`) 구현이 나오면 단계명/조건을 다시 맞춘다.
5. `RGB-D DATASET` 탭은 `datasets/<set>/meta.json`을 스캔한다(없으면 예시로 대체).
   실제로는 `calibration_sessions/<session_id>/result/{quality.json,extrinsic.yaml}`
   구조("02" 문서 §14.1)이므로, 카메라 단 세션 출력 위치가 정해지면 그에 맞춰야 한다.
6. TopBar POWER 버튼의 실제 의미(전원과 무관, DISARM/rearm에 가까움)를 재정의할지 검토.
