# SPATIAL·VMS — ADTS 1D LiDAR Pan-Tilt 스캐너 킷 Qt 관제 (VEDA-4th-Oppenheimer)

Hanwha Vision **PNM-C16083RVQ / PNM-9084QZ** 4채널 멀티센서 AI 카메라 + **TOFSense-F2D** 1D LiDAR pan-tilt 스캐너로 사람 표적 없이(targetless) camera-LiDAR 외부 파라미터(extrinsic)를 자동 산출하는 킷의 Qt 데스크톱 관제 UI입니다.

- **UI 프레임워크**: Qt6 Widgets & OpenGL (다크 관제실 테마, `src/Theme.h`)
- **CCTV 영상 스트리밍**: **FFmpeg C/C++ Native API** 직접 바인딩 + **Windows Direct3D 11 Video Acceleration (D3D11VA) GPU 하드웨어 가속** (`src/RtspDecoder`, `src/RtspSource`)
- **하이브리드 비디오 아키텍처**:
  - **4분할 그리드 화면**: **CPU 소프트웨어 디코딩 + H.264 `profile4` (800x448 @ 1.07ms 최저지연 / 채널당 0.63 Mbps 초경량)**
  - **1채널 4MP 확대 화면**: **GPU 하드웨어 가속(D3D11VA) + H.265 `profile3` (2592x1520 @ 0.76ms 초고속 디코딩 / CPU 점유율 0% 오프로드)**
- **0초 무중단 스왑 (Seamless Pre-warm Swap)**: 듀얼 디코더 백그라운드 웜업 기술로 채널 전환 시 영상 끊김(Freeze) 0초화 달성
- **ONVIF 인밴드 AI 메타데이터**: 단일 RTSP TCP 세션 내에서 Person/Vehicle 감지 바운딩 박스 XML 실시간 파싱 및 3D 공간 투영
- **MQTT 통신**: Eclipse Paho MQTT C++ (`src/MqttBridge`) — mTLS(포트 8883) 기반 스캔 제어 및 상태 모니터링 (`adts/...` 토픽)
- **3D 공간 투영 및 점군 뷰어**: ASCII PCD 포인트클라우드 OpenGL 실시간 렌더링 및 카메라-라이다 공간 융합 (`src/TopViewWidget`, `src/SpatialProjector`)

---

## 📑 공식 기술 문서 및 벤치마크 보고서 (`docs/`)

| 문서명 | 주요 내용 | 바로가기 |
| :--- | :--- | :---: |
| **Wireshark 패킷 분석 보고서** | RTSP TCP Interleaved 시그널링, NAL Unit/GOP(Zero B-Frame IPPP...) 구조, ONVIF XML 메타데이터 파싱 | [보기](docs/WIRESHARK_RTSP_PACKET_ANALYSIS_REPORT.md) |
| **4채널 RTSP 채널/프로파일 규격서** | 한화 4채널 센서(`/0/`~`/3/`) 맵핑, 프로파일 1~5번 상세 규격, 네트워크 대역폭(2.52Mbps) 및 0초 스왑 | [보기](docs/RTSP_CHANNEL_PROFILE_SPECIFICATION.md) |
| **정밀 Latency 벤치마크 보고서** | FFmpeg C API 디코딩 파이프라인 구간($T_{\text{decode}}$, $T_{\text{hw\_copy}}$, $T_{\text{scale}}$, $T_{\text{proc}}$) 실측 및 GPU 가속 분석 | [보기](docs/RTSP_LATENCY_BENCHMARK_REPORT.md) |
| **PPT 발표 슬라이드 및 시각화 차트** | 캘리브레이션 톤앤매너 매칭 16:9 슬라이드 이미지(300 DPI), 엑셀 차트용 데이터셋, 3줄 핵심 요약 | [보기](docs/PPT_SLIDE_RTSP_LATENCY_SUMMARY.md) |

---

## 📊 RTSP 성능 최적화 검증 결과 요약

### 1. 지연 시간 (Latency) 최적화

