const pptxgen = require("pptxgenjs");
const pres = new pptxgen();
pres.layout = "LAYOUT_WIDE";           // 13.333 x 7.5
const W = 13.333, H = 7.5;

// SPATIAL·VMS 앱의 관제실 다크 테마(src/Theme.h)를 그대로 문서 팔레트로 쓴다.
const P = {
  bg:"0B0E11", panel:"12181D", head:"1A222A", border:"212A32", soft:"1C242B",
  text:"E4E9EE", dim:"9AA6B1", faint:"6B7883",
  accent:"FF6A1A", accentBright:"FFA766", accentBg:"2B1A10",
  ok:"4BBD85", okBg:"16241D", warn:"E2A33C", warnBg:"2E2413", danger:"E0574A", dangerBg:"3A1A16"
};
const F = "맑은 고딕";
const M = 0.6, CW = W - M*2;   // content width 12.133

const T = (s,txt,o)=>s.addText(txt, Object.assign({fontFace:F, margin:0, color:P.text, valign:"top"}, o));
const newSlide = ()=>{ const s = pres.addSlide(); s.background = {color:P.bg}; return s; };
function panel(s,x,y,w,h,fill,lineColor){
  s.addShape(pres.ShapeType.roundRect,{x,y,w,h,fill:{color:fill||P.panel},
    line:{color:lineColor||P.border,width:1}, rectRadius:0.05});
}
function head(s,tag,title){
  T(s,tag,{x:M,y:0.36,w:CW,h:0.26,fontSize:12,bold:true,color:P.accent});
  T(s,title,{x:M,y:0.64,w:CW,h:0.52,fontSize:28,bold:true});
}
function num(s,x,y,d,sz){ // 오렌지 원 안의 인덱스 번호 — 전 슬라이드 공통 모티프
  const r = sz||0.42;
  s.addShape(pres.ShapeType.ellipse,{x,y,w:r,h:r,fill:{color:P.accentBg},line:{color:P.accent,width:1}});
  T(s,d,{x,y,w:r,h:r,fontSize:r>0.5?15:11,bold:true,color:P.accentBright,align:"center",valign:"middle"});
}
function bullets(s,items,o){
  const runs = items.map((t,i)=>({text:t,options:{bullet:{code:"2022"},breakLine:i<items.length-1}}));
  T(s,runs,Object.assign({fontSize:12,color:P.dim,lineSpacing:19,paraSpaceAfter:5},o));
}
function tbl(s,rows,o){
  s.addTable(rows, Object.assign({
    x:M, w:CW, border:{type:"solid",color:P.border,pt:1}, fill:{color:P.panel},
    color:P.text, fontSize:11.5, fontFace:F, valign:"middle", margin:[6,9,6,9], autoPage:false
  },o));
}
const th = t=>({text:t,options:{fill:{color:P.head},bold:true,color:P.accentBright,fontSize:11.5}});

/* ───────────────────────── 1. 표지 ───────────────────────── */
{
  const s = newSlide();
  s.addShape(pres.ShapeType.rect,{x:0,y:0,w:W,h:H,fill:{color:P.bg}});
  // 스캔 점군을 연상시키는 도트 필드 (우측)
  let seed=7; const rnd=()=>{ seed=(seed*9301+49297)%233280; return seed/233280; };
  for(let i=0;i<150;i++){
    const x=7.4+rnd()*5.6, y=0.5+rnd()*6.5, d=0.03+rnd()*0.05;
    const c=[P.accent,P.accentBright,P.ok,P.faint,P.faint][Math.floor(rnd()*5)];
    s.addShape(pres.ShapeType.ellipse,{x,y,w:d,h:d,fill:{color:c},line:{color:c,width:0}});
  }
  T(s,"VEDA 4기 · 프로젝트 결과보고서",{x:1.0,y:1.5,w:7,h:0.3,fontSize:13,bold:true,color:P.accent});
  T(s,"SPATIAL·VMS",{x:1.0,y:1.95,w:8,h:1.0,fontSize:52,bold:true});
  T(s,"타깃리스 Camera–LiDAR 캘리브레이션 킷 관제 콘솔",{x:1.0,y:2.95,w:8,h:0.45,fontSize:18,color:P.dim});
  s.addShape(pres.ShapeType.rect,{x:1.0,y:3.75,w:1.6,h:0.02,fill:{color:P.accent},line:{color:P.accent,width:0}});
  T(s,"TEAM  Oppenheimer",{x:1.0,y:4.1,w:7,h:0.4,fontSize:20,bold:true,color:P.text});
  T(s,"송영빈 · 김○○ · 박○○ · 최○○ · 정○○",{x:1.0,y:4.6,w:7,h:0.32,fontSize:14,color:P.dim});
  T(s,"[멘토] ○○○",{x:1.0,y:4.95,w:7,h:0.32,fontSize:14,color:P.dim});
  T(s,"2026. 08.   |   github.com/VEDA-4th-Oppenheimer/QT",{x:1.0,y:6.3,w:8,h:0.3,fontSize:11,color:P.faint});
  s.addNotes("표지 — 팀명/팀원 이름은 실제 명단으로 교체 필요. 저장소 커밋에서 확인되는 이름은 송영빈(youngbinsong/Ybs0127) 1인이며, mainvoid00 계정의 실명 확인이 필요하다.");
}

/* ───────────────────────── 2. 목차 ───────────────────────── */
{
  const s = newSlide();
  T(s,"CONTENTS",{x:M+0.2,y:2.5,w:3.5,h:0.3,fontSize:13,bold:true,color:P.accent,charSpacing:1.6});
  T(s,"목 차",{x:M+0.2,y:2.85,w:3.5,h:0.9,fontSize:44,bold:true});
  T(s,"프로젝트 결과보고서",{x:M+0.2,y:3.85,w:3.5,h:0.3,fontSize:13,color:P.faint});
  const items = [
    ["01","프로젝트 개요","주제·배경, 구현 내용, 개발환경, 시스템 구조, 기대 효과"],
    ["02","프로젝트 팀 구성 및 역할","파트별 담당 업무와 멘토 지원 내역"],
    ["03","프로젝트 수행 절차 및 방법","사전 기획 → 수행 → 완료, 기간별 활동"],
    ["04","프로젝트 수행 경과","통신 계층·관제 UI·점군 시각화·보안/배포·피드백 반영"],
    ["05","자체 평가 의견","완성도 평가, 잘한 점과 아쉬운 점, 개선 방향"]
  ];
  items.forEach((it,i)=>{
    const y = 1.45 + i*1.05;
    panel(s,4.6,y,8.13,0.9);
    num(s,4.9,y+0.235,it[0],0.43);
    T(s,it[1],{x:5.55,y:y+0.15,w:7.0,h:0.32,fontSize:16,bold:true});
    T(s,it[2],{x:5.55,y:y+0.5,w:7.0,h:0.28,fontSize:11,color:P.faint});
  });
}

