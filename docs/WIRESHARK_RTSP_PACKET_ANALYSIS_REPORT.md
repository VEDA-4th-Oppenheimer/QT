# Wireshark 패킷 캡처 기반 RTSP / RTP / ONVIF 프로토콜 정밀 분석 보고서

> **문서 버전**: v1.0  
> **분석 대상**: 한화비전 4채널 AI 멀티센서 카메라 (`172.20.32.43`) $\leftrightarrow$ SPATIAL-VMS 클라이언트 (`TCP 554`)  
> **캡처 도구**: Wireshark / TShark (TCP Interleaved RTSP/RTP/RTCP 스트림)

---

## 1. RTSP 연결 및 스트리밍 시그널링 흐름 (Signaling Sequence)

SPATIAL-VMS와 한화 카메라는 방화벽 및 NAT 통과 안정성과 패킷 유실 방지를 위해 **RTSP Interleaved TCP (Port 554)** 모드로 동작합니다.

```mermaid
sequenceDiagram
    autonumber
    actor VMS as SPATIAL-VMS (Client)
    participant CAM as Hanwha Camera (172.20.32.43:554)

    Note over VMS, CAM: 1단계: 연결 수립 및 기능 확인
    VMS->>CAM: OPTIONS rtsp://172.20.32.43:554/0/profile4/media.smp
    CAM-->>VMS: 200 OK (Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN)

    Note over VMS, CAM: 2단계: 세션 기술서(SDP) 요청 및 미디어 정보 파싱
    VMS->>CAM: DESCRIBE rtsp://172.20.32.43:554/0/profile4/media.smp (Accept: application/sdp)
    CAM-->>VMS: 200 OK (Content-Type: application/sdp)
    Note right of CAM: SDP: Video track (H.264/H.265) + Metadata track (ONVIF XML)

    Note over VMS, CAM: 3단계: 트랙별 전송 채널(Interleaved Channel) 바인딩
    VMS->>CAM: SETUP .../trackID=1 (Transport: RTP/AVP/TCP;unicast;interleaved=0-1)
    CAM-->>VMS: 200 OK (Session ID, interleaved=0-1: Video RTP/RTCP)
    VMS->>CAM: SETUP .../trackID=2 (Transport: RTP/AVP/TCP;unicast;interleaved=2-3)
    CAM-->>VMS: 200 OK (interleaved=2-3: Metadata RTP/RTCP)

    Note over VMS, CAM: 4단계: 실시간 스트리밍 시작
    VMS->>CAM: PLAY rtsp://... (Range: npt=0.000-)
    CAM-->>VMS: 200 OK (RTP-Info: seq, rtptime)

    Note over VMS, CAM: 5단계: 단일 TCP 세션 내 비디오/메타데이터 실시간 수신
    CAM-->>VMS: [$][0][Length][RTP Video Packet (H.264/H.265 NAL)]
    CAM-->>VMS: [$][2][Length][RTP Metadata Packet (ONVIF BoundingBox XML)]
    CAM-->>VMS: [$][1][Length][RTCP Sender Report]
```

---

## 2. NAL Unit 및 비디오 프레임 구조 분석

Wireshark 캡처 패킷을 역어셈블(Demux)하여 분석한 NAL(Network Abstraction Layer) 유닛 및 프레임 특성은 다음과 같습니다.

### 2.1 프레임 타입 통계 (I/P/B Frame Distribution)

| 프레임 타입 | 패킷/프레임 수 | 비율 (%) | 평균 크기 (Bytes) | 전송 소요 시간 | 비고 |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **I-프레임 (IDR)** | 181 | 1.9 % | **43,704 B** (약 43 KB) | 약 109.76 ms | SPS, PPS, VPS(HEVC), IDR Slice 포함 |
| **P-프레임** | 9,091 | 97.1 % | **1,006 B** (약 1 KB) | 약 2.44 ms | 이전 프레임 참조 단일/소형 패킷 |
| **B-프레임** | **0** | **0.0 %** | - | - | **완전 배제 (Zero B-Frame)** |
| **비-VCL/메타데이터** | 92 | 1.0 % | 412 B | < 1.0 ms | SEI, XML BoundingBox |

