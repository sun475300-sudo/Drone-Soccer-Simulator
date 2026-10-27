# Imperial Wings 검증 기록

## 통합 개발 계획서 문서 대조 — 2026-10-27

`a59aa84` 구현 상태를 기준으로 [전체 계획서](master-development-plan.md)를 작성했다. 기존 고유 기능326개/28개 분야, 피드백1~12번의 누락·중복, 계획서 내부/상대 링크63개와 명시적 앵커 링크43개를 확인했다. 오래된 물리 문서의 Acro·정적 골대·전력/조종기 설명도 현재 소스와 대조해 수정했다.

이번 변경은 문서뿐이다. 게임 소스/콘텐츠 입력 지문은 `c7a12551fe7381f834c09ba22f753d0964f5ff191732e9149e9eeb7fab7cdbf5`로 유지되며, 새 게임 빌드·실물 시험·AI/탑건 구현을 수행했다는 의미가 아니다. 아래 버전별 실제 검사 결과를 그대로 보존한다.

<a id="r10-controller-response"></a>
## v0.7.6-dev 조종기 레이트·Expo·아밍키 — 2026-10-27

별도 R10 / `codex/pilot-feedback`에서 구현했다. 최종 입력132개·소스 지문
`c7a12551fe7381f834c09ba22f753d0964f5ff191732e9149e9eeb7fab7cdbf5`으로 Editor와
Shipping build/cook/stage/archive를 통과했다. 빌드 메타데이터의 커밋은 부모 `89b3e52`,
dirty=true이며 게시 후 커밋에서 빌드했다고 주장하지 않는다. 게시 코드와는 입력 지문을 비교한다.

- 응답 곡선·기본 결합 각속도 한도·버튼 학습/누름·ARM 조건 **4,881개**, 기존 보정 **205개** 통과.
- Editor·Shipping·한글/공백 새 압축 해제본 각각 **13종·66개 검사** 통과. 취소·저장·재실행,
  잘못된 파일8종·쓰기 실패, 실제 ANGLE 목표(Yaw84deg/s)·Roll 전 구간30deg/s 한도·모터 추력 확인.
  기존 파일의 정상 교체와 Windows 공유 잠금으로 교체에 실패했을 때 원본 바이트/실행 설정 보존도 포함한다.
- 최종 Editor/Shipping 각각 **입력 안전29개·Mode1/2 UI/JSON20개** 통과. Shipping20/60/240FPS에서
  실제10대 ANGLE 몸체 수평 복원 통과. 중립 후 최악 기울기는 각각0.609084/0.204867/0.231259도.
- 최종 Shipping1600×1000 GPU 렌더에서 설정 화면의 곡선·버튼·문자와8개 설정 검사를 확인했다.
  화면 밖 렌더 시험이며 사용자 마우스 클릭·실물 USB 스틱/버튼 시험으로 확대하지 않는다.
- 새 ZIP CRC·파일 해시, 새 설치 경로의 실행기/동봉 APF/신규 사용자 저장 **13개** 통과.
  개발 도구를 자식 프로세스 PATH에서 제외했다. 다른 물리 PC의 실행을 검증한 것은 아니다.
- 로컬 QA ZIP `ImperialWings-Windows-v0.7.6-dev-controls-r3.zip`:374,833,197bytes,
  SHA-256 `2d1ebea61c69759cc768e3d45d3f0e72f1a22489d2876021e904055e87e0e54c`.
  공식 다운로드 v0.7.4-r2는 교체하지 않았다. 개인 설정 파일은 ZIP/소스에 포함하지 않았다.

실패도 보존한다. 첫 Roll 검사는 자세 복원 중 부호 반전을 오판했으므로 전 구간 절대 각속도와
한도 도달 검사로 바꿨다. r1의 실물 프로필 연결 검사는 저장 파일은 찾았지만 해당 장치 활성화에
실패했다. 실물 통합은 **미완료**이며 새 사용자13개 검사와 구분한다. 기존 실물 보정 원본은
R9 이전 해시와 동일함을 확인했다. 다른 유지 스위치를 OFF로 강제하던 초기 학습 절차는
한 버튼의 새 누름을 구분하도록 보완하고, 그 최종 소스에서 위 검사를 다시 수행했다.
추가 검토에서 UE의 일반 파일 이동 함수가 대상 파일을 먼저 삭제함을 확인해 Windows의
같은 볼륨 교체 호출로 바꿨다. r3 패키징 완료 전에 시작한 시험1회는 구 실행본의 지문 불일치로
거부됐다. 완성된 r3 실행본으로 모든 관련 검사를 다시 수행했으며 구 결과를 최종 통과로 세지 않았다.

[설정 방법·실행 화면](controller-response-2026-10-27.md) ·
[원시 결과/해시를 포함한 기록](verification/controller-response-2026-10-27.json) ·
[12개 피드백 및 미완료 목록](pilot-feedback-2026-10-27.md).

## v0.6.1 조종기 간소 화면·실행 GIF·수집 안내 — 2026-10-26

별도 작업 사본 `E:/ImperialWingsSimulator-PlanReview-20261026-7f9221c1`에서 구현·컴파일했다. 원 작업 사본의 소스·사용자 보정을 변경하지 않았다. [v0.6.1 배포·증거·한계](releases/v0.6.1.md)에 전체 결과와 파일 지문을 기록한다.

- Editor/Shipping build/cook/stage/package/archive ExitCode0. 빌드 입력 지문 `93ae92873be1bb69bd087792e146375d32ff9ae407c54d48eac5e6f095206d86`.
- 실제 Windows1600×1000 창의 조종기/수집 UI70개 검사,보정 수학161개,실제 Chaos30/60/120FPS 검사 통과. 다른 해상도 전체 검사·실물 Boxer 비행 완료를 뜻하지 않는다.
- 현재 열거된 장치는 `0416:8258`뿐. Boxer를 기대한 첫 시도는 장치 부재로 실패했다. 별도 실행에서 실제 장치 선택·정지 입력 거부·취소·설정 보존을 검사했으며 새 보정값을 만들지 않았다.
- F6 실제 JSONL 시작/sample/stop·10대/빌드 지문, F5 CSV와 실제 장치 JSON 저장을 확인했다. README에 세 파일의 차이·필드·단위·경로·공유 절차를 설명했다.
- 실제 게임 클라이언트48장을800×500 GIF로 인코딩. 자동 물리 검사 장면이며 USB 입력이 아니다.15.08초·7,127,056bytes. 초기/중간 프레임과 간소/고급 UI를 시각 검토했다.
- 실행 ZIP을 한글/공백 경로에 실제 해제하고11개 수동 배포 검사 통과. CRC/파일 지문·실행기 경로 조회·새 사용자 저장/진단·사용자 보정 배제 확인. 자율 서버 시작은 재시도하지 않았다.
- ZIP372,189,311bytes, SHA-256 `edfb201ded7a595adf843feb3a6c960caa302a04ac0a3121c6dd9941e42dd1b3`.
- 타 시뮬레이터4종의 공식 기능 조사와 제공 경기 영상의 표본 프레임 검토를 문서화했다. 기존 물리 상수는 변경하지 않았고 실기체와의 오차 검증·모든 영상 복기 완료로 표시하지 않는다.
- 최종 문서의 상대 링크/앵커135개·기능 번호326개 중복 없음·README PowerShell 예시3개와 변경 시험 스크립트 구문 오류0개·48프레임 GIF/8MiB 미만·`git diff --check` 확인.

아래 기록은 각 과거 버전의 별도 결과다.

검증일: **2026-10-26 KST**, R4. Windows / UE5.8.3 / Visual Studio2022 MSVC14.44.35229 / RTX5070Ti / RAM32GB. 제공 스타터는 Downloads에 보존했으며 실제 프로젝트는 `E:\ImperialWingsSimulator`다. R2의 과거 결과를 이번 변경의 검증 결과로 대신하지 않는다.

## 현재 빌드·렌더

- C++ 에디터 빌드 성공: `Saved/editor-r4c-build.log`.
- 맵·PBR 재질/카본 직조 질감 생성: `Saved/content-r3-build.log`,0 errors/0 warnings.
- Win64 Development build/cook/stage/package 성공: `Saved/package-r4c-console.log`.
- FIDA16×7×4.5m 경기장,40cm 구형 보호구,LOS 기본 카메라,역할 패드·앞뒤 LED를 실제 패키지에서 렌더했다. [경기장](images/arena-r4.png),[기체](images/airframe-r4.png).
- R4의1920×1080 경기장 장면에서1,323개 게임 프레임 평균11.343ms,95백분위13.266ms,평균88.16FPS. 실제 패키지의 오프스크린 GPU 렌더 기록이며 별도 headless 물리 시험이 함께 실행됐다. 화면 입력 지연이나 RTX5060 성능값이 아니다. 원문은 패키지 `Saved/render-profile.json`.

## 실제 Chaos 물리·네트·골대 시험