/* ─────────────── 3. 01 주제 및 선정 배경 ─────────────── */
{
  const s = newSlide();
  head(s,"01  프로젝트 개요","프로젝트 주제 및 선정 배경 · 기획 의도");
  panel(s,M,1.42,CW,1.35);
  T(s,"주제",{x:M+0.3,y:1.62,w:1.2,h:0.28,fontSize:12,bold:true,color:P.accent});
  T(s,"멀티센서 CCTV(Hanwha PNM-C16083RVQ)와 1D LiDAR 팬틸트 스캐너(TOFSense-F2D)를 묶어,\n체커보드 같은 표적 없이(targetless) 카메라–LiDAR 외부 파라미터를 산출하는 킷과 그 킷을 운용하는 관제 콘솔",
    {x:M+1.35,y:1.6,w:CW-1.7,h:1.0,fontSize:14,color:P.text,lineSpacing:24});

  T(s,"선정 배경",{x:M,y:2.95,w:4,h:0.3,fontSize:14,bold:true,color:P.accentBright});
  const bg = [
    ["01","캘리브레이션이 사람 손에 묶여 있다","체커보드·타깃을 현장에 설치하고 옮겨가며 찍어야 한다. 천장에 이미 설치된 CCTV는 재캘리브 자체가 사실상 불가능하다."],
    ["02","영상에 좌표가 없다","CCTV 화면만으로는 '무엇이' 보일 뿐 '어디에서'를 모른다. LiDAR 로 뜬 공간 좌표와 영상이 맞물려야 위치를 말할 수 있다."],
    ["03","현장에 남는 도구가 없다","캘리브 스크립트는 개발자 PC 안에만 있다. 설치 기사가 스캔을 돌리고 결과를 눈으로 확인할 화면이 필요했다."]
  ];
  bg.forEach((b,i)=>{
    const x = M + i*4.11;
    panel(s,x,3.28,3.89,2.3);
    num(s,x+0.28,3.55,b[0],0.4);
    T(s,b[1],{x:x+0.28,y:4.08,w:3.35,h:0.5,fontSize:13,bold:true,lineSpacing:19});
    T(s,b[2],{x:x+0.28,y:4.62,w:3.35,h:0.85,fontSize:10.5,color:P.dim,lineSpacing:16});
  });
  panel(s,M,5.78,CW,1.02,P.accentBg,P.accent);
  T(s,"기획 의도",{x:M+0.3,y:5.98,w:1.3,h:0.28,fontSize:12,bold:true,color:P.accentBright});
  T(s,"스캐너 킷과 관제 UI 를 한 세트로 만든다. 설치자가 앱 하나로 스캔을 걸고, 진행률과 장비 상태를 보고, 위험하면 즉시 멈추고,\n결과 점군을 그 자리에서 확인할 수 있게 하는 것이 이 프로젝트(Qt 파트)의 목표다.",
    {x:M+1.5,y:5.95,w:CW-1.8,h:0.7,fontSize:12,color:P.text,lineSpacing:19});
  s.addNotes("배경/의도는 기획안 원문이 확보되면 문구를 맞출 것. 현재는 저장소 README 와 커밋 메시지에서 도출.");
}

/* ─────────────── 4. 01 차별화 포인트 ─────────────── */
{
  const s = newSlide();
  head(s,"01  프로젝트 개요","특화 포인트 · 기존 유사 서비스와의 차별점");
  tbl(s,[
    [th("구분"), th("일반적인 방식 / 기존 도구"), th("SPATIAL·VMS")],
    ["캘리브 방식","체커보드·전용 타깃을 설치하고 사람이 위치를 옮겨가며 촬영","공간 구조물의 에지를 그대로 쓰는 타깃리스 방식 — 설치 인력 불필요"],
    ["결과 확인","터미널 로그와 파일. 현장에서는 성공 여부조차 알기 어렵다","관제 UI 에서 진행률 2Hz, 스캔 종료 즉시 Top-View·3D 점군 표시"],
    ["장비 통신","SSH 접속 후 스크립트 실행, 결과 파일 수동 복사","MQTT 인터페이스 계약(토픽·페이로드·QoS·retain) 기반 자동 연동"],
    ["보안","평문 통신, 설정 파일에 계정 정보 상주","mTLS 8883 + 인증서 CN 기반 ACL, 1회용 토큰 등록으로 자격 자동 발급"],
    ["배포","Qt·FFmpeg·Paho 를 설치해야 실행 가능","의존성 전부 번들링한 .dmg / .zip — 받아서 바로 실행"]
  ],{y:1.5,colW:[2.3,4.6,5.233],rowH:0.62});
  panel(s,M,5.5,CW,1.25,P.panel,P.accent);
  T(s,"한 줄 요약",{x:M+0.3,y:5.7,w:1.4,h:0.28,fontSize:12,bold:true,color:P.accent});
  T(s,"“타깃 없이 스캔하고, 계약대로 말하고, 설치 없이 배포한다.”\n캘리브 알고리즘 자체보다 현장에서 그것을 돌릴 수 있게 만드는 운용 도구가 이 프로젝트의 차별점이다.",
    {x:M+1.7,y:5.68,w:CW-2.0,h:0.9,fontSize:13,lineSpacing:21});
}

/* ─────────────── 5. 01 프로젝트 내용 ─────────────── */
{
  const s = newSlide();
  head(s,"01  프로젝트 개요","프로젝트 구현 내용 · 훈련 내용과의 연관성");
  const fs = [
    ["01","4채널 CCTV 실시간 뷰","멀티센서 카메라의 센서 0~3 을 CH1~CH4 로 직접 RTSP 연결. FFmpeg 로 채널별 백그라운드 디코딩."],
    ["02","스캐너 제어 · 안전정지","SCAN / STOP / HOME / DISARM / REARM. DISARM 은 상태와 무관하게 항상 눌린다(비상정지)."],
    ["03","상태 · IMU 모니터링","데몬 FSM, 링크, IMU 수평각을 상시 표시. 미수신과 '0.0° 정상' 을 구분해서 그린다."],
    ["04","스캔 점군 시각화","스캔 종료 즉시 .pcd 를 받아 Top-View 평면 투영과 3D 뷰로 표시. 높이로 색을 칠한다."],
    ["05","등록 마법사 · 보안","1회용 토큰 하나로 인증서·브로커·카메라 설정을 받아 mTLS 접속까지 자동 구성."],
    ["06","데모 모드","실장비 없이도 실제 세션 흐름(SCAN→진행률→EXPORT→IDLE)과 IMU 드리프트를 재생."]
  ];
  fs.forEach((f,i)=>{
    const x = M + (i%3)*4.11, y = 1.5 + Math.floor(i/3)*1.95;
    panel(s,x,y,3.89,1.8);
    num(s,x+0.26,y+0.24,f[0],0.38);
    T(s,f[1],{x:x+0.74,y:y+0.28,w:2.9,h:0.3,fontSize:12.5,bold:true});
    T(s,f[2],{x:x+0.26,y:y+0.75,w:3.37,h:0.9,fontSize:10.5,color:P.dim,lineSpacing:16});
  });
  panel(s,M,5.55,CW,1.2,P.panel,P.soft);
  T(s,"훈련 내용과의 연관성",{x:M+0.3,y:5.74,w:3,h:0.28,fontSize:12,bold:true,color:P.ok});
  T(s,"C++17 · Qt6 GUI 프로그래밍  ·  멀티스레드/동기화(채널별 디코딩 스레드)  ·  소켓·TLS 네트워크 프로그래밍(mTLS, HTTPS)  ·  임베디드 장비 연동(MQTT–UART–STM32)  ·  OpenGL 렌더링  ·  CMake 빌드/배포 자동화",
    {x:M+0.3,y:6.08,w:CW-0.6,h:0.6,fontSize:11.5,color:P.dim,lineSpacing:18});
}