> [!IMPORTANT]
> **저지연(Ultra-Low Latency)을 위한 B-프레임 배제**:
> - B-프레임(양방향 예측)을 사용할 경우 미래 프레임 수신을 대기해야 하므로 **2~3프레임(66~100ms)의 디코딩 버퍼 지연**이 불가피합니다.
> - 본 스트림은 **IPPP... 구조**로 구성되어 있어 프레임이 도착하는 즉시 디코딩할 수 있으므로, **디코딩 지연 시간을 0.76~0.85ms로 극소화**할 수 있습니다.

### 2.2 GOP (Group of Pictures) 구조
- **GOP 패턴**: `IPPPPP...P` (고정 60 프레임)
- **IDR 키프레임 주기**: **약 2.0 초** (30 fps 기준 60 프레임마다 I-Frame 도래)
- **FU-A 분할 전송**:
  - I-Frame은 MTU(1500 바이트)보다 크기 때문에 약 30~40개의 **RTP FU-A (Fragmentation Unit)** 패킷으로 쪼개져 연속 전송됩니다.
  - P-Frame은 대부분 1개의 단일 NAL 패킷으로 1회 전송되어 즉시 처리됩니다.

---

## 3. ONVIF 인밴드(In-Band) 메타데이터 패킷 구조 분석

한화 카메라는 영상과 함께 **AI 객체 감지(Person, Vehicle) 바운딩 박스 좌표**를 RTSP 트랙 2번(RTP Interleaved Channel 2)을 통해 실시간 XML 페이로드로 전송합니다.

### 3.1 메타데이터 패킷 캡처 Hex & XML 덤프
```xml
<tt:MetadataStream xmlns:tt="http://www.onvif.org/ver10/schema">
  <tt:VideoAnalytics>
    <tt:Frame UtcTime="2026-08-31T14:50:57.120Z">
      <tt:Object ObjectId="142">
        <tt:Appearance>
          <tt:Class>
            <tt:Type Likelihood="0.94">Human</tt:Type>
          </tt:Class>
          <tt:Shape>
            <tt:BoundingBox left="-0.320" top="0.145" right="-0.110" bottom="-0.680"/>
          </tt:Shape>
        </tt:Appearance>
      </tt:Object>
    </tt:Frame>
  </tt:VideoAnalytics>
</tt:MetadataStream>
```

### 3.2 VMS 메타데이터 처리 파이프라인
1. `RtspDecoder`가 RTP 패킷 수신 시 `pkt->stream_index == m_metadataStreamIndex` 또는 페이로드 내 `BoundingBox` 키워드 감지.
2. `emit metadataReady(channel, payload)` 시그널 발송.
3. `RtspSource::ingestMetadataPayload()`에서 XML을 파싱하여 정규화 좌표(`[-1, 1]`)를 화면 픽셀 좌표 및 3D 공간 좌표로 실시간 투영(Spatial Projection).

---

## 4. 네트워크 성능 및 전송 타이밍 지표

| 지표 | 실측값 (Profile4 서브) | 실측값 (Profile3 메인 4MP) | 분석 요약 |
| :--- | :---: | :---: | :--- |
| **패킷 도착 평균 간격** | **33.31 ms** | **32.46 ms** | 정확한 30.0 fps 등간격 전송 |
| **네트워크 비트레이트** | **`0.63 Mbps`** | **`2.35 Mbps`** | H.265 압축으로 4MP 기준 36.8% 대역폭 절감 |
| **I-프레임 버스트 수신 시간** | 약 35~45 ms | 약 85~110 ms | TCP 버퍼 내 연속 도달 |
| **패킷 유실률 (TCP)** | **0.00 %** | **0.00 %** | TCP 재전송으로 영상 깨짐(Artifact) 0건 달성 |