`Tests/run-runtime-tests.ps1 -Packaged`는 매번 새 결과 폴더를 만들고 실제 프로세스 종료와 새 JSON 생성·내용을 함께 확인한다. 오래된 파일을 성공으로 재사용하지 않는다. R4 결과 폴더는 `Saved/Verification/runtime-20261026-093559-730`이다.

| 고정 게임 계산 간격 | 마지막 목표 오차 | 네트 모델 최대 처짐 | 보이는 와이어 최대 처짐 | 반동 속도 |
|---|---:|---:|---:|---:|
| 1/30초 | 0.456cm | 5.297cm | 5.021cm | 1.1171m/s |
| 1/60초 | 0.284cm | 6.721cm | 6.437cm | 1.0507m/s |
| 1/120초 | 0.202cm | 7.467cm | 7.404cm | 1.0695m/s |

세 간격 모두 호버·수평 목표 이동,DISARM 낙하·바닥 접촉,25m/s 초기 속도의 보조벽 CCD 접촉,5m/s 네트 충돌·감쇠/반동·20cm 이하 처짐,스트라이커의 실제 골대 통과·정확히1골·오프사이드,10대 유지와 CSV 내보내기를 통과했다. 네트 충돌과 골대 시험의 첫 배치·충돌 초기 속도는 명시적인 fixture다. 이후 이동은 실제 모터 힘/Chaos 접촉이며 시험 중 위치를 따라 옮기지 않는다.

무게중심 오프셋에 루트 스케일0.4가 중복 적용되는 문제와 UE의 `InertiaTensorScale` 의미를 확인해 수정했다. Chaos 기본값에서 꺼져 있던 강체 자이로 결합도 명시적으로 켰다. 회전 중 몸체의 오일러 축 결합과 별도의 회전자 각운동량 토크를 함께 적용한다. 현재 질량0.9kg,전후 균형·중심 아래7.5mm와 계산 관성 대각을 실제 Chaos에 적용하고 시작 시 읽어 로그를 남긴다. 제조사 실측 질량 배분은 아니다. 프레임별 접촉 응답 차이가 남으므로 이 표를 모든 FPS에서 완전히 동일한 물리의 증거로 해석하지 않는다.

## 독립 시험

| 대상 | 결과 |
|---|---|
| 규정 상태·골대 통과·역방향·오프사이드·시간 | MSVC51개 통과 |
| USB 범위·중립·반전·데드존·연결 손실 수학 | MSVC115개 통과 |
| ESC·배터리·항력·근접/후류·센서·PID·접촉·관성 | MSVC83개,실패0 |
| 실제 upstream APF와HTTP·동시 요청·귀환 유도 | Python8개 통과 |

APF40개 동시 요청의 계산 중앙값2.472ms,테스트 전체1.414s. 이 유도 인터페이스의 이상적인 외부 제어기 시험은 Unreal 기체 전체의 비행 검증과 구분한다. 실제 upstream 원본 커밋은 `1e9d826265069696d7e415c7778d17483d7a8580`이며 원본 작업 사본은 수정하지 않았다.

## 화면 조작과 발견한 문제

마우스가 버튼 위에 있어도 클릭이 전달되지 않는 원인을 UE5.8 standalone viewport의 `NoCapture` 입력 경로에서 확인했다. 현재는 누르는 동안만 캡처하고 커서를 보인 채 잠그지 않는다. Canvas 버튼·목록은 하나의 명시적 포인터 경로로 처리해 중복 클릭을 막는다. 역할별 명칭·LED 색상과 목록 선택도 연결했다. F10으로 넓은 LOS 연습 화면을 사용할 수 있다.

R3h 실제 user32 키보드/마우스 검사에서는 **59개 확인 항목**을 통과했다. 0.2m에서4.1991m 수동 상승,스로틀0.5309,높은 스로틀 재ARM 거부,USB 미보정 활성화 거부,패널 뒤 명령 차단,10대 AUTO 평균고도2.34926m,HOLD2초 동안 최대하강2.36cm,RETURN 진입을 확인했다. 귀환 대기 중 다른 앱이 전면에 와 입력을 중단했으며 전체 흐름을 통과했다고 기록하지 않았다. 근거는 `Saved/Verification/ui-20261026-181339-226/result.json`이다. R4의 전체 재검사 결과는 완료 후 아래에 추가한다.

에디터 `-game` 장시간 검사에서 PythonScriptPlugin의 GC 콜백 접근 위반도 한 차례 발생했다. 배포 실행 파일의 검사는 에디터 Python이 포함되지 않는 실제 패키지를 사용한다. 해당 에디터 내부 오류를 프로젝트 물리 성공으로 숨기지 않는다.

## Boxer 실물 상태

Windows PnP의 상위 장치까지 확인했다. `0416:8258`은 **Archon M1 PRO MAX**였고 Boxer가 아니다. 사용자 재연결 후 `1209:4F54`의 실제 USB 제품명이 **Radiomaster Boxer Joystick**으로 확인됐다. WinMM슬롯1,6축·24버튼이며 실제 `joyGetPosEx` 데이터가 수신된다. 진단은 `Tests/radio_probe.py`로 수행하고 원본·장치 경로는 로컬 Saved에만 보관한다.

정지 잡음·초기 패킷 변화와 네 스틱의 전 범위 검증은 다르다. 현재 기록만으로 네 조종 축의 전 범위·방향·중립·최저 스로틀 보정을 완료했다고 주장하지 않는다. 실제 범위·배정이 유효하기 전까지 USB 조종을 자동 활성화하거나 가짜 프로필을 만들지 않는다. [연결·보정 절차](BOXER_SETUP.md)를 참고한다.

## 정밀도와 경기 기능의 한계

보호구는 구체 충돌체·반발/마찰과 제한된 시각 압축을 사용한다. 네트는 국소 스프링·댐퍼와 가우시안 와이어 표시이며 분포 장력/FEM 해석이 아니다. 표면 근접과 다운워시는 경험적 근사다. 회전수 변화의 추가 각운동량 항,정교한 로터 항력/진동,실기체 추력·배터리·관성 측정 보정은 남아 있다. 센서 융합·독립 고정 물리 스레드 PID는 미구현이다. 상세 범위는 [물리 모델](physics-models.md)에 명시했다.

규정 코어는 기본 세트·득점·오프사이드·경기 단계 ARM 제한을 구현한다. 인간 심판의 전체 경고/퇴장,실제 페널티 경기,공격수 승계,VAR,연장·승부치기까지 완성된 공식 대회 운영 시스템은 아니다. 자율 APF는 Practice 훈련 보조이며 공식 경기는 LOS 수동 조종이다.
## R4 전체 네이티브 UI 재검사 완료

실제 패키지 프로세스에서 **96개 항목 전부 통과**, ok=true다. 근거: Saved/Verification/ui-20261026-183035-815/result.json 및 같은 폴더의 실제 화면·CSV. 수동 상승은0.2→4.2258m,스로틀0.5279,10대 AUTO 평균고도2.34279m,HOLD 최대하강2.15cm였다. RETURN은11.82초 안에10대가 홈 근처 바닥으로 돌아와 모두 DISARM·자율 해제됐다. LOS/외부 관찰/경기장 시점1·2·3,마지막IW-10 목록 선택 후 내보내기·초기화·재이륙까지 통과했다. 이 결과는 앞선 입력 간섭으로 중단된 기록과 분리해 보관했다.

검사 뒤에는 전체 초기화·IW-01 선택·LOS 복귀 후 USB 패널에서 실제 Boxer에 대응하는6축·24버튼 장치를 선택했다. Saved/Verification/r4-boxer-panel.png에 실제 연결과 원시 축 표시를 남겼다. 보정 또는USB 활성화를 대신 성공 처리하지 않았으며 모든 기체는 DISARM 상태다.
최종 R4c에서도 강체 자이로 결합을 켠 상태로30/60/120Hz 물리 시험을 다시 통과했다. 네이티브 UI 재검사도 `ok=true`,96항목 전부 통과했다. 최종 근거: `Saved\Verification\ui-20261026-183656-498/result.json`. 소스·패키지·최종 검사 이후 물리 코드를 추가 변경하지 않았다.

지속 입력 기록을 준비한 뒤 사용자가 스틱을 다시 움직였고,X/Y/Z/V 네 아날로그 축에서 실제0–65535 끝값을 모두 관찰했다. 원본은 `Saved/Verification/boxer-final-range.json`과 시각별 실제 입력 journal이다. 물리 스틱별 정확한 채널 배정과 양의 방향·최저 스로틀 확정은 끝값 수집과 구분하며,미확정 프로필을 만들지 않는다. 첫 패킷 중심값을 이동으로 오인하지 않도록 읽기 전용 진단에 시작250ms 제외 구간을 추가했다. 별도의 HID 제품 정보 조회에서도 제품명이 Radiomaster Boxer Joystick으로 일치했다.
최종 설정 패키지 R4d에서는 F5의 엔진 디버그 화면 전환 바인딩을 제거했다. 실제 F5 입력으로 새10행 CSV 생성,LOS1·Boxer연결·10대DISARM을 확인했고 ShaderComplexity 모드로 바뀌지 않았다. `Saved/Verification/r4d-input-check.log`, `r4d-boxer-panel.png`에 근거를 보관했다. 이 변경은 입력 설정과 읽기 전용 진단에 한정하며 R4c의 물리 코드·자산을 바꾸지 않는다.