/* ─────────────── 6. 01 활용 장비 및 개발환경 ─────────────── */
{
  const s = newSlide();
  head(s,"01  프로젝트 개요","활용 장비 및 재료 · 개발 환경");
  const cols = [
    ["하드웨어",P.accent,[
      "Hanwha Vision PNM-C16083RVQ — 4MP × 4채널 멀티센서 CCTV",
      "TOFSense-F2D — 1D LiDAR 거리 센서",
      "팬틸트 구동부 — STM32 펌웨어가 2축 제어",
      "IMU — 설치 수평 감시",
      "Raspberry Pi — 통합 데몬 + Mosquitto 브로커 상주",
      "관제 PC — macOS / Windows 11"
    ]],
    ["소프트웨어 · 라이브러리",P.ok,[
      "Qt 6 (Widgets / Network / OpenGLWidgets)",
      "C++17, CMake",
      "FFmpeg — libavformat / libavcodec / libswscale",
      "Eclipse Paho MQTT C++ · Mosquitto",
      "OpenSSL — mTLS, 사설 CA 체인 검증",
      "Git / GitHub (VEDA-4th-Oppenheimer/QT)"
    ]],
    ["개발 · 검증 환경",P.warn,[
      "macOS — Homebrew(Qt 6.11 / FFmpeg 8.1 / OpenSSL 3.6)",
      "Windows 11 — vcpkg + Visual Studio 2022 (MSVC x64)",
      "빌드 결과: .app / .dmg, .exe / .zip 이중 배포",
      "정적 분석 — clang-format / clang-tidy 설정 동봉",
      "실장비 검증 — RPi 브로커 mTLS 8883 실연동",
      "장비 부재 시 — 앱 내장 데모 모드"
    ]]
  ];
  cols.forEach((c,i)=>{
    const x = M + i*4.11;
    panel(s,x,1.5,3.89,4.35);
    s.addShape(pres.ShapeType.rect,{x:x+0.26,y:1.78,w:0.16,h:0.16,fill:{color:c[1]},line:{color:c[1],width:0}});
    T(s,c[0],{x:x+0.55,y:1.72,w:3.1,h:0.3,fontSize:13.5,bold:true,color:c[1]});
    bullets(s,c[2],{x:x+0.28,y:2.2,w:3.35,h:3.5,fontSize:10.5,lineSpacing:17});
  });
  panel(s,M,6.05,CW,0.85,P.panel,P.soft);
  T(s,"산출물 규모",{x:M+0.3,y:6.22,w:1.6,h:0.28,fontSize:11.5,bold:true,color:P.accent});
  T(s,"Qt 저장소 커밋 52건 (2026-08-03 ~ 08-19)   |   src/ C++ 소스 5,839 라인 · 클래스 20여 개   |   macOS 번들 115MB / DMG 52MB · Windows zip 배포본",
    {x:M+1.85,y:6.22,w:CW-2.1,h:0.3,fontSize:11,color:P.dim});
}

/* ─────────────── 7. 01 프로젝트 구조 ─────────────── */
{
  const s = newSlide();
  head(s,"01  프로젝트 개요","프로젝트 구조 — 시스템 아키텍처");
  // 3 columns
  const boxes = [
    [0.6,3.2,"현장 장비 (카메라)",P.warn,["Hanwha PNM-C16083RVQ","센서 0~3 → CH1~CH4","H.264 서브스트림(profile2)","※ RPi 를 경유하지 않는다"]],
    [5.0,3.4,"관제 콘솔 (본 저장소)",P.accent,["Qt6 Widgets · 5개 탭","RtspDecoder — 채널별 디코딩","MqttBridge / DemoBridge","ScanFetcher · ScanCloud · 3D 뷰","EnrollDialog — 등록/인증서"]],
    [9.6,3.13,"Raspberry Pi",P.ok,["Mosquitto 브로커 (8883/mTLS)","통합 데몬 — FSM·IMU·스캔","발급 서비스 (8443)","  · POST /enroll","  · GET /scan/<파일>.pcd"]]
  ];
  boxes.forEach(b=>{
    panel(s,b[0],1.5,b[1],3.7);
    T(s,b[2],{x:b[0]+0.22,y:1.72,w:b[1]-0.44,h:0.3,fontSize:13,bold:true,color:b[3]});
    bullets(s,b[4],{x:b[0]+0.24,y:2.2,w:b[1]-0.48,h:2.8,fontSize:10.5,lineSpacing:17});
  });
  // arrows
  const arrow=(x,y,w,lbl,ly)=>{
    s.addShape(pres.ShapeType.line,{x,y,w,h:0,line:{color:P.accent,width:1.5,endArrowType:"triangle",beginArrowType:"triangle"}});
    T(s,lbl,{x:x-0.06,y:ly,w:w+0.12,h:0.5,fontSize:9,color:P.accentBright,align:"center",lineSpacing:12});
  };
  arrow(3.85,3.1,1.1,"RTSP\n554/TCP",2.55);
  arrow(8.45,2.6,1.1,"MQTT\n8883 mTLS",2.05);
  arrow(8.45,4.3,1.1,"HTTPS\n8443",3.75);
  // bottom row
  panel(s,9.6,5.5,3.13,1.4);
  T(s,"팬틸트 · LiDAR",{x:9.82,y:5.68,w:2.7,h:0.28,fontSize:12,bold:true,color:P.dim});
  T(s,"STM32 펌웨어 (UART) — 2축 구동,\nTOFSense-F2D 거리 측정 · 진단 카운터",{x:9.82,y:6.02,w:2.7,h:0.6,fontSize:10,color:P.faint,lineSpacing:14});
  s.addShape(pres.ShapeType.line,{x:11.16,y:5.2,w:0,h:0.3,line:{color:P.ok,width:1.5,endArrowType:"triangle",beginArrowType:"triangle"}});
  panel(s,M,5.5,8.45,1.4,P.panel,P.soft);
  T(s,"Qt 내부 계층",{x:M+0.25,y:5.68,w:2,h:0.28,fontSize:12,bold:true,color:P.accent});
  T(s,"UI 탭(대시보드·CALIBRATION·DEVICES/MQTT·EVENT LOG·SETTINGS)  →  DataBridge 인터페이스  →  MqttBridge(실장비) / DemoBridge(데모)\n영상은 이 경로와 분리되어 RtspDecoder 가 직접 카메라에 붙는다. 설정 탐색은 ConfigPath 가 개발 트리 → 사용자 데이터 순으로 해석한다.",
    {x:M+0.25,y:6.02,w:7.95,h:0.8,fontSize:10,color:P.dim,lineSpacing:15});
}

