# 한화 4채널 멀티센서 카메라 RTSP 채널 및 프로파일 규격서

> **대상 장비**: 한화비전 4채널 멀티디렉셔널 AI 카메라 (`PNM-9084QZ` / `PNM-9085RQZ` 계열)  
> **IP 주소**: `172.20.32.43:554`  
> **운용 시스템**: SPATIAL-VMS (Qt C++ & FFmpeg C API 기반)

---

## 1. 4채널 센서 인덱스 및 RTSP URL 매핑

한화 멀티센서 카메라는 단일 IP 주소에서 4개의 독립 렌즈 센서를 URL의 센서 인덱스 경로(`/0/`, `/1/`, `/2/`, `/3/`)로 구분하여 스트리밍합니다.

```
rtsp://admin:5hanwha!@172.20.32.43:554/{sensor_id}/profile{profile_id}/media.smp
```

| 채널 (UI) | 센서 ID (`sensor_id`) | RTSP 기본 URL (4분할 서브) | RTSP 확대 URL (1채널 4MP 원본) | 설치 방위 / 감시 영역 |
| :---: | :---: | :---: | :---: | :--- |
| **CH 1** | **`0`** | `rtsp://admin:...@172.20.32.43:554/0/profile4/media.smp` | `.../0/profile3/media.smp` | 전방 (0° ~ 90°) |
| **CH 2** | **`1`** | `rtsp://admin:...@172.20.32.43:554/1/profile4/media.smp` | `.../1/profile3/media.smp` | 우측 (90° ~ 180°) |
| **CH 3** | **`2`** | `rtsp://admin:...@172.20.32.43:554/2/profile4/media.smp` | `.../2/profile3/media.smp` | 후방 (180° ~ 270°) |
| **CH 4** | **`3`** | `rtsp://admin:...@172.20.32.43:554/3/profile4/media.smp` | `.../3/profile3/media.smp` | 좌측 (270° ~ 360°) |

---

## 2. 스트림 프로파일별(Profile 1~5) 상세 규격 및 실측 벤치마크

| 프로파일 ID | 코덱 | 해상도 (Pixel) | 프레임레이트 | 실측 비트레이트 | 순수 디코딩 시간 ($T_{\text{decode}}$) | 총 지연 ($T_{\text{proc}}$) | VMS 채택 및 운용 전략 |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| **`profile4`** | **H.264 High** | **`800 × 448`** | **30.0 fps** | **`0.63 Mbps`** | **`0.85 ms`** (CPU) | **`1.07 ms`** ⭐ | **4분할 기본 화면 채택 (초저지연/초경량)** |
| **`profile5`** | **H.265 Main** | **`800 × 448`** | 29.8 fps | `0.73 Mbps` | `1.59 ms` (CPU) | `1.80 ms` | 서브스트림 H.265 모드 |
| **`profile2`** | **H.264 High** | **`2592 × 1520` (4MP)** | 30.1 fps | `3.72 Mbps` | `5.98 ms` (CPU)<br>`0.77 ms` (GPU) | `8.14 ms` (CPU)<br>`17.61 ms` (GPU) | 4MP H.264 고해상도 백업 |
| **`profile3`** | **H.265 Main** | **`2592 × 1520` (4MP)** | **30.8 fps** | **`2.35 Mbps`** | **`10.71 ms`** (CPU)<br>**`0.76 ms`** (GPU) 🚀 | **`12.71 ms`** (CPU)<br>**`16.16 ms`** (GPU) ⭐ | **1채널 확대 화면 채택 (14.1배 가속/CPU 0%)** |

---

## 3. 네트워크 대역폭 및 CPU 부하 분석

### 3.1 4채널 동시 수신 시 네트워크 대역폭 비교
- **4분할 그리드 (`profile4` H.264 800x448)**:
  - $0.63\text{ Mbps} \times 4 = \mathbf{2.52\text{ Mbps}}$ (일반 사무망/현장 LTE 라우터에서도 여유롭게 동작)
- **1채널 4MP 확대 시 (`profile3` H.265 2592x1520)**:
  - 단 1개 확대 채널만 $2.35\text{ Mbps}$로 전환, 나머지 3개는 $0.63\text{ Mbps}$ 유지 $\rightarrow$ 총 $4.24\text{ Mbps}$

### 3.2 하이브리드 가속 아키텍처 (CPU Grid $\leftrightarrow$ GPU Zoom)
- **4분할 화면**:
  - 저해상도 연산량이 매우 가벼움(0.85ms).
  - PCIe VRAM $\rightarrow$ RAM 복사 오버헤드(7.3ms)가 없는 **CPU 소프트웨어 디코딩**으로 **1.07ms 최저지연** 구현.
- **1채널 확대 화면**:
  - 400만 화소 연산량이 CPU에서 10.71ms (CPU 점유율 25% 급증).
  - **Windows Direct3D 11 Video Acceleration (D3D11VA)** 전용 ASIC 디코더에 오프로드하여 **0.76ms (14.1배 가속) 및 CPU 점유율 0%** 달성.

---

## 4. 0초 무중단 듀얼 디코더 스왑 (Seamless Pre-warm Swap)

4분할 $\leftrightarrow$ 1채널 확대 전환 시 RTSP 연결 재수립에 따른 2~3초간의 검은 화면(Freeze)을 제거하기 위한 알고리즘입니다.

1. **Active Decoder 유지**: 기존 35fps 저해상도 화면을 계속해서 실시간 렌더링 유지.
2. **Background Pre-warm**: 목표 고해상도(`profile3`) 디코더를 백그라운드 스레드에서 조용히 연결 및 첫 키프레임(I-Frame) 디코딩 준비.
3. **Instant Swap**: 첫 번째 완전한 400만 화소 프레임이 나오는 즉시 화면을 0ms로 교체하고 기존 디코더를 안전하게 종료.