## R5 Mode 2 안내 보정과 설정창 입력 분리

사용자가 왼쪽 세로 스로틀인 Mode 2를 확인했다. 축 순서를 추측하는 대신 Roll→Pitch→Yaw→Throttle을 한 단계씩 실제로 움직여 식별하는 안내 보정을 추가했다. 매 단계의 준비/수집을 분리하고,안정 구간 중앙값으로 범위와 마지막 유지 방향을 판단한다. 동시 축 이동·중복 축·불충분 범위·불안정 끝값·높은 스로틀·연결/기능 변경은 거부한다. 마지막 중립 확인·검토·저장·USB 활성화는 각각 구분된다. 취소·실패 시 기존 프로필은 유지한다.

- 에디터 빌드 성공: `Saved/editor-r5-radio-build.log`.
- Windows build/cook/stage/package 성공: `Saved/package-r5-radio-console.log`.
- 독립 보정 수학 검사 **159개,실패0**. 이 내부 수치 자료는 실물 보정 파일을 만들지 않는다.
- 새 실제 패키지의 네이티브 키보드/마우스 검사 **119개 전부 통과**,`ok=true`: `Saved/Verification/ui-20261026-195728-781/result.json`.
- 실제 Boxer 드라이버 ID `1209:4F54` 선택·라이브 수집 샘플을 확인했다. 실제 정지 입력으로는 첫 단계가 통과하지 않았고,닫기 후 단계 초기화·10대DISARM·USB 비활성·보정 파일 미변경을 확인했다.
- 설정창이 열린 동안 Enter가 AUTO FLEET을 실행하고 Space가 ARM될 수 있던 경로를 차단했다. 실제 Enter는 첫 단계 수집을 시작했고 Space·위 방향키로 기체가 ARM되거나 키보드 스로틀이 누적되지 않았다.
- 기존 수동 상승·ARM 제한·10대 자율 훈련/HOLD/귀환·착륙·선택·세 카메라·초기화 재검사도 함께 통과했다. 비행 힘 모델·기체/경기장 자산은 R4c에서 변경하지 않았으며,30/60/120Hz 물리 결과는 위 R4 기록으로 구분한다.

검사 후 LOS1·IW-01·10대DISARM·Boxer선택·USB비활성으로 복귀하고 안내 보정 첫 준비 화면을 열어 뒀다. `Saved/Verification/r5-mode2-ready-live-fleet.csv`로 실제 상태를 확인했다. [실행 중 안내 화면](images/radio-mode2-setup.png).

**아직 실물 스틱별 축·방향·중립·최저 스로틀 보정과 USB 수동 비행을 완료하지 않았다.** 현재 검사는 보정 UI·계산·미완료 입력 거부·취소 흐름이며,사용자의 실제 단계별 스틱 움직임을 대신하지 않는다. CSV에는 검사용 `radio_setup_phase`(0대기/1준비/2수집/3중립/4검토/5저장완료/6중단),현재 채널·수집 수·드라이버ID를 추가했다.

## R6 / v0.4.0 — 전 기체 ANGLE·시작 위치·실물 보정

사용자가 전 기체 ANGLE 전환을 요청했다. 수동 Roll/Pitch는 최대35° 목표 기울기의 외부 자세 루프를 거쳐 기존 각속도PID·모터 믹싱으로 전달한다. 대각 스틱도 총 목표 기울기를35°로 제한한다. 중립은 수평 복원,Yaw는 각속도,고도는 수동 스로틀이다. 충돌로 생긴 순간적인 실제 기울기를 강제로 클램프하거나 물리 위치를 옮기지 않는다. 현재 루프는 게임 프레임의 PrePhysics이며 독립 고정 스레드 제어기를 구현했다고 주장하지 않는다.

- 에디터 빌드: `Saved/editor-r6-angle-build.log` 성공.
- Windows build/cook/stage/package: `Saved/package-r6-angle-console.log` 성공.
- 독립 물리96·보정161·규정51·요청 순서16개 검사 통과. 실제 upstream APF/HTTP 브리지10개 검사 통과.
- 실제 패키지의10대 수동 ANGLE 기체에서20/30/60/120/240 고정 게임FPS 모두 초기 기울기 복원·스틱 기울기 응답·해제 후 수평 복원 통과.

| 고정 게임FPS | 스틱 해제1.8초 후10대 중 최대 기울기 |
|---|---:|
| 20 | 1.3384° |
| 30 | 0.0526° |
| 60 | 0.0429° |
| 120 | 0.1243° |
| 240 | 0.1185° |

근거: `Saved/Verification/angle-20261026-115414-501/results.json`. 시험은 처음에만 명시적으로 초기 자세를 배치하고,비USB 시험용 스틱 명령과 수직 보상을 사용해 자세 제어를 분리했다. 이후 운동은 실제 모터/Chaos이며 USB 비행·실기체 성능·렌더링FPS 측정 결과가 아니다.

새 실행 창의 **69개 네이티브 화면 검사 전부 통과**. 실제10대의 초기XY·홈·그려진 패드 인스턴스의 월드XY·역할·중앙선을 향하는 초기 방향,10개 역할 각각의 외부LOS 카메라 위치를 대조했다. 근거: `Saved/Verification/ui-20261026-210203-219/result.json`과 같은 폴더의CSV/역할별 화면. 이 검사는 배치/수동/USB 기본 제한 범위이며 R4의 전체AUTO/HOLD/RETURN96개 검사를 R6에서도 반복했다고 주장하지 않는다. [실제 ANGLE LOS 화면](images/angle-los-r6.png).

실물 조작 과정에서는 한쪽 이동만 수집해도 안내 단계가 넘어가 마지막 중립을 거부할 수 있음을 발견했다. 시작 시 실제 중립을 기억하고 양쪽 범위를 확인한 후 단계 완료를 허용하도록 수정했다. 단발값을 끝값으로 꾸미거나 기준을 낮춰 완료시키지 않았다.

실제 사용자 조작으로 확인된 Roll=X,Pitch=Y,Yaw=V,Throttle=Z,모두NORMAL을 수동 범위 수집에 적용해 **6,454개 실물 입력**의0–1 끝값·중립·최저 스로틀을 검증하고 저장했다. X/Y/V중립은0.499016/0.499504/0.497551,최저Throttle=0이다. 저장된 실제 프로필은 로컬Saved에만 있다. `boxer-after-ManualSave.png`와 실제CSV의 `radio_calibrated=1`로 저장 성공을 확인했다. 이어 USB활성화·최저 스로틀ARM도 `boxer-after-EnableAndArm.png`와CSV로 확인했다. [저장 직후 실제 화면](images/boxer-calibrated-r6.png).

연속USB비행 검사는 포커스 상실로 중단됐다. `boxer-flight-20261026-203625/observation.json`의32개 샘플은 DISARM·키보드 상태이며 비행 성공으로 계산하지 않는다. 케이블 분리·재연결·기체 반응별 방향·USB지연 검증은 남아 있다.

## R6 빌드 출처와 작업 폴더의 추가 변경

화면·CSV·물리 결과와 패키지 옆manifest에 버전·빌드UTC·기반커밋/수정표시·빌드 입력SHA-256을 기록한다. 검증한 입력은62개이며 SHA-256은 **`01fa67e132697ad826c30bec585ca45242e3c9df97be4347689efd63046237c9`**다. C#빌드 계산과 독립 PowerShell 계산이 일치함을 확인했다. 소스 텍스트의CRLF/LF는 정규화하며 자체자산은 바이트 해시를 사용한다. 에디터와 게임manifest는 다른 파일명으로 저장한다.

빌드 이후 작업 폴더에 `ImperialPresentationEffects.{h,cpp}`와 `ImperialGameMode.cpp`의 효과 생성 변경이 추가됐다. 이 추가 작업은 덮어쓰지 않았고 검증한62개 입력과 구분한다. 원래GameMode와 효과 파일 제외 조건으로 재계산한 해시는 위 빌드값과 정확히 일치했다. 화면검사에는 이 명시적인 빌드 스냅샷을 지정했고 `workspace_matches_runtime=false`를 숨기지 않고 기록했다. 새 효과의 성능·시각 품질을 이 검사로 보증하지 않는다.

자율 통신은 세션UUID·명령세대·요청번호를 추가하고,모드/초기화 전환 시 대기 요청을 취소한다. 이전 콜백이 새 요청 대기 상태를 해제하지 못하며,서버도 이전세대/중복 요청을409로 거부한다. 임무·귀환에서2초 동안 유효 응답이 없으면 현 위치HOLD로 전환하고 복구만으로 임무를 재개하지 않는다. API/순서 수학·HTTP 검사와 UE컴파일은 통과했으나 실제UE귀환 도중 장애 검사는 남아 있다.