/* ─────────────── 8. 01 활용방안 및 기대 효과 ─────────────── */
{
  const s = newSlide();
  head(s,"01  프로젝트 개요","활용 방안 및 기대 효과");
  const ef = [
    ["설치·유지보수 공수 절감",P.accent,"타깃 설치와 재캘리브에 들어가던 인력·시간을 스캔 한 번으로 대체한다. 천장에 이미 달린 카메라도 내리지 않고 다시 맞출 수 있다."],
    ["좌표를 가진 영상",P.ok,"검지 이벤트를 평면 좌표로 옮길 수 있다. 사고 위치 특정, AMR·로봇의 주행 지도 정합, 디지털 트윈 초기 측량에 그대로 쓰인다."],
    ["교체 가능한 관제 셸",P.warn,"UI 가 장비가 아니라 MQTT 계약에 묶여 있어, 킷이 바뀌어도 계약만 맞추면 화면은 그대로 재사용된다."],
    ["현장 배포 가능한 완성품",P.accentBright,"개발 환경 없이 실행되는 배포본과 1회용 토큰 등록 절차까지 갖춰, 설치 기사에게 그대로 넘길 수 있는 형태로 만들었다."]
  ];
  ef.forEach((e,i)=>{
    const x = M + (i%2)*6.17, y = 1.5 + Math.floor(i/2)*1.75;
    panel(s,x,y,5.96,1.6);
    s.addShape(pres.ShapeType.rect,{x:x+0.26,y:y+0.3,w:0.14,h:0.14,fill:{color:e[1]},line:{color:e[1],width:0}});
    T(s,e[0],{x:x+0.55,y:y+0.24,w:5.1,h:0.3,fontSize:14,bold:true,color:e[1]});
    T(s,e[2],{x:x+0.28,y:y+0.7,w:5.4,h:0.75,fontSize:11,color:P.dim,lineSpacing:17});
  });
  panel(s,M,5.15,CW,1.6,P.panel,P.soft);
  T(s,"적용 후보 도메인",{x:M+0.3,y:5.35,w:3,h:0.28,fontSize:12.5,bold:true,color:P.accent});
  const dom=[["물류창고","적재 공간 측량·안전 구역 감시"],["주차장","차량 위치 좌표화"],["공장 안전","작업자 접근 구역 판정"],["시설 점검","공간 형상 변화 추적"]];
  dom.forEach((d,i)=>{
    const x = M + 0.3 + i*2.9;
    T(s,d[0],{x,y:5.78,w:2.7,h:0.28,fontSize:12,bold:true});
    T(s,d[1],{x,y:6.1,w:2.7,h:0.5,fontSize:10,color:P.faint,lineSpacing:14});
  });
}

/* ─────────────── 9. 02 팀 구성 및 역할 ─────────────── */
{
  const s = newSlide();
  head(s,"02  프로젝트 팀 구성 및 역할","훈련생별 주도 참여 영역 · 멘토 지원 내역");
  tbl(s,[
    [th("훈련생"), th("역할"), th("담당 업무"), th("본 저장소 커밋")],
    ["송영빈","팀원 / Qt 파트","관제 콘솔 전담 — UI 5탭 설계·구현, MQTT 브리지, RTSP 디코딩, 스캔 점군 Top-View·3D, 등록 마법사와 mTLS, macOS/Windows 패키징, README 문서화","49건"],
    ["○○○\n(mainvoid00)","팀원","데몬 프로토콜 v6 반영(STM 진단 카운터·오류 축 표기), 센서 높이 실측값 반영, 데모 오류코드를 실물 데몬과 일치","3건"],
    ["이현우","팀원 / 데몬","RPi 통합 데몬 구현, MQTT 인터페이스 계약서 v1.0 작성, 스캔 FSM·IMU 처리","— (타 저장소)"],
    ["이광진","팀원 / 인프라","Mosquitto 브로커 구성, 사설 CA·인증서 발급 서비스, CN 기반 ACL 설계","— (타 저장소)"],
    ["이영민","팀원 / 캘리브","카메라 단 자동 캘리브레이션(NCC · edge_rmse · extrinsic) 설계 및 구현","— (타 저장소)"],
    ["○○○","멘토","주제 선정 피드백, 아키텍처 리뷰, 인터페이스 계약 정합성 질의응답","—"]
  ],{y:1.45,colW:[1.75,1.85,7.03,1.503],rowH:0.6,fontSize:10.5});
  panel(s,M,5.95,CW,0.8,P.panel,P.warn);
  T(s,"※ 커밋 수는 본 Qt 저장소 기준(총 52건). 데몬·브로커·캘리브 파트는 별도 저장소에 있어 여기에 집계되지 않는다. 표의 ○○○ 는 실제 명단 확인 후 채울 것.",
    {x:M+0.3,y:6.17,w:CW-0.6,h:0.4,fontSize:10.5,color:P.warn});
  s.addNotes("역할 배분은 README 의 파트 표기(데몬=이현우, 브로커·인증서=이광진, 카메라 캘리브=이영민)에서 가져왔다. 실제 담당과 다르면 수정 필요.");
}

/* ─────────────── 10. 03 수행 절차 및 방법 ─────────────── */
{
  const s = newSlide();
  head(s,"03  프로젝트 수행 절차 및 방법","사전 기획 → 수행 → 완료");
  const steps = ["주제 선정","기획 · 인터페이스\n계약 확정","아키텍처 설계","파트별 개발","실장비 통합","테스트 · 패키징\n· 보고"];
  steps.forEach((st,i)=>{
    const x = 0.64 + i*2.04;
    panel(s,x,1.4,1.85,1.0);
    num(s,x+0.14,1.52,"0"+(i+1),0.3);
    T(s,st,{x:x+0.1,y:1.88,w:1.65,h:0.45,fontSize:10,bold:true,align:"center",lineSpacing:13});
    if(i<5) T(s,"›",{x:x+1.85,y:1.68,w:0.19,h:0.4,fontSize:16,color:P.accent,align:"center"});
  });
  tbl(s,[
    [th("구분"), th("기간"), th("주요 활동"), th("비고")],
    ["분석 및 기획","○/○(월) ~ ○/○(금)","주제 선정, 유사 사례 조사, 요구사항 정리, 파트 분담","아이디어 선정 · 일자 확인 필요"],
    ["아키텍처 설계","○/○(월) ~ 8/2(일)","시스템 구성도 확정, MQTT 인터페이스 계약 v1.0 (토픽·페이로드·QoS·retain) 서명","계약 문서 기준 개발 착수"],
    ["SW 개발","8/3(월) ~ 8/14(금)","Qt 대시보드·MQTT 계층·RTSP 수신·점군 뷰·등록 마법사·패키징","커밋 46건"],
    ["통합 및 테스트","8/5(수) ~ 8/19(수)","실장비 mTLS 연동, 배포본 타 PC 실행 검증, 데몬 프로토콜 v6 반영","커밋 6건"],
    ["총 개발기간","○/○(월) ~ ○/○(금) (총 7주)","기획 2주 + 설계 1주 + 개발/통합 3주 + 정리 1주 (초안)","전체 일정 확인 필요"]
  ],{y:2.55,colW:[1.9,2.6,5.633,2.0],rowH:0.55,fontSize:10.5});
  panel(s,M,6.05,CW,0.85,P.panel,P.soft);
  T(s,"협업 방식",{x:M+0.3,y:6.25,w:1.4,h:0.28,fontSize:11.5,bold:true,color:P.accent});
  T(s,"계약 우선(contract-first) — 파트 간 인터페이스를 문서로 먼저 고정하고 각자 구현   |   GitHub 파트별 저장소 + main 브랜치 통합   |   변경 사유를 커밋 메시지와 README 에 남겨 추적",
    {x:M+1.6,y:6.25,w:CW-1.9,h:0.5,fontSize:10.5,color:P.dim,lineSpacing:15});
}

