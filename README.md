# SPATIAL·VMS — 실내 3D 매핑 CCTV 관제 대시보드 (Qt / CLion)

천장 중앙에 4채널 CCTV + Pan-Tilt 1D LiDAR를 동일 마운트로 설치해, 실내 공간을 자동으로
3D 매핑하고 마커리스 2D/3D 캘리브레이션을 수행하는 시스템의 Qt 데스크톱 관제 UI.
`~/Downloads/design_handoff_spatial_vms`의 디자인 시안(`CCTV LiDAR Dashboard.dc.html`)을
Qt6 Widgets 코드로 옮긴 것이다.

- **UI**: Qt6 Widgets, 다크 관제실 테마 (`src/Theme.h`)
- **MQTT**: Eclipse Paho MQTT C++ (`src/MqttBridge`) — Homebrew에 Qt MQTT 애드온이 없어
  기존 프로젝트와 동일하게 Paho를 사용한다.
- **데모 모드**: 실제 브로커/펌웨어가 없어도 상단 메뉴 `모드 → Demo Mode` 토글로
  4채널 상태(CH1-3 LIVE / CH4 DISCONNECTED) + IMU 드리프트 + 8단계 캘리브레이션
  파이프라인 로그 + WiseAI 객체를 그대로 재생한다 (기본값 켜짐, `src/DemoBridge`).

## 의존성 설치 (macOS / Homebrew)

```bash
brew install qt paho-mqtt-c paho-mqtt-cpp
# 실제 브로커 연동 테스트용 (선택)
brew install mosquitto
```

## 빌드

```bash
cmake -S . -B build
cmake --build build
./build/spatial_vms
```

## 화면 구성

5개 탭: `메인 대시보드`(기본) / `CALIBRATION` / `DEVICES / MQTT` / `RGB-D DATASET` / `EVENT LOG`.
상세 스펙(색상·간격·상태값)은 `~/Downloads/design_handoff_spatial_vms/README.md`의
Design Tokens를 그대로 따른다 (`src/Theme.h`에 상수 + 전역 QSS로 반영).

## MQTT 토픽 스키마

| 토픽 | 방향 | 페이로드 |
|---|---|---|
| `cctv/ch{1-4}/h264` | 구독 | 영상 프레임 (현재 JPEG 가정) |
| `wiseai/+/objects` | 구독 | `{"objects":[{"class","x","y","dist","ch"}]}` (실내 좌표 변환 완료 가정) |
| `kit/lidar/scan` | 구독 | `{"edges":[{"a":{"x","y"},"b":{"x","y"}}]}` (Depth-Edge 평면 투영 결과 가정) |
| `kit/imu/level` | 구독 | `{"roll_deg","pitch_deg"}` |
| `kit/calib/status` | 구독 | `{"ncc","reproj_px","retry","progress","points","coverage","inliers","candidate_lines"}` |
| `kit/cmd/power` | 발행 | `{"power":1\|0}` |
| `kit/cmd/rescan` | 발행 | `{"rescan":1}` |

## 구조

```
src/
├── main.cpp / MainWindow      # 5탭 셸, 시그널 배선, Demo/Live 모드 전환
├── Theme.h / Models.h         # 디자인 토큰, 공용 데이터 모델
├── DataBridge / MqttBridge / DemoBridge   # 공용 시그널 인터페이스 + 실제 구현체
├── TopBar / TiltBanner / StatusBar        # 상단/경고/하단 바
├── CameraTile / TopViewWidget / TopViewPanel  # 대시보드 좌(CCTV)/우(Top-View) 패널
└── CalibrationTab / DevicesTab / DatasetTab / EventLogTab   # 나머지 4개 탭
```

## 남은 TODO (백엔드/펌웨어 연동)

1. `MqttBridge::handleFrame` — 현재 JPEG 가정. H.264면 FFmpeg/QtMultimedia 디코더 삽입.
2. `MqttBridge::handleLidarScan` — 펌웨어가 원시 포인트 클라우드를 올린다면 Depth-Edge
   라인 피팅을 여기서 먼저 수행해야 한다 (현재는 이미 평면 투영된 라인을 가정).
3. WiseAI 2D BBox → 실내 좌표 변환(RT/H 로드, Module 4) — 현재 `wiseai/+/objects`
   payload 가 변환 완료된 좌표를 싣고 온다고 가정한다.
4. CALIBRATION 탭의 EXTRINSIC RT 그리드는 정지값(레퍼런스 시안 값)이다. 실제 캘리브레이션
   결과 JSON을 로드해 바인딩해야 한다.
5. `RGB-D DATASET` 탭은 `datasets/<set>/meta.json`을 스캔한다(없으면 예시 6건으로 대체).
   실제 캡처 파이프라인이 이 디렉터리 구조로 저장하도록 맞추면 된다.