자동 승인 검토가 기존 브리지 재시작 및 별도 브리지/게임 창 시작 명령을 “blocked by policy”로 거부했다. 해당 명령을 다른 실행 수단으로 우회하지 않았고 사용자가 새 실행 파일을 직접 열었다. 기존브리지의 프로토콜과 맞지 않아 R6관제는SDACS오프라인 상태였으며,수동ANGLE·배치 검사는 브리지 없이 수행했다. 보안창/커서 이동으로 중단된 추가USB준비 검사는 실패 기록으로 별도 보관한다.

사용자가 보안창을 처리하고 입력을 멈춘 뒤 R6 USB준비 검사를 완료했다. **45개 항목 모두 통과**: `Saved/Verification/ui-20261026-211420-879/result.json`. 실제Boxer의 저장 프로필을 불러왔고 최종CSV에서 `input_source=USB,radio_connected=1,radio_calibrated=1,radio_enabled=1,armed=0,flight_mode=ANGLE,throttle=0`을 확인했다. 전체10대는DISARM이다. 이45개를 앞선69개 배치 검사와 중복 없는114개라고 합산하지 않는다.

## 2026-10-26 시각 효과·화질 개선

별도 `ImperialPresentationEffects` 액터와 GameMode 스폰 연결을 추가했다. 바닥 접촉 충격량 기반 먼지, 실제 추력·바닥 추적으로 제한한 저고도 로터 먼지, 유한 인스턴스 풀, 사진식 색 보정·낮은 bloom·넓은 보조광과 Balanced/High/Cinematic을 제공한다. 비행 힘·득점·ARM·USB 코드에는 이 변경을 넣지 않았다. 상세 범위는 [시각 효과 설명](presentation-upgrade.md).

- 최종 에디터 C++ 빌드 성공: `Saved/presentation-editor-build.log`.
- 에디터 효과 수명/풀 검사 3단계 통과: `Saved/Verification/presentation-20261026-115828-013`. 최대96/192/256, 수명 종료 후0개.
- 새 Windows 패키지 build/cook/stage/package/archive 성공: `Saved/presentation-isolated-package-console.log`, ExitCode0.
- 기존 `dist/Windows`가 실행 중이어서 첫 archive는 파일 잠금으로 실패했다. 실행 중인 게임을 종료하지 않고 새 패키지를 `dist/PresentationUpgrade/Windows`로 분리했다. 기존 런처는 이 새 경로로 전환하지 않았다.
- 패키지 효과 수명/풀 검사 3단계 통과: `Saved/Verification/presentation-20261026-120323-271`의 각 새 JSON ok=true.
- 패키지 실제 Chaos60Hz 시나리오 ok=true: `Saved/Verification/presentation-packaged-physics-60.json`. 호버 오차0.284cm, 바닥/벽·네트 접촉 및 실제 스트라이커 득점 통과, 네트 최대 처짐6.721cm.
- 에디터 GPU1920×1080 전후 렌더를 확인했다: `Saved/Verification/presentation-before.png`, `presentation-after.png`.
- 먼지 풀 시험은 명시적 가상 충격으로 시각 수명/상한을 검사한다. 실제 접촉 외형의 정밀도·실기체 먼지·실측 손상 검증을 뜻하지 않는다.
- 실제 USB비행, 전체 화면 조작, 지속 GPU 성능 비교는 이번에 재검사하지 않았다. 기존 30/120Hz·UI 수치를 이번 변경의 새 검증으로 재사용하지 않는다. 모터 고장·보호구 실제 파괴·역할 승계 구현은 별도 작업이다.
- 새 패키지1920×1080 렌더도15초 예열 후 확인했다: `Saved/Verification/presentation-packaged-warm.png`. 초기6초 캡처에는 HUD 일부 글리프가 미완성으로 보였고15초에는 정상 표시됐다. 초기 글꼴/렌더 예열 문제는 추가 확인 대상으로 남기며 전체 UI 시험 통과로 대신하지 않는다.

## 스트라이커 중앙 배치·양쪽 하단 띠 — 2026-10-26

사용자 요청을 적용했다. 양 팀 Striker는 자기 시작선 정중앙(x=±6.5m,y=0)이다. 조종자 기준 왼쪽부터 Libero/Guide/Striker/Sweeper/Keeper,간격1.25m다. 공유 배치 함수로 기체·홈·바닥 패드·LOS 위치를 동기화했다.

양 팀 Striker 초록/Guide 분홍 하단 띠를 좌우 각각2개,기체당4개 추가했다. 기존 역할LED·팀LED·S/G태그와 별개이며 비충돌 메시다. 흔들림은 시각 근사이며 cloth·파손·탈락·득점 자격 연동은 구현하지 않았다.

- 에디터 C++ 빌드 성공: `Saved/role-layout-editor-build.log`.
- 실제10대 패드·초기화·LOS 위치·중앙 Striker·띠 개수 검사 ok=true: `Saved/Verification/center-striker-ribbons.json`.
- 60Hz 실제 물리 시나리오 통과: `Saved/Verification/runtime-20261026-124018-448/results.json`,호버 오차0.341cm,바닥/벽·네트·실제 득점 통과.
- 실제1920×1080 근접 렌더 확인: `Saved/Verification/lower-role-ribbons.png`.
- 별도 Windows build/cook/stage/package/archive 성공: `Saved/role-layout-package-console.log`,ExitCode0.
- 새 패키지의10대 배치·초기화·LOS·띠 검사 통과: `Saved/Verification/role-layout-20261026-124413-578/result.json`.
- 실행 파일은 `dist/RoleLayoutUpgrade/Windows/ImperialWingsSimulator.exe`. 기존 실행 중인 게임을 종료하거나 교체하지 않았다. 실제 USB 비행·전체 UI·새 배치의 자율10대귀환은 이번에 재검사하지 않았다.
- `Tests/imperial-ui-smoke.ps1`의 기존 선형 역할 순서 기대값은 새 중앙 배치로 수정했다. 이 수정 자체를 전체 화면 검사 통과로 기록하지 않는다.

## 2026-10-26 오늘 최종 Windows 통합 빌드

현재 구현을 합친 v0.5.0을 `Scripts/Build.ps1 -Package`로 새로 build/cook/stage/package/archive했다. 미래 계획 전체의 구현 완료를 뜻하지 않는다.

- 실행 파일: `dist/Integrated/Windows/ImperialWingsSimulator.exe`.
- 바탕화면: `C:/Users/User/Desktop/Imperial Wings 2026-10-26.lnk`. 별도 서버를 시작하지 않는 직접 실행 바로가기다.
- 소스와 패키지 빌드 입력SHA-256 일치: `d5f05e26ebc0df6bdfdff5d4206802bfa376264ba9c3f0b4253203179ba31e10`.
- 빌드 ExitCode0 및 bootstrap 실행 ExitCode0,1920×1080 GPU 화면 확인: `Saved/Verification/final-build-20261026-124755-754`의 build-console.log/final-render.log/final-windows.png/final-summary.json.
- 새 패키지30/60/120Hz 실제 물리 검사 전부 통과: `Saved/Verification/runtime-20261026-124923-369/results.json`.
- 새 패키지10대 실제 패드/초기화/LOS/하단 띠 검사 통과: `Saved/Verification/role-layout-20261026-124923-365/result.json`.
- 새 패키지Balanced/High/Cinematic 효과 검사 통과: `Saved/Verification/presentation-20261026-124929-465`.
- 기존 실제RadioProfile을 새 패키지Saved/Config에 복사했다. 기존 파일은 덮어쓰지 않았고 새 보정 데이터나 USB 자동 활성화를 만들지 않았다.
- 자율훈련 브리지 백그라운드 시작 명령은 자동 승인 검토에서 blocked by policy로 거부됐다. 추가 사유는 제공되지 않았으며 다른 명령으로 재시도하지 않았다. 따라서 이번 최종 빌드는 자율 연결이나 실제 USB비행 검증 결과를 포함하지 않는다.
- 모터 고장·실제 파괴·하단 띠 탈락 득점 판정·공격수 승계는 여전히 후속 구현이다. 프로젝트 전체 계획의 완료나 GitHub 최종 반영으로 보고하지 않는다.


## v0.6.0 다른 PC용 포터블 배포·기체 모델 — 2026-10-26

Shipping build/cook/stage/package/archive 성공. 최종 배포 입력SHA-256은 `6ad24fa79cf3e75c6ae2235c51ce0283fc286e5065ad81d76af89f2ab1371a3c`(78개)다. 빌드 당시 부모커밋은85e5f99이며 최신 기체 변경이 적용된 작업 트리에서 컴파일했다. 배포 소스의 입력 지문으로 실행 파일과 내용을 대응시킨다.