/* ─────────────── 11. 04-① MQTT 통신 계층 ─────────────── */
{
  const s = newSlide();
  head(s,"04  프로젝트 수행 경과","결과 제시 ①  계약 기반 MQTT 통신 계층");
  panel(s,M,1.45,7.5,3.95);
  T(s,"구현한 토픽 (실제 운용 기준)",{x:M+0.25,y:1.62,w:4,h:0.28,fontSize:12.5,bold:true,color:P.accent});
  s.addTable([
    [th("토픽"),th("방향"),th("QoS"),th("Retain")],
    ["adts/cmd/scan · stop · home · disarm · rearm","발행","1","금지"],
    ["adts/state/daemon  (FSM·링크·IMU·STM 진단)","구독","1","예"],
    ["adts/state/scan  (스캔 결과 .pcd 경로)","구독","1","예"],
    ["adts/event/progress  (진행률 ~2Hz)","구독","0","아니오"],
    ["adts/event/error  (오류코드·메시지·축)","구독","1","아니오"]
  ],{x:M+0.25,y:1.98,w:7.0,colW:[4.0,0.9,0.7,1.4],rowH:0.42,border:{type:"solid",color:P.border,pt:1},
     fill:{color:P.panel},color:P.text,fontSize:9.5,fontFace:F,valign:"middle",margin:[4,7,4,7]});
  bullets(s,[
    "req_id 를 명령마다 생성하고, 자신이 보낸 req_id 가 아닌 응답은 무시한다 (계약 §4)",
    "LWT 를 걸어 데몬이 죽으면 state:\"OFFLINE\" 이 즉시 도착한다"
  ],{x:M+0.25,y:4.75,w:7.0,h:0.6,fontSize:10,lineSpacing:16});

  panel(s,8.4,1.45,4.33,3.95,P.panel,P.danger);
  T(s,"문제 → 해결",{x:8.65,y:1.62,w:3,h:0.28,fontSize:12.5,bold:true,color:P.danger});
  T(s,"계약서 v1.0 은 토픽에 kit1 세그먼트를 넣지만, RPi 데몬 실구현에는 그 세그먼트가 없었다. 문서대로 짜면 브로커에 붙어도 명령이 한 건도 전달되지 않는다.",
    {x:8.65,y:2.0,w:3.83,h:1.1,fontSize:10.5,color:P.dim,lineSpacing:16});
  T(s,"→  실구현(daemon/modules/mqtt) 기준으로 토픽 상수를 정렬하고, 계약서와 어긋난 지점을 README 에 경고로 명시해 재확정 시 함께 고치도록 남겼다.",
    {x:8.65,y:3.15,w:3.83,h:1.1,fontSize:10.5,color:P.ok,lineSpacing:16});
  T(s,"커밋: “Rewrite MQTT layer against MQTT_INTERFACE_CONTRACT.md v1.0” → “Align MQTT layer to RPi develop branch's real daemon implementation”",
    {x:8.65,y:4.3,w:3.83,h:0.55,fontSize:9,color:P.faint,lineSpacing:13});

  panel(s,M,5.6,CW,1.32,P.panel,P.soft);
  T(s,"검증 방법",{x:M+0.3,y:5.78,w:2,h:0.28,fontSize:12,bold:true,color:P.accent});
  T(s,"① mosquitto_sub 로 브로커에서 토픽을 직접 확인해 발행/수신 구간을 분리   ② state/daemon 의 online 플래그로 브로커 정상·데몬 사망을 구분\n③ 데모 브리지로 전체 세션 흐름을 장비 없이 재생해 UI 반응 검증   ④ 실장비 mTLS 8883 연동으로 하트비트·IMU 수신 확인",
    {x:M+0.3,y:6.1,w:CW-0.6,h:0.75,fontSize:10,color:P.dim,lineSpacing:16});
}

/* ─────────────── 12. 04-② 관제 UI ─────────────── */
{
  const s = newSlide();
  head(s,"04  프로젝트 수행 경과","결과 제시 ②  관제 UI 구성과 조작 안전성");
  const tabs=[["메인 대시보드","좌 CCTV 4채널 / 우 Top-View. 경계 드래그로 비율 조정, Top-View 는 별도 창으로 분리"],
              ["CALIBRATION","스캔 파라미터(팬·틸트 범위, 스텝, 센서 높이) 입력과 실행. 센서 높이 기본값은 실측 1805mm"],
              ["DEVICES / MQTT","브로커 접속 상태와 토픽 표를 그대로 노출해 현장에서 통신 구간을 눈으로 확인"],
              ["EVENT LOG","오류코드·메시지·발생 축을 시간순으로. 최대 1,000행 유지"],
              ["SETTINGS","메뉴에 흩어져 있던 항목(테마·데모·카메라·센서 높이)을 값과 함께 한 화면에 모음"]];
  tabs.forEach((t,i)=>{
    const y = 1.45 + i*1.06;
    panel(s,M,y,6.6,0.92);
    T(s,t[0],{x:M+0.25,y:y+0.14,w:2.2,h:0.28,fontSize:12,bold:true,color:P.accentBright});
    T(s,t[1],{x:M+0.25,y:y+0.44,w:6.1,h:0.4,fontSize:9.5,color:P.dim,lineSpacing:13});
  });
  panel(s,7.5,1.45,5.23,2.55,P.panel,P.danger);
  T(s,"조작 안전성 설계",{x:7.75,y:1.62,w:3,h:0.28,fontSize:12.5,bold:true,color:P.danger});
  bullets(s,[
    "버튼 활성화는 계약 §5 의 상태–버튼 매핑을 그대로 따른다",
    "DISARM 만 상태와 무관하게 항상 활성 — 비상정지는 조건이 붙으면 안 된다",
    "스캔 종료 후 데몬이 스스로 DISARM 으로 내려가므로, HOME 버튼이 REARM 으로 바뀌어 cmd/rearm 을 발행한다",
    "IMU 미수신을 '수평 정상 0.0°' 로 그리던 버그 수정 — 모르는 값을 정상으로 표시하지 않는다"
  ],{x:7.75,y:2.0,w:4.73,h:1.9,fontSize:10,lineSpacing:15});
  panel(s,7.5,4.15,5.23,2.6,P.panel,P.soft);
  T(s,"UI 개선 이력 (커밋 기준)",{x:7.75,y:4.32,w:3.5,h:0.28,fontSize:12.5,bold:true,color:P.accent});
  bullets(s,[
    "관제실 다크 테마 토큰화(Theme.h) + 라이트 테마 동시 지원",
    "좁은 화면에서 Top-View 하단 통계바가 잘리던 문제 수정",
    "DEVICES/MQTT 토픽 표가 마지막 행을 빠뜨리던 off-by-one 수정",
    "쓰지 않던 RGB-D DATASET 탭 제거, IMU 센서 모델명 노출 제거",
    "메뉴 항목을 SETTINGS 탭으로 이관 — 값과 조작을 같은 화면에"
  ],{x:7.75,y:4.7,w:4.73,h:1.9,fontSize:10,lineSpacing:15});
}