| 지표 (해상도) | 최적화 전 (기본/CPU) | 최적화 후 (하이브리드 GPU 가속) | 속도 향상 |
| :--- | :---: | :---: | :---: |
| **1채널 4MP 순수 디코딩 시간** (2592x1520 @ H.265) | **`10.71 ms`** (CPU) | **`0.76 ms`** (GPU D3D11VA) | **14.10x 고속화 🚀** |
| **CPU 연산 부하율** (400만 화소 디코딩 점유율) | **25%** (CPU 과열/병목) | **0%** (GPU 전용 ASIC 오프로드) | **CPU 100% 해방** |
| **4분할 총 처리 지연** (800x448 @ H.264) | **`3.14 ms`** (GPU 복사 오버헤드) | **`1.07 ms`** (CPU 무복사 최적화) | **2.93x 고속화 ⚡** |
| **화면 전환 지연 (스왑)** (4분할 ↔ 1채널 확대) | **2~3초** (Black Frame 끊김) | **0초** (Seamless Pre-warm 스왑) | **0초 무중단 전환** |

### 2. 비트레이트 및 네트워크 대역폭 (Bitrate) 최적화

| 지표 (해상도) | H.264 (AVC) | H.265 (HEVC - 채택) | 대역폭 절감 효과 |
| :--- | :---: | :---: | :---: |
| **1채널 4MP 전송 비트레이트** (2592x1520 @ 30fps) | **`3.72 Mbps`** | **`2.35 Mbps`** | **36.8% 절감 (1.37 Mbps 절약)** |
| **4채널 전체 4MP 전송 시 대역폭** (2592x1520 4채널 전체) | **`14.88 Mbps`** (대역폭 부담) | **`9.40 Mbps`** | **총 5.48 Mbps 대폭 절감** |
| **4분할 4채널 총 수신 대역폭** (800x448 4채널 전체) | **`2.52 Mbps`** (최적 채택) | **`2.92 Mbps`** | **네트워크 부하 최소화** |

---

## 🛠️ 개발 환경 구축 및 빌드 방법

### Windows (MSVC 2022 / MinGW 64-bit)

1. **vcpkg 의존성 설치**:
   ```powershell
   vcpkg install qtbase[widgets,opengl] ffmpeg[avcodec,avformat,swscale] openssl paho-mqttcpp --triplet x64-windows
   ```
2. **CMake 빌드 및 실행**:
   ```powershell
   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
   cmake --build build --config Release
   .\build\Release\spatial_vms.exe
   ```

### macOS (Homebrew)

```bash
brew install qt paho-mqtt-c paho-mqtt-cpp ffmpeg pkg-config
cmake -S . -B build && cmake --build build
./build/spatial_vms.app/Contents/MacOS/spatial_vms
```

---

## 📦 시스템 아키텍처 및 디렉터리 구조

```
src/
├── main.cpp / MainWindow      # 5개 탭 셸, 시그널 배선, Demo/Live 전환, 카메라 설정
├── CameraConfig.h             # 지능형 하드웨어 가속(D3D11VA) 프로빙 및 프로파일 매핑
├── RtspDecoder.h/.cpp         # FFmpeg C API 기반 GPU/CPU RTSP 디코더 스레드
├── RtspSource.h/.cpp          # 4채널 스트림 관리자, 0초 무중단 웜업 스왑, 메타데이터 분기
├── CameraTile.h/.cpp          # 실시간 해상도/4MP 배지 OSD, 비디오 뷰어, BBox 렌더러
├── SpatialProjector.h/.cpp    # 2D 이미지 픽셀 ↔ 3D LiDAR 공간 투영 변환 엔진
├── TopViewWidget / TopViewPanel # 2D/3D Top-View 포인트클라우드 점군 렌더러
├── MqttBridge / DemoBridge    # mTLS Paho MQTT 브리지 및 계약 시뮬레이터
└── ConfigPath.h / Models.h    # 설정 파일 탐색기 및 공용 데이터 모델

docs/
├── README.md                                  # 기술 문서 종합 인덱스
├── WIRESHARK_RTSP_PACKET_ANALYSIS_REPORT.md   # Wireshark 패킷 캡처 정밀 분석 보고서
├── RTSP_CHANNEL_PROFILE_SPECIFICATION.md      # 4채널 RTSP 규격서
├── RTSP_LATENCY_BENCHMARK_REPORT.md           # 정밀 지연시간 벤치마크 보고서
├── PPT_SLIDE_RTSP_LATENCY_SUMMARY.md          # PPT 발표 슬라이드 및 엑셀 데이터
└── images/                                    # 300 DPI 고해상도 슬라이드/차트 이미지
```