- 최종 빌드: `Saved/portable-model-build.log`, ExitCode0.
- 배포 ZIP 실제 압축 해제 후 한글·공백 경로와 분리된 새 사용자 데이터에서15개 검사 전부 통과: `Saved/Verification/portable-final-extracted/result.json`.
- 자식 프로세스의 PATH에서 개발용 Python·Git·Unreal 경로와 관련 환경변수를 제거했다. 네이티브 실행기의 상대 경로, 독립 APF의 실제10대 protocol2 HTTP 응답·원본 소스해시·유한 수명 종료를 확인했다.
- 빈 사용자 데이터에서 시작은키보드/USB비활성, 임의 보정 생성 없음, 진단 파일 저장을 확인했다. 별도 QA 데이터에만 기존 실물Boxer 프로필을 복사하여 실제 장치와의 일치·최저 스로틀 활성화·실제 값 저장/재로딩·검사 종료 시 비활성을 확인했다. 배포 ZIP에 해당 프로필은 없다.
- 새42셀 기체의80접합점·720봉 조각·29체결부·48모터 슬롯·배선/클램프,역할/방향LED 수,무충돌 메시와0.9kg 질량을 실제10대에서 확인했다: `Saved/Verification/role-layout-20261026-133951-145/result.json`.
- 기하학4개 검사 통과: 닫힌 구면 위상,오각12/육각30,외경40cm,최대 이상 구면 개구부131.856cm²,중심 대칭.
- 최종 모델의30/60/120FPS 물리 시나리오 통과: `Saved/Verification/runtime-20261026-133958-374/results.json`. 호버 오차0.522/0.341/0.273cm,바닥/벽·네트 접촉과 실제 득점 확인.
- 최종 모델의20/30/60/120/240FPS ANGLE10대 자세 검사 통과: `Saved/Verification/angle-20261026-134007-196/results.json`. 명시적 비USB 입력/고도 보상 fixture이며 실물 비행 성적이 아니다.
- Shipping 엔진은 개발용 `-FPS` 지정 처리가 제외돼 초기 반복 검사에서 계산 간격이 같았다. 해당 초기 결과를 서로 다른 FPS 검증으로 사용하지 않는다. 명시적 smoke 시나리오에서만 간격을 설정하고 실제 Tick 최소/최대값이요청값과 일치하는지 검사한 위 결과로 대체했다.
- 화질3단계 풀 검사: `Saved/Verification/presentation-20261026-133307-772`. 모터·조종기 코드와 무관한 모델 변경 전 동일 표시효과의 수명/상한 검사다.
- 최종 모델 실제1600×1000 GPU 렌더를 시각 검토: `docs/images/airframe-v06.png`. 첫 사용 조종기 모달도 포터블 Shipping 렌더에서 확인했다(`Saved/Verification/portable-first-use.png`,모델 변경 전 동일 설정UI).
- 실제 APF HTTP10개,번들 원본 검증5개,비행기록 판정5개 수학 검사 통과. 번들 손상·잘못된출처/경로탈출·누락 라이선스를 거부한다.

최종 실행 ZIP `ImperialWings-Windows-v0.6.0-r2.zip`:372,189,778bytes, SHA-256 `0b79c05dda4b7b85ce776a11b383ec917a9df4b39b584e229b8aa2dfbbd71296`.

실제 다른 물리PC·새HID드라이버·미설치VC상태에서의 설치 대화상자·모든GPU에서 실행한 결과는 아니다. 해당 조합과 완전한 USB이륙/착륙·케이블 분리·입력 지연은 추가 현장 검증 대상이다. 재배포 ZIP은 VC x64 설치 파일을 포함하며 Python/Git/Unreal 설치를 요구하지 않는다. 사용자별 보정·화질·진단/비행 기록은 `%LOCALAPPDATA%/ImperialWingsSimulator/Saved`에 저장한다.

앞선 R7(v0.5.0,소스7a20a652)실물 USB기록에서는 이륙 연속7.145초·호버2.336초를 관측했지만 포커스 상실과 이후 AUTO전환으로 착륙/DISARM 연속 검증은 실패했다. 이 기록을 v0.6의 USB비행 완료로 재사용하지 않는다. 파일은 로컬 `Saved/FlightLogs`에만 보관한다.

## 2026-10-26 실행 README·장착 조사·멀티플레이/피드백 계획

문서 변경만 수행했다. README를 실행 ZIP과 소스 빌드 경로로 나누고 첫 비행·Boxer·화질·오류 해결·저장 위치를 상세화했다. 실행 스크립트의 `-Package` 필요 조건과 통합 기본 포트8767을 소스와 대조했다.

- README·장착 조사·개발 계획의 상대 링크/명시 앵커50개 확인, 누락0개.
- README의 PowerShell 예시8개를 PowerShell 구문 분석기로 검사, 구문 오류0개. 설치·빌드·브리지 시작 명령을 실제 실행한 시험은 아니다.
- 예시의 프로젝트 스크립트 호출5개를 실제 스크립트의 매개변수와 대조, 없는 매개변수0개.
- `Scripts/Launch.ps1 -Inspect -ManualOnly` 조회에서 실행 파일 존재·현재 빌드 입력 일치 모두True. SHA-256은 기존 최종 패키지와 같은 `d5f05e26ebc0df6bdfdff5d4206802bfa376264ba9c3f0b4253203179ba31e10`.
- GitHub v0.5.0이 공개 상태이며 Windows ZIP·해시·출처 파일의 첨부를 조회 확인했다.
- FIDA Class40 태그 조항과 FAI2026 인쇄20쪽 B.1.6·공식 대회 기사 설명을 대조했다. FAI의 하단 리본과 FIDA의 색상/득점 자격을 합쳐 단일 규정으로 주장하지 않는다.
- 띠의 실제 고정 부품·치수·좌우 개수·부분 탈락 기준은 확보한 근거로 확정하지 않았다.
- 멀티플레이·추가 연속 데이터 수집·진단 내보내기는 계획으로 등록했다. 이 변경에 새 기능 구현·새 Windows 빌드·비행/UI/USB 재시험 결과는 없다.

## 2026-10-26 전체 기능 명세 추가

[전체 기능 계획](simulator-feature-plan.md)을 추가하고 기존 개발 계획·README·체크리스트에 연결했다. 각 기능에 고유 번호·우선순위·현황·완료 조건을 붙이고 M0~M5 순서와 공통 검수 양식을 등록했다. 현재 소스의 실제 USB 보정·GuidanceRequestWindow 번호 처리·배포 기록에 맞춰 이전 계획의 상태 설명을 정정했다.

- 기능 행292개·분야28개. 번호 중복·분야별 순번 누락·잘못된 표 행0개.
- 우선순위 합계: P0 36개, P1 163개, P2 87개, P3 6개.
- 현황 표시는 기본19개·보강75개·신규192개·선택6개이며 구현률·시험 통과율을 뜻하지 않는다.
- README·개발 순서·전체 기능 명세의 상대 링크/명시 앵커91개 확인, 누락0개.
- `git diff --check` 통과. `Launch.ps1 -Inspect -ManualOnly` 조회에서 기존 실행 파일 존재·입력 일치 모두True.
- 현재 입력SHA-256은 `d5f05e26ebc0df6bdfdff5d4206802bfa376264ba9c3f0b4253203179ba31e10`으로 기존 패키지와 같다.
- FIDA 공식 규정과 Epic UE5.8의 네트워크 물리·리플레이 공식 문서를 계획의 검토 근거로 참조했다. 엔진의 제공 기능을 프로젝트의 현재 구현으로 표시하지 않았다.
- 문서 변경만 수행했으며 새 코드·실행 파일·물리/UI/USB·온라인 시험 결과는 없다.

## 2026-10-26 공식 원문·정적 자체 피드백과 정밀 계획

[정밀 검토](simulator-plan-review.md)를 추가하고 전체 명세·개발 순서·README·태그 조사·규정 근거를 연결/보완했다. 조사 자료와 정적 코드 확인, 설계 위험, 향후 인수 시험을 구분했다.

- 공식 자료: FIDA 영문 PDF/웹, VelociDrone, ArduPilot, Betaflight, Epic 네트워크 물리/장애 모사, EdgeTX, FAI 안내를 확인했다.
- 초기 `a5f817a` 검토 뒤 최신 `f05aa66`을 별도 작업 사본에서 대조했다. HOLD/RETURN·종료 호출·제어Delta 제한·고정 공격수·활성 상태·세트 확정 경로는 여전히 정적 검수 항목이다. 새 실행 재현이나 결함 수정 결과로 표시하지 않았다.
- 자체 피드백16건·신규34개·기존16개 구체화·인수 계획20개. 전체326개·28개 분야.
- 우선순위 P0 41개·P1 180개·P2 98개·P3 7개. 번호 중복·분야 순번 누락·참조 기능 누락0개.
- 최신 v0.6 기반 F6·F8·진단·사용자별 저장을 보존하고 DAT-03/13·REL-08을 보강으로 조정했다. 상태 기본19개·보강78개·신규222개·선택7개이며 구현/검증 완료율이 아니다.
- 최초 문서 세트의 상대 링크/명시 앵커104개를 확인했고, 최신 README·포터블 안내와 병합한 뒤 최종 문서89개 링크를 다시 검사했다. 문서 구성 변경으로 개수가 달라졌으며 누락0개다.
- 검수 스크립트의 신규 기능 집계에서 SC 시나리오를 함께 센 부분과 기존 문서의 정당한 조항 기호 검사를 보정한 뒤 문서 검사 통과.
- `git diff --check` 통과. 최초 E작업 사본(v0.5 입력)에서 읽기 전용 `Launch.ps1 -Inspect -ManualOnly`의 기존 패키지 일치를 확인했으며, 이를 v0.6 패키지 검사로 재사용하지 않는다.
- 최종 문서는 별도 작업 사본의 `f05aa66` 위에서 병합했다. 해당 소스 입력 지문은 `6ad24fa79cf3e75c6ae2235c51ce0283fc286e5065ad81d76af89f2ab1371a3c`, 버전0.6.0, 소스 입력 변경 없음이다. 이 작업에서 소스 코드·설정·콘텐츠·실행 패키지를 변경하지 않았다.
- 모든 수치 기준은 검토용 목표, SC-01~20은 향후 인수 계획이다. 새 빌드·실물 비행·성능 측정·온라인 시험을 수행하지 않았다.