/* ─────────────── 13. 04-③ 점군 시각화 ─────────────── */
{
  const s = newSlide();
  head(s,"04  프로젝트 수행 경과","결과 제시 ③  스캔 포인트클라우드 시각화");
  // pipeline
  const st=[["state/scan 수신",".pcd 파일 경로만 전달된다 (점 데이터 없음)"],
            ["ScanFetcher","로컬 scans/ 조회 → 없으면 HTTPS 8443 GET /scan/<파일명>"],
            ["ScanCloud 파싱","ASCII PCD + binary PCD 파싱, 좌표계 변환"],
            ["Top-View / 3D 렌더","평면 (x, z) 투영 · QOpenGLWidget 포인트 스프라이트"]];
  st.forEach((p,i)=>{
    const x = M + i*3.09;
    panel(s,x,1.45,2.85,1.5);
    num(s,x+0.22,1.62,"0"+(i+1),0.34);
    T(s,p[0],{x:x+0.22,y:2.05,w:2.4,h:0.28,fontSize:11.5,bold:true});
    T(s,p[1],{x:x+0.22,y:2.36,w:2.45,h:0.5,fontSize:9.5,color:P.dim,lineSpacing:13});
    if(i<3) s.addShape(pres.ShapeType.line,{x:x+2.87,y:2.2,w:0.2,h:0,line:{color:P.accent,width:1.5,endArrowType:"triangle"}});
  });
  panel(s,M,3.15,6.0,2.35);
  T(s,"렌더링 규칙",{x:M+0.25,y:3.32,w:3,h:0.28,fontSize:12.5,bold:true,color:P.accent});
  bullets(s,[
    "점 색은 높이로 칠한다 — 낮음 짙은 청색, 높음 밝은 난색",
    "상태색(Ok/Warn/Danger)과 일부러 다른 계열: '초록 점 = 정상' 으로 오독되면 안 된다",
    "스캔이 방보다 넓게 찍히면 뷰가 자동 확대되고 스케일 바에 VIEW 로 표기",
    "PCD 원본은 +x 오른쪽 / +y 아래 / +z 전방 → Top-View 는 (x, z) 투영, 높이는 화면 관례에 맞춰 부호 반전"
  ],{x:M+0.25,y:3.7,w:5.5,h:1.7,fontSize:10,lineSpacing:15});
  panel(s,6.85,3.15,5.88,2.35,P.panel,P.danger);
  T(s,"문제 → 해결",{x:7.1,y:3.32,w:3,h:0.28,fontSize:12.5,bold:true,color:P.danger});
  T(s,"스캔 파일을 8443 으로 받아오는 요청이 계속 깨졌다. 브로커(8883)는 붙는데 스캔만 안 오는 모습이라 원인이 잘 보이지 않았다. RPi 서버 인증서 SAN 에 발급 당시 IP 만 들어 있어, DHCP 로 주소가 바뀌자 QSslSocket 의 호스트명 검증에서 걸린 것이었다.",
    {x:7.1,y:3.7,w:5.38,h:1.05,fontSize:10,color:P.dim,lineSpacing:15});
  T(s,"→  mqtt.json 에 server_name 을 두어 검증할 호스트명만 인증서상의 이름으로 맞추고, 사설 CA 체인 검증과 mTLS 는 그대로 유지했다. 실패 사유별 대처를 README 표로 정리.",
    {x:7.1,y:4.8,w:5.38,h:0.6,fontSize:10,color:P.ok,lineSpacing:15});
  panel(s,M,5.7,CW,1.05,P.panel,P.warn);
  T(s,"보안 고려",{x:M+0.3,y:5.88,w:1.6,h:0.28,fontSize:11.5,bold:true,color:P.warn});
  T(s,"인증서를 발급하는 서비스에 파일 경로를 여는 일이라 세 겹으로 제한했다 — ① 검증된 클라이언트 인증서 필수(SSL_get_verify_result 를 핸들러에서 직접 확인)\n② 파일명만 허용('/' 나 '%' 가 있으면 400, 디렉터리는 고정)   ③ .pcd 확장자만 허용",
    {x:M+1.9,y:5.85,w:CW-2.2,h:0.75,fontSize:10,color:P.dim,lineSpacing:15});
}

/* ─────────────── 14. 04-④ 보안 · 등록 ─────────────── */
{
  const s = newSlide();
  head(s,"04  프로젝트 수행 경과","결과 제시 ④  배포용 등록 절차와 보안 설계");
  panel(s,M,1.45,CW,1.35,P.panel,P.accent);
  T(s,"원칙",{x:M+0.3,y:1.65,w:1,h:0.28,fontSize:12,bold:true,color:P.accent});
  T(s,"접속에 필요한 것(인증서 · 브로커 주소 · 카메라 URL)은 전부 비밀정보다. 저장소에도, 배포본에도 넣지 않는다.\n대신 사용자가 1회용 토큰 하나를 입력하면 나머지를 런타임에 발급·구성한다.",
    {x:M+1.35,y:1.62,w:CW-1.7,h:0.95,fontSize:12.5,lineSpacing:21});
  const flow=[["토큰 입력","등록 화면에서 1회용 토큰 · 기기 이름 · 카메라 접속 정보 입력"],
              ["POST /enroll","실행파일에 내장된 사설 CA(resources/ca.crt)로만 서버 신원 검증"],
              ["자격 수신","CA · 클라이언트 인증서 · 개인키 · 브로커 주소를 사용자 데이터 폴더에 저장"],
              ["mTLS 접속","8883 접속. 권한은 인증서 CN 으로 판정되고 Client ID 는 호스트명+난수로 유일화"]];
  flow.forEach((f,i)=>{
    const x = M + i*3.09;
    panel(s,x,3.0,2.85,1.75);
    num(s,x+0.22,3.18,"0"+(i+1),0.34);
    T(s,f[0],{x:x+0.22,y:3.62,w:2.4,h:0.28,fontSize:11.5,bold:true});
    T(s,f[1],{x:x+0.22,y:3.94,w:2.45,h:0.7,fontSize:9.5,color:P.dim,lineSpacing:13});
    if(i<3) s.addShape(pres.ShapeType.line,{x:x+2.87,y:3.85,w:0.2,h:0,line:{color:P.accent,width:1.5,endArrowType:"triangle"}});
  });
  panel(s,M,4.95,6.0,1.85);
  T(s,"설계 판단 — 카메라 설정은 서버를 거치지 않는다",{x:M+0.25,y:5.12,w:5.5,h:0.28,fontSize:11.5,bold:true,color:P.ok});
  T(s,"처음에는 카메라 설정을 브로커 retained 토픽으로 뿌리게 만들었다. 그런데 카메라는 RPi 와 물리적으로 떨어져 있고 데몬은 카메라를 건드리지 않는다 — 서버를 경유할 이유가 없었다. 배포 경로를 걷어내고 등록 화면 직접 입력으로 되돌렸고, 이후 변경은 재시작 없이 즉시 반영된다.",
    {x:M+0.25,y:5.45,w:5.5,h:1.2,fontSize:10,color:P.dim,lineSpacing:15});
  panel(s,6.85,4.95,5.88,1.85,P.panel,P.soft);
  T(s,"그 밖의 보안 조치",{x:7.1,y:5.12,w:3,h:0.28,fontSize:11.5,bold:true,color:P.accent});
  bullets(s,[
    "Windows 패키징 스크립트가 .key/.crt/.pem, mqtt.json, cameras.json 혼입을 검사해 발견 시 중단",
    "로그아웃 시 이 기기의 인증서·설정을 삭제",
    "RTSP 재연결은 3회(3초·6초)만 시도 — 무한 재시도로 로그·계정 잠금을 유발하지 않는다",
    "cert_dir 을 실행파일 위치 기준으로 해석 — 평문 degrade 를 막는다"
  ],{x:7.1,y:5.45,w:5.38,h:1.25,fontSize:9.5,lineSpacing:14});
}