## 2026-10-27 v0.7 실제 기자재·접촉·시점·자율 검증

[배포·시험 결과](releases/v0.7.0.md), [수치 요약](verification/v0.7-summary.json), [실패에서 찾은 원인과 수정](autonomy-feedback-v07.md)을 추가했다. 이번에는 실제 코드를 변경하고 Editor/Shipping 빌드·실행·GPU 렌더를 수행했다. 최종 입력 지문은 `5634f0bd80e6dd09013841e3537d484eb90130160380679bebc4770192e8da4e`(108개)다.

- 최종 Editor/Shipping 빌드: `Saved/v07-r3-shipping-build.log`, ExitCode0.
- Shipping30/60/120FPS 가드·골대·6개 보기: `Saved/Verification/contact-view-20261026-182547-251`.
- 최종10대 모델·시작점/패드/LOS·LED/띠: `Saved/Verification/role-layout-20261026-182558-030`.
- Shipping20/30/60/120/240FPS10대 수동ANGLE: `Saved/Verification/angle-20261026-182546-313`.
- Shipping30/60/120FPS 호버·바닥/벽/네트·득점: `Saved/Verification/runtime-20261026-182559-782`.
- Shipping10대 실제APF/Chaos·귀환 중503·재연결 HOLD·착륙/DISARM: `Saved/Verification/autonomy-20261026-182546-413023`, 통과. 최소 간격1.098m,55.304초 시점 전체 착륙/DISARM.
- Shipping 효과 풀3단계: `Saved/Verification/presentation-20261026-182601-812`, 상한·수명 종료 확인.
- 실제 GPU 자율 렌더: `Saved/Verification/render-autonomy-20261026-182546-413023`. High1600×1000,1721표본,평균114.66FPS,p95프레임9.682ms. 짧은 개발 PC 측정이다.
- 실제 ZIP 한글/공백 경로 새 압축 해제·개발 도구 PATH 제거12개: `Saved/Verification/portable-v07-r3-extracted`, 전부 통과. 실물Boxer가 분리되어 기존15개 중 실제 프로필/장치3개 검사를 이번 버전의 재검증으로 계산하지 않았다.
- 규칙57·물리104·USB 보정 수학161·요청 순서16·실제 APF HTTP11·보호구 기하4·비행 기록 판정5개 통과. 제조사 실측 계수를 얻은 시험이 아니다.

중간 시험에서는 실제 용접 골대 질량93.26kg,초기 낙하 마모,심판 기둥 가림,낮은 기체의 APF 경로,귀환 잔여 오차/과도하게 높은 DISARM을 발견했다. 해당 기록을 성공으로 재분류하지 않고 수정 후 위 최종 결과로 확인했다. 실제 조종기·다른 물리PC·정밀 연성체/파단·전체 심판·온라인 기능과 전체326개 기능은 미완료 범위로 유지한다.

## 2026-10-27 v0.7.1 입력 복구·회귀 검증

[변경과 범위](releases/v0.7.1.md), [공개 수치 요약](verification/v0.7.1-summary.json). 입력 지문 `fc4a4ad9f3e63acc99e9170cb316c2e1888a31df1812c34090a4f3cf7d962921`(110개).

- 기준 버전은 시험 액터만 추가한 상태에서22개 중7개 실패: `Saved/Verification/input-safety-20261026-185213-561`.
- 최초 수정22개 통과 뒤 확장 검사에서 Reset 프레임의 잔류 스로틀0.002917을 발견했고 프레임 단위 억제로 수정했다. 이 중간 실패를 성공으로 재분류하지 않는다.
- 최종 Editor29개: `Saved/Verification/input-safety-20261026-190853-282`.
- 최종 Shipping29개: `Saved/Verification/input-safety-20261026-191424-223`. 빌드 `Saved/v071-final-shipping-build.log`, ExitCode0.
- 실제 ZIP을 한글/공백 경로에 새로 풀어29개: `Saved/Verification/input-safety-20261026-192034-432`, 전부 통과.
- Shipping ANGLE5개 계산 간격: `Saved/Verification/angle-20261026-191613-146`; 물리3개 간격: `Saved/Verification/runtime-20261026-191623-195`, 호버 오차3.297/3.150/2.988cm와 접촉·득점 통과.
- 가드/골대3개 간격·6개 보기: `Saved/Verification/contact-view-20261026-191614-061`;10대 모델/배치: `Saved/Verification/role-layout-20261026-191621-667`, 통과.
- 실제 APF/Chaos10대 출발·HOLD·통신 장애/복구·귀환·착륙·정규 경기 API 진입 제한: `Saved/Verification/autonomy-20261026-191613-251246`, 통과. 최소 간격1.089m,HOLD 최대 이동28.23cm.
- 포터블 새 설치12개: `Saved/Verification/portable-v071-extracted`, 통과. 보존한 실제 보정 원본과 사용자 보정 파일의 바이트 일치도 확인했다. 원시 값과 개인 파일은 공개하지 않았다.

엔진 simulated keyboard event·Viewport API 시험이다. 실제 OS Alt-Tab·물리 키보드·Boxer/HID·입력 지연을 측정한 시험이 아니다. CTL-13과 실제 현장 시험을 미완료로 유지하며, 그래픽·독립 수학 검사 수치는 이전 버전의 기록과 구별한다.

## 2026-10-27 v0.7.2 시간·자율 착륙·다운로드 안내

[실패·변경·재현·범위](releases/v0.7.2.md), [공개 수치 요약](verification/v0.7.2-summary.json), [시간 계약](time-contract.md).
최종 입력 지문 `39189d9c53e9f4337124a84a7a15c56289c70e1aaf538bda130db11318e571d7`(114개).

- 실제100/250ms 정지의 기존 실패: 모델33.333ms 누락·센서220.967ms 초과. 동기 장면 시작의 실제 예산으로 통일했다.
- 최종 Shipping 정상/지연 연습/지연 공식 경기 각60초·10대, 모델/센서와 완료된 Chaos 누적 시간 차이 약1.1e-12초 이하. 배터리 전류 적분·생략/보류·경기 시간 계약 통과. 경기 시계는 게임 시간이며 물리 생략을 숨기지 않는다.
- 실제 APF 자율 첫 시도는2대가33~35cm에서 멈춰125초 착륙 실패.10cm 목표 데드밴드/지면 회피 원인을 재현·종단 목표 보완 후13개 유도 검사와 실제10대54.32초 착륙/DISARM 통과. 최소 간격1.071m·HOLD29.53cm·통신503/복구 뒤 명시 재개·공식 경기 차단 통과.
- 동일 지문의20~240FPS10대 ANGLE,30/60/120FPS 호버/접촉/네트/골대,6개 시점·10대 배치·Shipping29개 입력 검사 통과. 새 ZIP의 개발 도구 제거/독립 APF/사용자 경로12개와 입력29개 통과.
- 실제 Shipping1280×720 GPU 캡처·F6 start/sample/stop·시간 장부를 확인했다. 기존 개인 조종기 보정은 원본과 바이트 동일하며 공개 파일에 포함하지 않는다.
- README 상세 다운로드·실행·일반 조종기 설정·현재 작업 목록을 갱신했다. 독립 고정 스레드 PID·USB 현장 시험·다른 PC·실측 물리와 전체 계획 완료는 별도다.

## 2026-10-27 v0.7.3 조종기 Mode 1·Mode 2

[변경·검사 범위](releases/v0.7.3.md), [공개 결과](verification/v0.7.3-summary.json).
입력 지문 `a307273e71778f07e102b83fd4269e59129a49b3012937e5f46a9085855cdcb4`(118개).
Editor/Shipping 빌드와 보정205개·실행 UI/JSON20개·입력 안전29개를 통과했다.
새 ZIP을 한글·공백 경로에 풀어 동일 UI/JSON20개를 다시 확인했다.
두 모드 선택 화면의 실제 GPU 렌더를 기록했다. 모드 변경은 조종기 본체의 설정 변경이 아니며 실제 축을 다시 측정한다.
실물 조종기는 분리돼 있어 물리 입력·Mode 1 비행·실물 저장 재시작 검증과 구별한다.

## 2026-10-27 v0.7.4 전력·정밀 스로틀·득점 경계

[변경·실패 재현·검사 범위](releases/v0.7.4.md), [공개 결과](verification/v0.7.4-summary.json), [점수101개 상세](scoring-test-2026-10-27.md).

최종 Shipping 입력 지문은 `33e6f4eaed5089dc6a8e71a2c4e0b3cefa7c1d26e6d105616b67c6e9c6b2fe3b`(123개)다. 모터 전력이 공급을 초과하던4개 조건을 재현·수정했고 실제20/60/240FPS의6개 전력 조건이 통과했다. 같은 간격의 정밀 스로틀 첫 실행6개·재실행36개 및 일반F6의6구간 기록이 통과했다. 60FPS에서54→56% 입력의 실제 추력 변화 비율은0.6501875였다. 최대출력은 유지하며55%를 실기체 호버점으로 주장하지 않는다.

규칙 101개, 30/60/120FPS 실제 1득점·오프사이드, 20~240FPS 10대 ANGLE, UI 20개·입력 29개, 공식 경기 지연 60초 검사를 통과했다. 실제 APF 10대는 54.24초에 모두 착륙·DISARM했다. 최소 간격 1.11837m·HOLD 최대 이동 28.2821cm, 통신 장애 시 HOLD·복구 뒤 자동 재개 금지를 확인했다. 새 한글·공백 경로에서 ZIP 13개·정밀 설정 재실행 42개·전력 6개 검사도 통과했다.

실제 USB 비행·다른 물리 PC·독립 고정 120Hz PID·전체 심판 운영은 미검증/미완료다. 이번 공식 시간 검사의 JSON 공통 scope에 HOLD라는 옛 설명이 남지만, official_manual_match와 match_stayed_manual_disarmed=true인 수동·비무장 시계 검사다. 원본 결과를 바꾸지 않고 이 차이를 명시한다.

<a id="ctl-13-r9-probe-pending"></a>
<a id="ctl-13-r9-probe"></a>
## 2026-10-27 v0.7.5-dev / R9 CTL-13 읽기 전용 계측 — 하위 단계 검사 완료

이 항목은 `codex/r9-solver-probe`의 격리된 개발 실험 결과다. 공식 실행용 v0.7.4-r2의 검증 기록·다운로드·해시는 그대로 유지한다. 이번 실험으로 새 일반 사용자 릴리스를 배포할 계획은 없다. [실험 보고서](solver-probe-2026-10-27.md) · [기계 판독 결과](verification/solver-probe-2026-10-27.json).

추가 소스는 [ImperialSolverProbe](../Source/ImperialWingsSimulator/ImperialSolverProbe.cpp)와 [검사 진입점](../Tests/run-solver-probe.py)이다. 명시적으로 켠 진단에서 `OnPreSimulate`·`OnPreIntegrate`·`OnPostIntegrate`의 횟수·순서, solver advance의 `dt`, 시간 합·스레드 식별자를 관찰한다. callback은 기체·컴포넌트·강체 proxy를 읽지 않으며 PID·센서·모터 계산이나 힘·토크 적용을 추가하지 않는다. fixed120은 DISARM 상태에서 선택하는 solver 카운터 실험이며 **고정120Hz 비행 제어 또는 별도 적분 `dt`의 직접 측정 결과가 아니다.**

최종 Editor와 새 경로 Shipping의 빌드 입력은125개, 지문은 `1a6b144d1404dfc32b5336125d5236f9ad3f97f0ab3019b9378943cd90cf955f`로 같다.

| 확인 단계 | 최종 결과 |
|---|---|
|20/30/45/60/120/240FPS·동기/fixed120 각8초 | Editor12개+Shipping12개, 합계24개 통과. 각 실행본의 동기 단계 수는 순서대로1120/1200/1080/1440/1920/1920, fixed120은 모두960 |
|43초 시나리오 baseline/observer | 각 실행본의 비교1쌍, 합계2쌍 통과. observer는 예열 후 월드0.5166667~43.0166689초·완료 tick까지7650단계를 관찰 |
| 경과 시간 장부 | 관찰 시간 약42.5000022초, solver 간격 약0.005555556초, 누적 장부 오차 약1.75e-12초. 게임 시간43초 전체와 예열 후 관찰 시간을 구분 |
| 잘못된 옵션 거부 | 실행본별4종, 합계8개 통과. 새 JSON의 실패 상태·구체 오류·빌드 지문 확인 |
| 새 경로 포터블 |13개 통과. 파일 해시·런처·독립 APF·새 사용자 저장/보정 안전 확인 |
| observer 전후 비교 | 호버·네트·골대 수치 및10대 종료 위치/속력/SOC/전압 차이는 직렬화 정밀도에서0. 연속 궤적·속도 벡터·전체 에너지 적분 검증은 아님 |

실제 호버 오차3.15cm, 네트 모델 최대 처짐0.06753m·표시 와이어0.0646m·반동1.0562m/s, 스트라이커의 실제 통과와 BLUE1골을 확인했다. 종료 상태에서 비교한 속력은 크기이며 속도 벡터의 동등성으로 확대 해석하지 않는다.

거부 사례도 UE Windows의 정상 `WM_QUIT` 경로에서 프로세스 종료 코드0이 나올 수 있다. 강제 종료를 사용하지 않았고, runner는 종료 코드만으로 통과시키지 않고 새 JSON의 `ok=false`·오류 사유·동일 지문을 필수로 검사했다. Python 최적화 실행 거부와 CSV의 모든 수치 유한값·8개 거부 문구 사후 검사도 통과했다.