/* ─────────────── 15. 04-⑤ 배포 패키징 ─────────────── */
{
  const s = newSlide();
  head(s,"04  프로젝트 수행 경과","결과 제시 ⑤  배포 패키징 및 검증 결과");
  const cols=[["macOS  (.app / .dmg)",P.accent,[
      "macdeployqt 로 Qt 프레임워크 + Homebrew dylib 번들링",
      "쓰지 않는 플러그인(QtPdf/QtSvg/VirtualKeyboard) 제거",
      "남은 /opt/homebrew 절대경로를 install_name_tool 로 재기록",
      "codesign ad-hoc 재서명 후 hdiutil 로 .dmg 생성"
    ],"검증 2026-08-14 — Homebrew 잔여 참조 0건, .app 115MB / .dmg 52MB, 환경변수를 전부 지운 상태에서도 정상 기동"],
    ["Windows  (.exe / .zip)",P.ok,[
      "vcpkg + MSVC 2022 x64 로 빌드 (qtbase / ffmpeg / openssl / paho-mqttcpp)",
      "windeployqt 로 Qt DLL·플러그인·MSVC 런타임 수집",
      "windeployqt 가 모르는 Paho/FFmpeg/OpenSSL DLL 을 와일드카드로 수집",
      "비밀정보 혼입 검사 통과 후 zip 생성"
    ],"검증 2026-08-14 — Qt/FFmpeg/Paho 가 설치되지 않은 다른 PC 에서 압축 해제 후 실행 확인"]];
  cols.forEach((c,i)=>{
    const x = M + i*6.17;
    panel(s,x,1.45,5.96,3.6);
    T(s,c[0],{x:x+0.25,y:1.65,w:5.4,h:0.3,fontSize:13.5,bold:true,color:c[1]});
    bullets(s,c[2],{x:x+0.27,y:2.05,w:5.4,h:1.7,fontSize:10.5,lineSpacing:16});
    panel(s,x+0.25,3.85,5.45,1.0,P.okBg,P.ok);
    T(s,c[3],{x:x+0.45,y:4.0,w:5.05,h:0.75,fontSize:10,color:P.ok,lineSpacing:15});
  });
  panel(s,M,5.25,CW,1.5,P.panel,P.warn);
  T(s,"패키징에서 걸렸던 함정",{x:M+0.3,y:5.45,w:3.5,h:0.28,fontSize:12,bold:true,color:P.warn});
  T(s,"·  vcpkg 로 빌드하면 DLL 이 PATH 에 없어 -ExtraDllDirs 없이 만든 zip 이 다른 PC 에서 실행 즉시 죽는다. 스크립트가 중단하지 않고 경고만 남기므로 마지막 경고 목록을 반드시 확인해야 한다.\n·  개발 머신에서의 env -i 실행은 '설정 없는 새 PC' 검증이 아니다 — Qt 가 getpwuid 로 사용자 데이터 폴더를 찾아내기 때문. 신규 사용자 흐름은 그 폴더를 옮겨두고 확인했다.\n·  빈 certs/ 폴더를 배포본 옆에 두면 발급받은 인증서를 가려 MQTT 가 조용히 평문으로 degrade 된다 — 문서에 경고로 명시.",
    {x:M+0.3,y:5.8,w:CW-0.6,h:0.9,fontSize:10,color:P.dim,lineSpacing:15});
}

/* ─────────────── 16. 04-⑥ 피드백 반영 ─────────────── */
{
  const s = newSlide();
  head(s,"04  프로젝트 수행 경과","결과 제시 ⑥  피드백과 반영 내역");
  const fb=[
    ["데몬 파트","계약서 토픽과 실구현이 다르다","토픽 상수를 실구현 기준으로 정렬하고, 계약서와 어긋난 지점을 README 에 경고로 남겨 재확정 시 함께 고치도록 했다"],
    ["아키텍처 리뷰","카메라 설정을 브로커로 배포할 이유가 없다","retained 토픽 배포 경로를 제거하고 등록 화면 직접 입력으로 전환(커밋 2건). 변경은 재시작 없이 즉시 반영"],
    ["현장 실측","센서 높이 기본값이 실측과 다르고 오류코드가 데몬과 어긋난다","기본값을 2400mm → 실측 1805mm 로, 사용자 안전정지 코드를 0 → 100 으로 정정하고 오류코드 표를 데몬 현행에 맞춤"],
    ["펌웨어 파트","프로토콜 v6 — state/daemon 에 STM 진단 카운터, event/error 에 축 정보가 실린다","수신 측 모델을 맞추고, valid=false 인 카운터를 0(정상)으로 그리지 않도록 처리. 오류 로그에 발생 축을 표기해 배선 점검 범위를 좁혔다"]
  ];
  fb.forEach((f,i)=>{
    const y = 1.45 + i*1.32;
    panel(s,M,y,CW,1.18);
    T(s,f[0],{x:M+0.25,y:y+0.42,w:1.5,h:0.3,fontSize:11,bold:true,color:P.accent,align:"center"});
    s.addShape(pres.ShapeType.line,{x:M+1.85,y:y+0.15,w:0,h:0.88,line:{color:P.border,width:1}});
    T(s,"피드백",{x:M+2.05,y:y+0.16,w:1,h:0.24,fontSize:9,bold:true,color:P.warn});
    T(s,f[1],{x:M+2.05,y:y+0.42,w:4.1,h:0.6,fontSize:10.5,color:P.text,lineSpacing:15});
    T(s,"→",{x:M+6.25,y:y+0.42,w:0.4,h:0.3,fontSize:14,color:P.accent,align:"center"});
    T(s,"반영",{x:M+6.75,y:y+0.16,w:1,h:0.24,fontSize:9,bold:true,color:P.ok});
    T(s,f[2],{x:M+6.75,y:y+0.42,w:5.15,h:0.65,fontSize:10.5,color:P.dim,lineSpacing:15});
  });
  T(s,"※ 피드백 출처와 일자는 회의록 기준으로 보완 필요. 위 내용은 커밋 메시지에 남은 변경 사유에서 재구성한 것이다.",
    {x:M,y:6.75,w:CW,h:0.3,fontSize:9.5,color:P.faint});
}