로컬 원본은 Editor `Saved/Verification/solver-probe-20261027-082339-906316/results.json`, Shipping `Saved/Verification/solver-probe-20261027-082339-926636/results.json`, 포터블 `Saved/Verification/portable-probe-r1/result.json`이다. 이번 완료는 **계측 하위 단계**다. 이 카운터 단계는 연속 궤적·전체 에너지 적분을 검사하지 않았으며, 아래 후속 기록에서 프레임별 검증 범위를 구분한다. 실제 고정주기 비행 제어의 상태 전달·힘 이관, 실물 USB·새 하드웨어·다른 물리 PC·GPU/그래픽 검증은 미완료다. [순차 작업](sequential-work-2026-10-27.md#r9-ctl-13-probe)과 [단계 설계](fixed-step-control-design.md)에서 후속 조건을 관리한다.

<a id="ctl-13-r9-flight-trace"></a>
## 2026-10-27 v0.7.5-dev / R9 프레임별 궤적·전류·에너지 — 게임 프레임 비교 완료

카운터 단계의 종료 상태 비교 한계를 보완하는 후속 실험이다. 최종 입력127개·소스 지문 `71aa36b021080fcbc262ed1820ba6d169e7d3fc36651e3978beb690a2eb8515e`, 빌드 부모 커밋 `0e0dd21b7b261cd9690fe18e9b2882e65d9fd299`다. 같은 개발 브랜치와 v0.7.5-dev를 사용하며, 공식 다운로드 v0.7.4-r2와 그 검사 기록은 유지한다. [후속 보고서](flight-trace-2026-10-27.md) · [기계 판독 결과](verification/flight-trace-2026-10-27.json).

첫 샘플은 기준값으로 두고 최초 구간을 적분에서 제외한다. 이후43게임초 시나리오 완료 프레임까지 **매 동기 게임 물리 프레임의 끝점**에서10대 위치·quaternion·선속도/각속도 벡터·전류·SOC·전압·소모Ah·공급/모터 전력을 비교한다. `physics_step_seq`는 매 프레임+1이어야 하며,0번 기체의 reset epoch3회·나머지9대의 무reset과 최종 순번을 확인한다. PID·힘·토크의 계산 및 적용 경로는 이관하지 않았다.

| 확인 단계 | 최종 확인 결과 |
|---|---|
| Shipping 동일FPS baseline/observer |20/30/45/60/120/240FPS6쌍 통과. 실행당 프레임 수860/1290/1935/2580/5160/10320, 기체 샘플 수8600/12900/19350/25800/51600/103200 |
| Editor 동일FPS 비교 |20/30/45/60/120/240FPS6쌍 통과 |
| 새 경로 ZIP 비교 |60FPS1쌍 통과. 세 실행 환경 합계13쌍·26회, 기체 행937,400개·첫 기준행260개를 제외한 적분 구간937,140개 |
| 독립 분석기 검사 | Editor·Shipping·새 ZIP 모두18개 통과.17개 손상 사례 거부·동일 회전의 quaternion 부호 반전1개 허용. 행 누락/중복/순서·완료 프레임 누락·NaN/Inf·reset/물리 갱신 누락·중간 오류 상쇄·속도 방향 변경·양자화 한도를 넘은 모터 공급 초과 포함 |
| 전류·에너지 장부 | 매 프레임 `I*dt/3600`를 reset 구간을 반영한 소모Ah/SOC와 대조. `V*I`·공급 에너지 및 모터 전력 적분을 독립 재계산하고 baseline/observer의 프레임별 값과 전체 적분 비교 |
| baseline/observer 차이 |13쌍 모두17자리 CSV의 SHA-256이 쌍별 일치. 위치·선속도/각속도 벡터·전류·SOC·전압·소모Ah·프레임 전력·전체 전류/에너지 적분 차이는0. 동일 quaternion의 각도 재계산 잔차는 최대2.98e-8rad로1e-4rad 기준 이내 |
| 최신 소스 회귀·새 경로 ZIP | 동일127입력의 Shipping 카운터12개·옵션 거부4개와 새 포터블13개 통과. 이전125입력의 결과를 재사용하지 않음 |
| 진단 옵션·기존 CSV 보호 |6개 통과: fixture 누락·fixed 모드·passthrough 누락·CSV 경로 누락·잘못된FPS·이미 존재하는 CSV를 거부. 기존 CSV의 검사용 원본 바이트 보존 확인 |

60FPS 추적 창은 월드0.033333335~43.016668910초, 약42.983335575초이며 첫 샘플 앞의 구간은 적분하지 않는다. Shipping60FPS0번 기체는3회 reset을 반영해 전류 적분0.1526968602575126Ah·공급 에너지8855.907633820878J·모터 에너지8640.440938004913J를 기록했다. 최대 프레임별 전하 오차는 약6.94e-18Ah, SOC 오차는 약5.55e-17이다.60FPS 호버 오차3.15cm와 실제 BLUE1골도 유지했다.

Editor120FPS에서 같은 CSV quaternion을 정규화 내적과 `acos`로 각도로 바꿀 때2.980232238769531e-8rad가 계산됐다. 원본 CSV 해시는 같으며, 이 각도 계산 잔차까지 모두0이라고 표현하지 않는다.

float로 저장한 실제 모터 추력에서 전력을 추정하면 double 공급 예산과 약18.5μW의 차이가 생겼다. 분석기의 허용은 `3*2^-24*P + 1e-8 W`이며 `P`는 적용/가용 모터 전력 중 큰 값이다. 추력의 수치 양자화를 위한 해석적 오차 한도이며 물리 전력 예산 증가가 아니다.

추가 거부 검사에서 UE5.8의 `CreateFileWriter`가 `FILEWRITE_NoReplaceExisting`를 무시해 기존 CSV를 덮어쓰는 문제를 재현했다. 진단 시작 전에 `FileExists`/`DirectoryExists`로 사전 존재 경로를 명시적으로 거부하도록 보완했다. 검사용 기존 CSV가 바뀌지 않았음을 확인하고, 수정한 동일 입력 지문으로 전체 비행 비교·분석기·카운터·새 ZIP 검사를 다시 통과했다.6개 거부 사례의 프로세스 종료 코드는0이지만 새 결과 JSON의 구체 오류·실패 상태·지문을 확인하며, 기존 CSV 사례는 원본 보존도 검사한다.

최종 로컬 원본은 Editor `Saved/Verification/flight-trace-20261027-090216-011829/results.json`, Shipping `Saved/Verification/flight-trace-20261027-090245-610934/results.json`, 새 ZIP `Saved/Verification/flight-trace-20261027-090443-893557/results.json`이다. 옵션/기록 보호6개는 `Saved/Verification/flight-trace-options-20261027-090205-810108/results.json`, 포터블은 `Saved/Verification/portable-trace-r2/result.json`, 최신 카운터 회귀는 `Saved/Verification/solver-probe-20261027-090244-449724/results.json`에 있다. 새 로컬 QA r2 ZIP은374,810,923바이트·SHA-256 `33ec007b00d6e1075690ad2cb1c8096330af1aeda207dbc37de27643870df67c`이며 공식 실행용 다운로드를 교체하지 않는다.

완료된 범위는 **CTL-13 단계1의 현재 게임 물리 프레임 해상도에서의 궤적·전류·에너지 baseline 비교**다. 내부 Chaos 서브스텝의 연속 궤적, 서로 다른 FPS끼리의 동일 궤적, 실제 고정120Hz 비행 제어·상태 버퍼/힘 이관·GPU 성능·실물 USB·새 하드웨어·다른 물리 PC 검증은 주장하지 않는다. [남은 개발](sequential-work-2026-10-27.md#r9-flight-trace).

<a id="ctl-13-r9-solver-bridge"></a>
## 2026-10-27 v0.7.5-dev / R9 입력 전달·물리 단계별 힘 — 기반 검증 완료

별도 구형 강체의 입력/출력 전달과 내부 핸들 힘 적용을 구현·검증했다. 최종 소스 입력129개·지문 `4ed812ec3ebad3bee96ec4c4235dbf060ac1e53ff54a4038d1dd5a070b7bb838`, 빌드 부모 커밋 `a1092dc8b33fde9d372566fef36d9f0d63ae5661`이며 빌드 시 작업 사본은 dirty였다. 이후 게시 커밋이 실행 파일에 포함돼 있다고 주장하지 않고 실제 검사한 소스 지문으로 대조한다. [상세 보고서](solver-bridge-2026-10-27.md) · [기계 판독 결과](verification/solver-bridge-2026-10-27.json).

질량2kg·반지름0.25m, 중력·감쇠·접촉이 없는 구형 강체에서 매 solver 단계의 내부 속도·각속도와 힘/토크 적용 횟수를 기록했다. 독립 분석기는 미리 정한 입력 구간을 정답으로 삼아 `Δv = F/m × dt`, 구형 관성 `I=0.05kg·m²`의 `Δω = τ/I × dt`를 단계별·누적으로 대조한다. 힘(2,-1,0.5)N과 질량중심에서 월드축(0,0.05,0)m 떨어진 작용점을 사용하며 기체 자체의 중심은 원점과 같다.

| 검사 | 최종 결과 |
|---|---|
| Editor·Shipping20/30/45/60/120/240FPS |각6회 통과. 실행당 단계 수630/675/609/810/1080/1080 |
| 새 압축 해제본20/45/240FPS |3회 통과. 전체15회·12,087개 실제 물리 단계 |
|9개 명령 구간 |DISARM·ARM·DISARM·오래된 입력·잘못된 세대·비유한 입력·재전송·새 ARM·DISARM. 재전송은 같은 순번/새 내용과 더 오래된 순번1을 모두 거부 |
| 분석기와 옵션 거부 |각 실행 환경에서 손상 결과19개·옵션4개 통과. 세 환경 옵션 거부 합계12개 |
| 실제 해석값 오차 |단계별 속도 최대3.258e-8m/s·각속도9.344e-8rad/s, 누적 속도2.213e-6m/s·각속도3.918e-6rad/s |
| 종료·잔류 힘 |강체를 유지한 callback 등록 해제·소멸 확인. 종료 뒤 callback 호출0회·관성 운동 속도 변화0 |
| 같은 소스의 일반 비행 회귀 |Shipping60FPS43초 baseline/observer1쌍, 각각10대 기록25,800행·분석기18개 통과 |
| 새 포터블 |13개 통과. 로컬 QA용 ZIP374,825,451바이트·SHA-256 `d1afd549e8ffc622d07692e1dfa03f263905588049e2d9a2d6516318bf4235f9` |

구간은 약0.5게임초씩이며 전체4.5초(45FPS에서는 프레임 경계로 약4.51111초)다. 패키징 초기 객체에서 질량을 설정하던 문제를 실제 `BeginPlay`로 옮겼고, 같은 순번의 새 내용을 허용하던 경로를 차단했다. 병렬 검사의 결과 폴더 충돌은 기존 자료를 덮어쓰지 않고 실패했으며 고유 식별자를 추가한 실행기로 보완했다. 예비 성공 결과를 최종 검사 대신 재사용하지 않는다.

최종 원본은 Editor `Saved/Verification/solver-bridge-20261027-092934-999092-0c8e98ce/results.json`, Shipping `Saved/Verification/solver-bridge-20261027-092910-580395/results.json`, 새 경로 `Saved/Verification/solver-bridge-20261027-093111-509764-12893a4f/results.json`이다. 일반 비행 회귀는 `Saved/Verification/flight-trace-20261027-093345-744322/results.json`, 포터블은 `Saved/Verification/portable-bridge-r1/result.json`에 있다.

완료 범위는 **별도 단순 강체의 입력·힘 연결 기반**이다. 일반 드론의 센서·PID·모터/배터리 이관, 중심이 치우친 기체·초기화/파괴/재바인딩·비동기 수명·정확한120Hz 비행은 후속이다. 입력 만료는 시뮬레이션 시각 기준이며 실물 USB 지연이나 실제 벽시계 정지 감시를 뜻하지 않는다. GPU·다른 물리PC 시험으로 확대하지 않는다. 공식 v0.7.4-r2 다운로드는 유지한다. [다음 작업](sequential-work-2026-10-27.md#r9-solver-bridge).