/* ─────────────── 17. 04-⑦ 시연 ─────────────── */
{
  const s = newSlide();
  head(s,"04  프로젝트 수행 경과","결과 제시 ⑦  시연 동영상");
  panel(s,M,1.5,7.4,4.6,"0E1216",P.border);
  s.addShape(pres.ShapeType.ellipse,{x:4.05,y:3.45,w:0.75,h:0.75,fill:{color:P.accentBg},line:{color:P.accent,width:1.5}});
  T(s,"▶",{x:4.05,y:3.45,w:0.75,h:0.75,fontSize:22,color:P.accentBright,align:"center",valign:"middle"});
  T(s,"시연 영상 삽입 자리",{x:M+0.4,y:4.4,w:6.6,h:0.35,fontSize:15,bold:true,align:"center",color:P.dim});
  T(s,"팀별 5~10분 내 · 100MB 이하 · 기능별 소개 음성 포함 · 별도 파일 제출 가능",
    {x:M+0.4,y:4.8,w:6.6,h:0.3,fontSize:10.5,color:P.faint,align:"center"});
  panel(s,8.4,1.5,4.33,4.6);
  T(s,"시연 시나리오 (초안)",{x:8.65,y:1.7,w:3.5,h:0.3,fontSize:13,bold:true,color:P.accent});
  const sc=["등록 마법사 — 토큰 입력부터 인증서 발급·mTLS 접속까지",
            "메인 대시보드 — CCTV 4채널 동시 표시, 좌우 비율 조정",
            "CALIBRATION 탭에서 스캔 파라미터 입력 후 SCAN 실행",
            "진행률 표시(~2Hz)와 상태 전이(SCANNING → EXPORT → IDLE)",
            "스캔 종료 즉시 Top-View 점군 표시, 3D 뷰 전환",
            "DISARM 비상정지 → REARM 재개",
            "IMU 수평 경고 배너 · EVENT LOG 오류 표시",
            "데모 모드 — 장비 없이 동일 흐름 재생"];
  const runs = sc.map((t,i)=>({text:t,options:{bullet:{type:"number"},breakLine:i<sc.length-1}}));
  T(s,runs,{x:8.65,y:2.1,w:3.83,h:3.85,fontSize:10,color:P.dim,lineSpacing:16,paraSpaceAfter:4});
  panel(s,M,6.25,CW,0.75,P.panel,P.warn);
  T(s,"※ 영상은 미촬영 상태. 미구현 구간(발급 서비스 실연동)은 데모 모드 또는 실장비 mTLS 연동 화면으로 대체 촬영이 필요하다.",
    {x:M+0.3,y:6.45,w:CW-0.6,h:0.35,fontSize:11,color:P.warn});
}

/* ─────────────── 18. 05 자체 평가 ─────────────── */
{
  const s = newSlide();
  head(s,"05  자체 평가 의견","기획 의도와의 부합 정도 · 완성도 · 활용 가능성");
  const sco=[["기획 의도 부합도","8","기획한 '한 화면에서 스캔·감시·정지·확인' 은 전부 동작한다"],
             ["결과물 완성도","7","Qt 파트는 배포 가능한 수준. 발급 서비스 실연동이 남았다"],
             ["실무 활용 가능성","7","계약 기반이라 재사용은 쉽지만, 캘리브 품질 표시가 아직 없다"]];
  sco.forEach((c,i)=>{
    const x = M + i*4.11;
    panel(s,x,1.45,3.89,1.6);
    T(s,c[0],{x:x+0.25,y:1.62,w:2.6,h:0.28,fontSize:11.5,bold:true,color:P.dim});
    T(s,c[1],{x:x+2.55,y:1.6,w:1.1,h:0.6,fontSize:34,bold:true,color:P.accent,align:"right"});
    T(s,"/ 10",{x:x+2.55,y:2.15,w:1.1,h:0.25,fontSize:10,color:P.faint,align:"right"});
    T(s,c[2],{x:x+0.25,y:2.42,w:2.4,h:0.55,fontSize:9.5,color:P.faint,lineSpacing:13});
  });
  const boxes=[
    ["잘한 점",P.ok,[
      "인터페이스를 문서로 먼저 고정해 파트 간 재작업을 줄였다",
      "문서와 실구현이 어긋났을 때 실구현을 기준으로 삼고 그 사유를 기록으로 남겼다",
      "'모르는 값' 을 정상으로 표시하지 않는 원칙을 IMU·STM 진단 양쪽에 일관되게 적용",
      "설치 없이 실행되는 배포본을 두 OS 모두에서 실제 검증까지 마쳤다"
    ]],
    ["아쉬운 점",P.danger,[
      "발급 서비스(/enroll)가 뜨지 않아 등록 마법사의 실제 발급 경로를 끝까지 확인하지 못했다",
      "Qt 저장소 커밋이 특정 인원에 집중되어 파트 내 리뷰가 충분치 않았다",
      "단위 테스트가 없다 — 검증이 수동 시연과 데모 모드에 의존한다",
      "점군을 그리기만 할 뿐 벽·기둥 에지 추출까지는 가지 못했다"
    ]],
    ["추후 개선 방향",P.accent,[
      "발급 서비스 연동 마무리 후 신규 PC 등록 전 과정 검증",
      "CALIBRATION 탭에 캘리브 품질(NCC·edge_rmse·extrinsic) 패널 추가 — 발행 토픽 확정 대기",
      "에지 추출과 스캔–영상 정합 결과의 시각적 확인",
      "설치 프로그램(Inno Setup) 및 인증서 무효화(CRL) 대응"
    ]]
  ];
  boxes.forEach((b,i)=>{
    const x = M + i*4.11;
    panel(s,x,3.2,3.89,2.75);
    T(s,b[0],{x:x+0.25,y:3.38,w:3.4,h:0.3,fontSize:13,bold:true,color:b[1]});
    bullets(s,b[2],{x:x+0.27,y:3.78,w:3.35,h:2.05,fontSize:9.5,lineSpacing:14});
  });
  panel(s,M,6.1,CW,0.85,P.panel,P.soft);
  T(s,"느낀 점",{x:M+0.3,y:6.28,w:1.3,h:0.28,fontSize:11.5,bold:true,color:P.accentBright});
  T(s,"여러 파트가 물리는 시스템에서는 코드보다 인터페이스 합의가 먼저였다. 문서와 실물이 다를 때 무엇을 기준으로 삼고 그 판단을 어디에 남길지가 개발 속도를 갈랐다는 점이 가장 크게 남는다.",
    {x:M+1.6,y:6.28,w:CW-1.9,h:0.5,fontSize:11,color:P.dim,lineSpacing:16});
  s.addNotes("점수와 느낀 점은 초안. 팀 협의 후 각자 문장으로 교체할 것.");
}

/* ─────────────── 19. 마무리 ─────────────── */
{
  const s = newSlide();
  let seed=13; const rnd=()=>{ seed=(seed*9301+49297)%233280; return seed/233280; };
  for(let i=0;i<110;i++){
    const x=0.4+rnd()*12.5, y=0.4+rnd()*6.7, d=0.03+rnd()*0.05;
    const c=[P.accent,P.faint,P.faint,P.ok][Math.floor(rnd()*4)];
    s.addShape(pres.ShapeType.ellipse,{x,y,w:d,h:d,fill:{color:c},line:{color:c,width:0}});
  }
  panel(s,2.6,2.3,8.13,2.9,P.panel,P.accent);
  T(s,"감사합니다",{x:2.6,y:2.9,w:8.13,h:0.9,fontSize:40,bold:true,align:"center"});
  T(s,"SPATIAL·VMS  —  TEAM Oppenheimer",{x:2.6,y:3.85,w:8.13,h:0.35,fontSize:15,color:P.accentBright,align:"center"});
  T(s,"github.com/VEDA-4th-Oppenheimer/QT",{x:2.6,y:4.35,w:8.13,h:0.3,fontSize:11,color:P.faint,align:"center"});
}

pres.writeFile({ fileName: "docs/SPATIAL-VMS_결과보고서_초안.pptx" })
  .then(f=>console.log("written:",f));
