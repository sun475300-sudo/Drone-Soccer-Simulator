# Imperial Wings Drone Soccer Simulator

**[전체 개발 계획서](docs/master-development-plan.md)** — 피드백12개, 현재 완료/미완료 상태, 조종기·AI·탑건·LOS·언어/UI·경기·물리·LAN/온라인·배포의 순서와 완료 기준을 통합했습니다. [28개 분야·326개 상세 기능](docs/simulator-feature-plan.md) · [작업 체크리스트](IMPERIAL_WINGS_CODEX_TASKS.md).

<a id="current-development"></a>
## 현재 개발: 조종기 레이트·Expo·아밍키 — v0.7.6-dev

사용자 요청에 따라 이주한 피드백12개 중 **조종기 설정부터** 진행합니다. 기존 Mode1/2 단계형 보정을 유지하며 축별 레이트·Expo 편집/곡선 미리 보기, 조종기 아밍 버튼 학습·토글/2단 스위치 선택·저장을 추가했습니다. 기본은 **LOS·ANGLE35도**이고 자동 고도 유지 기능은 아닙니다. [설정 방법·화면·검사 범위](docs/controller-response-2026-10-27.md) · [12개 요청과 진행 순서](docs/pilot-feedback-2026-10-27.md).

응답·버튼 조건4,881개, 기존 보정205개, Editor/Windows 실행본/새 압축 해제본 각각13종·66개 설정·제어 검사, 기존 입력·Mode1/2 검사, 실제10대 ANGLE20/60/240FPS와 새 ZIP13개 검사를 통과했습니다. 실제1600×1000 설정 화면도 렌더로 확인했습니다. **실물 USB 아밍키·다른 PC 확인은 미완료**입니다. [상세 검증과 실패 기록](docs/verification.md#r10-controller-response).

**개발 브랜치 `codex/pilot-feedback`의 기능이며 아래 공개 실행 ZIP v0.7.4-r2에는 아직 없습니다.** WinMM6축/32버튼 외의 범용 USB 확장, 실물 아밍 버튼/다른 PC 검사는 별도입니다. AI3~4대 난이도별 연습·대전과 탑건 S코스/닉네임·ID·국적/기록방은 후속 작업이며 기존 자율비행 기능을 이 요청의 완료로 표시하지 않습니다.

## 이전 개발: CTL-13 입력 전달·물리 단계별 힘 연결 검증 — v0.7.5-dev

격리된 `codex/r9-solver-probe` 브랜치에서 **별도 시험 강체의 입력 전달·힘 적용 연결**을 구현하고 검사했습니다. 질량2kg·반지름0.25m의 구체에 힘과 질량중심에서 벗어난 작용점의 토크를 물리 단계마다 한 번 적용하고, 실제 속도·각속도 변화를 독립 계산값과 대조했습니다. 실제 드론의 센서·PID·모터/배터리를 이 경로로 옮기는 작업은 다음 단계입니다.

**Editor·Shipping20/30/45/60/120/240FPS와 새 압축 해제본20/45/240FPS, 총15회·12,087개 물리 단계**를 통과했습니다. DISARM·오래된 입력·잘못된 세대·비유한 입력·같거나 지난 순번의 새 명령을 차단하고 새 ARM에서 힘이 재개되는9개 구간을 확인했습니다. 각 실행 환경의 분석기19개·옵션 거부4개, 새 ZIP13개와 같은 소스의 정상10대 비행60FPS 비교1쌍도 통과했습니다. [이번 결과·수정 내역](docs/solver-bridge-2026-10-27.md) · [검증 기록](docs/verification.md#ctl-13-r9-solver-bridge) · [다음 작업](docs/sequential-work-2026-10-27.md#r9-solver-bridge).

이전 단계1의 **13쌍·26회·기체 기록937,400행** 프레임별 비행·전류·에너지 비교는 당시 소스 지문의 결과로 보존합니다. [이전 프레임별 결과](docs/flight-trace-2026-10-27.md) · [카운터 결과](docs/solver-probe-2026-10-27.md).

이번 완료는 단순 강체의 연결 검증입니다. **일반 드론 PID 이관·정확한120Hz 비행·CTL-13 전체는 아직 미완료**이며, 입력 만료는 시뮬레이션 시각 기준입니다. 실물 USB 지연·기체 파괴/재생성·비동기 수명·GPU·다른PC 검증으로 확대하지 않습니다. **공식 실행용 다운로드는 아래 v0.7.4-r2 그대로**이며 새 일반 사용자 릴리스는 배포하지 않았습니다.

<a id="windows-run"></a>
## Windows 다운로드·실행 안내

**[다운로드 방법](#download-steps) · [실행 방법](#run-steps) · [첫 조종기 설정](#first-controller-setup)**

**v0.7.4-r2는 로컬 실행·새 경로의 배포 ZIP 검사를 통과했습니다.** [릴리스·검증 기록](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/tag/v0.7.4) · **[Windows x64 실행용 ZIP 다운로드](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/download/v0.7.4/ImperialWings-Windows-v0.7.4-r2.zip)**. 전력 제한·선택형 정밀 스로틀·득점 경계 오류 수정·피드백 양식을 포함합니다. ZIP은 374,786,305바이트이며 [해시와 검사 범위](docs/releases/v0.7.4.md)를 확인할 수 있습니다.

<a id="download-steps"></a>
이번 점수 검사는 **101개 규칙 조건**과 실제 기체의 골대 통과를 확인했습니다. 중앙선 접촉에서 오프사이드가 먼저 풀리는 오류를 수정했습니다. [득점 검사 상세](docs/scoring-test-2026-10-27.md) · [Liftoff·VelociDrone·Uncrashed·DRL 참고 비교](docs/simulator-reference-comparison.md).

GitHub에 공개한 실행 ZIP·소스 ZIP을 다시 다운로드해 검증한 원본과 SHA-256이 같음을 확인했습니다. [업로드·다운로드 확인 기록](docs/verification/v0.7.4-publication.json).

### 1. 다운로드 방법

1. [GitHub Releases 페이지](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases)를 엽니다. 개발 시험판에는 `Pre-release`가 표시되므로 `Latest` 표시만 찾지 말고 **버전과 변경·검증 기록**을 확인합니다.
2. 받을 버전의 **Assets**를 펼치고 **`ImperialWings-Windows-<버전>-<빌드>.zip`**을 누릅니다. 검증된 파일명은 **`ImperialWings-Windows-v0.7.4-r2.zip`**입니다. Assets의 파일명·용량이 맞는지 확인합니다.
3. 다운로드가 완료될 때까지 기다립니다. 브라우저의 다운로드 목록에서 **폴더에서 보기**를 누르거나, 파일 탐색기의 **다운로드** 폴더에서 ZIP을 찾습니다. 이어서 아래 [실행 방법](#run-steps)을 따릅니다.

| 다운로드 항목 | 용도 |
|---|---|
| **`ImperialWings-Windows-…zip`** | **일반 사용자용 실행 프로그램 — 이 파일을 받습니다.** |
| `Source code (zip)` / `Source code (tar.gz)` / `…-source.zip` | 개발자가 직접 빌드할 소스코드. 일반 실행용 프로그램이 들어 있지 않습니다. |
| 저장소 상단 **Code → Download ZIP** | 저장소 소스코드. 실행용 ZIP과 다릅니다. |

<a id="run-steps"></a>
### 2. 실행 방법

**실행 환경:** Windows 10/11 x64, DirectX12·Shader Model6를 지원하는 GPU가 필요합니다. Unreal Engine·Python·Git을 별도로 설치할 필요는 없습니다.

1. 받은 ZIP을 **마우스 오른쪽 → 모두 압축 풀기**로 전체 해제합니다. 예를 들어 v0.7.4는 `C:\Games\ImperialWings-v0.7.4`에 풉니다. ZIP 안에서 바로 실행하거나 EXE 하나만 꺼내지 않습니다.
2. 파일 탐색기에서 압축을 푼 폴더를 열고 **`ImperialWingsLauncher.exe`**를 더블클릭합니다. ZIP 이름의 하위 폴더가 한 번 더 있으면 그 안으로 들어갑니다. 브라우저 주소창에 파일 경로를 붙여넣는 방식이 아니라, 탐색기에서 실행 파일을 엽니다.
3. 실행기 옆에 **`Windows`·`Guidance`·`Prerequisites`** 폴더와 `START_HERE.ko.md`가 있는지 확인합니다. 폴더 구성을 유지해야 하며, 기본 실행은 바깥쪽의 `ImperialWingsLauncher.exe`를 사용합니다.
4. Visual C++ 실행 구성 요소가 없다는 메시지가 나오면 **`Prerequisites\vc_redist.x64.exe`**를 실행해 설치한 뒤 실행기를 다시 엽니다.
5. 게임의 조종기 설정 화면이 열리면, **키보드는 USE KEYBOARD → CLOSE**로 시작합니다. USB 조종기는 아래 [첫 조종기 설정](#first-controller-setup)을 진행합니다.
6. 처음에는 **START MATCH를 누르지 않고 Practice**에서 [첫 비행](#first-flight)을 합니다. 기본은 경기장 밖 **LOS** 시점입니다. **ANGLE은 수평 자세를 복원하며, 높이는 스로틀로 직접 조절합니다.**
7. 종료할 때는 착륙·DISARM 후 상단 **EXIT** 또는 창 닫기를 사용합니다. F6 비행 기록을 켰다면 F6을 다시 눌러 저장을 마친 뒤 종료합니다.

<a id="first-controller-setup"></a>
### 3. USB 조종기 처음 설정하기

**v0.7.3부터 Mode 1·Mode 2를 화면에서 선택할 수 있습니다.**

1. 조종기의 **데이터 전송용 USB 포트**에 데이터 케이블을 연결합니다. 기종에 따라 전원을 켜고 **USB Joystick / 게임 컨트롤러 모드**를 선택합니다.
2. 게임에서 **실제 장치 선택 → MODE 1 또는 MODE 2 선택 → STICK SETUP**를 누르고 화면 안내대로 스틱의 전체 범위와 중립을 보정합니다. **MODE 1은 오른쪽 위아래가 스로틀**, **MODE 2는 왼쪽 위아래가 스로틀**입니다. 화면이 선택한 모드에 맞게 피치·스로틀 손 위치를 안내합니다. 모드를 바꾸면 USB가 해제되며 새 보정을 저장하기 전까지 이전 모드의 축으로 활성화하지 않습니다.
3. **CONFIRM & SAVE**로 저장한 뒤 **스로틀 최저·나머지 축 중립**에서 **ENABLE RADIO**를 누릅니다. 패널을 닫고 Space로 ARM합니다.
4. 다음 실행부터는 같은 조종기를 연결하고 최저 스로틀에서 **F8 / CONNECT SAVED RADIO**로 저장된 설정을 불러옵니다. 다른 장치에 이전 보정 파일을 그대로 복사하지 않습니다.

| 스틱 배치 | 왼쪽 좌우 | 왼쪽 위아래 | 오른쪽 좌우 | 오른쪽 위아래 |
|---|---|---|---|---|
| Mode 1 | Yaw | Pitch | Roll | Throttle |
| Mode 2 | Yaw | Throttle | Roll | Pitch |

<a id="manual-throttle"></a>
### 선택 사항: 정밀 스로틀 — v0.7.4

새 사용자 기본값은 **THROTTLE: DIRECT**입니다. 작은 스틱 움직임으로 출력이 크게 변한다고 느끼면 모든 수동 기체를 착륙·DISARM한 뒤 조종기 설정의 **THROTTLE: DIRECT**를 눌러 **THROTTLE: PRECISION**으로 바꿉니다. 간소·고급 화면 모두에서 선택할 수 있으며, 보정 중이나 수동 기체가 ARM 상태일 때는 변경할 수 없습니다. 저장이 완료되면 USB가 해제되므로 최저 스로틀에서 **ENABLE RADIO / F8**로 명시적으로 다시 연결합니다. 키보드 수동 스로틀에도 같은 선택이 적용됩니다.

Precision은 **입력55% 부근의 변화량을 줄이는 곡선**입니다. 최저0%·최대100%·입력55%의 출력은 유지하고, 중심에서 입력 변화에 대한 출력 변화율은0.65입니다.55%는 모든 기체·배터리에서 실제로 호버하는 지점을 뜻하지 않습니다. **ANGLE은 자세를 수평으로 복원하며 고도는 계속 스로틀로 조절합니다.** Precision에도 자동 고도 유지가 없고 자율비행 스로틀에는 이 곡선을 적용하지 않습니다.

조종기 보정과 USB 활성화·ARM의 최저 스로틀 판정은 **정밀 곡선 적용 전 입력**을 사용합니다. 이 옵션은 모터 자체의 추력 곡선을 바꾸지 않습니다. 설정은 사용자 저장 폴더의 `Config/PilotControls.json`에 별도로 저장하며 `RadioProfile.json`의 실제 축·범위·중립을 변경하지 않습니다. 저장에 실패하면 이전 선택을 유지하고, 설정 파일이 잘못됐거나 지원하지 않는 버전이면 다음 시작에서 DIRECT를 사용합니다.

**다른 앱으로 창을 전환하면 수동 기체는 DISARM됩니다.** USB는 게임으로 돌아온 뒤 최저 스로틀에서 F8로 다시 연결하고 ARM합니다. 상세 안내는 [다른 PC에서 실행·조종기 설정](docs/PORTABLE_QUICK_START.ko.md)을 확인합니다.

**업데이트:** 새 버전 ZIP을 별도 폴더에 전체 압축 해제합니다. 이전 실행본에 일부 파일만 덮어쓰지 않습니다. 사용자 보정은 `%LOCALAPPDATA%\ImperialWingsSimulator\Saved\Config\RadioProfile.json`, 정밀 스로틀 선택은 같은 폴더의 `PilotControls.json`에 따로 저장합니다. 같은 Windows 사용자·같은 조종기의 설정은 새 버전에서도 불러올 수 있으며 USB 활성화·ARM은 자동 복원하지 않습니다. 이전 버전은 새 버전을 확인한 뒤 정리합니다.

**다운로드/실행 문제:**

- 탐색기에 `file:///E:/...exe` 또는 `%20`이 들어간 경로를 붙여 넣어 “찾을 수 없습니다”가 나오면, 브라우저 주소를 실행한 것입니다. 파일 탐색기로 **실제 압축 해제 폴더**를 연 뒤 `ImperialWingsLauncher.exe`를 더블클릭합니다. 내려받은 PC에는 개발 PC의 `E:\...` 경로가 없습니다.
- 실행 파일이 없으면 `Source code`를 받았는지, 아직 ZIP 내부를 보고 있는지 확인합니다. 이름에 `ImperialWings-Windows-`가 포함된 실행용 ZIP을 다시 확인합니다.
- 파일 손상/누락은 ZIP을 다시 내려받고 새 폴더에 전체 압축을 풀어 확인합니다. GitHub 릴리스의 `SHA256SUMS.txt`로 다운로드 무결성을 확인할 수 있습니다. 확인 예: PowerShell에서 `Get-FileHash -Algorithm SHA256 -LiteralPath 'C:\Users\본인\Downloads\ImperialWings-Windows-v0.7.4-r2.zip'`을 실행하고 같은 파일명의 해시와 비교합니다. 버전·사용자 이름·저장 위치는 실제 내려받은 파일에 맞춥니다.
- 조종기가 보이지 않으면 Windows의 `joy.cpl`에서 먼저 확인합니다. 게임의 **EXPORT DEVICE REPORT**는 진단용이며 자동으로 보정을 완료하거나 실제 스틱 입력을 만들어 주지 않습니다.
- 실행 후 문제가 있으면 **다운로드한 버전·오류 문구·발생 단계**와 사용자 저장 폴더의 진단 파일을 함께 확인합니다. 개인 보정 파일과 전체 사용자 폴더는 공개 저장소에 올리지 않습니다.

최신 검사 범위는 **[v0.7.4 변경·검증 기록](docs/releases/v0.7.4.md)**과 [기계 판독 결과](docs/verification/v0.7.4-summary.json)에 있습니다. Mode 1·Mode 2의 이전 근거는 [v0.7.3](docs/releases/v0.7.3.md), 시간 계산·자율 착륙 변경은 [v0.7.2](docs/releases/v0.7.2.md)를 보존합니다. 미완료 항목은 [순차 작업 목록](docs/sequential-work-2026-10-27.md)을 따릅니다. 실제 다운로드 가능 여부는 Releases의 Assets를 기준으로 확인합니다.

<a id="feedback-form"></a>
### 테스트 결과 보내기 — v0.7.4 배포 양식

1. ZIP 전체를 압축 해제한 폴더에서 실행기 옆의 **`feedback-form.html`**을 Edge 또는 Chrome으로 엽니다. 인터넷이나 로그인이 필요하지 않습니다.
2. 실제 실행한 **버전·ZIP 이름·조종기 배치·입력 방식**과 직접 확인한 증상, 재현 순서, 기대한 동작·실제 동작을 적습니다. 정밀 스로틀 문제라면 DIRECT/PRECISION 선택도 기록합니다.
3. **보고서 미리보기 → 내용 확인란 → TXT 보고서 저장 / JSON 보고서 저장** 순서로 저장합니다. 브라우저 다운로드 목록에서 파일이 생겼는지 확인합니다. 내용을 수정했다면 미리보기를 다시 확인하고 창 닫기·새로고침 전에 저장합니다.
4. 저장한 보고서와 필요한 화면·F6 비행 기록을 배포자에게 직접 전달합니다. 양식은 자동 전송하지 않으며 첨부란은 파일 이름·설명 메모입니다. 파일을 자동으로 읽거나 보고서에 포함하지 않습니다. 개인 `RadioProfile.json`이나 사용자 `Saved` 폴더 전체는 보내지 않습니다.

HTML은 게임·조종기 설정을 변경하지 않습니다. 양식을 열거나 저장할 수 없으면 같은 내용을 텍스트로 작성해 전달합니다. 검토한 [양식 원본](docs/feedback-form.html)과 배포 ZIP의 HTML·체크섬이 일치함을 확인했습니다. 실제 브라우저의 파일 연결 방식은 PC 환경에 따라 다릅니다.

첫 실행의 간소 조종기 화면에서 **실제 장치 카드 선택 → MODE 1 / MODE 2 선택 → STICK SETUP → CONFIRM & SAVE → ENABLE RADIO** 순서로 보정합니다. 상단 STEP 안내로 다음 행동을 확인합니다. 이미 보정한 장치는 **CONNECT SAVED RADIO / F8**로 연결할 수 있습니다. 스로틀 최저에서 패널을 닫고 Space로 ARM합니다. 보정값은 각 Windows 사용자의 `%LOCALAPPDATA%\ImperialWingsSimulator\Saved`에 저장하고 다른 사람의 보정값을 배포하지 않습니다.

[다른 PC에서 실행·조종기 설정](docs/PORTABLE_QUICK_START.ko.md)을 확인하세요. Windows 10/11 x64와 DirectX12 / Shader Model6 GPU가 필요합니다. 필수 Visual C++ 설치 파일과 자율 훈련용 독립 실행 환경을 포함합니다. 개발 PC에서 설치 도구 경로를 제거하고 한글·공백 경로로 옮긴 새 사용자 데이터 환경을 검사했으며, 실제 다른 PC 전체에서의 비행을 인증한 것은 아닙니다.

Unreal Engine 5.8 기반 Windows 드론축구 시뮬레이터입니다. 기본 시점은 **경기장 밖 조종자 위치에서 기체를 보는 LOS(육안) 조종**입니다. FIDA Class40의 경기장·기체 규격과 5대5 경기 흐름, USB 조종기 보정 UI, 실제 SDACS APF를 사용하는 자율 훈련을 함께 개발합니다.

기체는 네 모터의 개별 힘과 회전 모멘트, 중력·항력·Chaos 접촉으로 움직입니다. 정상 비행에서 위치를 강제로 갱신하지 않으며 초기화에서만 물리 위치를 되돌립니다. 실기체와 일치하도록 실측 보정된 제품은 아닙니다.

**v0.7.4 전력 제한:** 모터가 요구한 전력과 배터리에서 공급 가능한 전력을 연결해, 공급이 부족하면 실제 모터 추력·반작용 토크를 함께 낮춥니다. 항전 장비의 전력도 반영하고 빈 배터리에서는 모터 전력을 공급하지 않습니다. 화면의 **POWER LIMITED / THRUST %** 또는 **BATTERY EMPTY / NO MOTOR POWER**가 상태를 알립니다. 이는 실험용 모델이며 실제 기체의 배터리·ESC·추력 실측 보정을 완료한 것은 아닙니다. 정밀 스로틀과 전력 제한은 서로 다른 기능입니다.

![v0.7 내부 기자재의 실제 Unreal 렌더](docs/images/equipment-v07.png)

v0.7은 FC/ESC·수신기·모터 권선·배터리·커넥터·배선과 비틀린 프로펠러를 자체 메시로 추가했습니다. DISARM 후 **F9 EQUIPMENT**로 보호구를 숨겨 내부를 확인하고 다시 F9로 LOS에 돌아옵니다. 장비 확인 중에는 ARM하지 않습니다. 제품 CAD나 실측 디지털 트윈은 아닙니다.

**보기 분리:** 1 파일럿 LOS, 2 기체 추적 관찰자, 3 경기장 전체, 4 관객석, 5 심판 측면. 우클릭 드래그·휠은 해당 보기의 시선·확대에 적용합니다. 관객·심판 보기는 비행 조작을 제한하며, 수동 비행 중 진입하려면 먼저 DISARM합니다. 관측 카메라를 바꿔도 자율 훈련의 물리 계산은 계속됩니다.

**접촉 반응:** 충격량으로 가드 상태를 낮추고 마모 흔적·작은 파편 효과·이벤트·GUARD %를 표시합니다. 강체 충돌 형상은 유지하며 실제 부품 파단·모터 고장을 뜻하지 않습니다. Backspace/전체 RESET은 상태를 복원합니다. 골대는 규정 범위의12.5kg이며 연습에서 작은 앞뒤 충격 반응, 정규 경기에서 고정 지지를 사용합니다. 계수는 실험값입니다.

**경기/연습 구분:** AUTO·HOLD·RETURN과 기체 API의 자율 활성화는 Practice에서만 허용합니다. 정규 경기 종료는 자동 귀환을 시작하지 않으며 이미 비행 중인 수동 기체의 착륙 조종을 유지합니다. 정비·종료 단계의 새 ARM은 차단합니다.

**v0.7.1 입력 복구:** 창 포커스를 잃으면 수동 기체를 DISARM하고 누적 키보드 스로틀·축 입력과 USB 활성화를 해제합니다. 복귀 후 키 자동 반복으로 다시 움직이거나 ARM하지 않으며 새 키 누름/ARM 클릭이 필요합니다. USB 조종은 실제 최저 스로틀에서 F8로 다시 연결합니다. 메뉴·장비 보기·기체 전환·초기화에서도 남은 키보드 입력을 제거합니다. 메뉴에서 직접 활성화한 USB 연결은 메뉴를 닫아도 유지하며 원시 축·보정값은 바꾸지 않습니다. 명시적으로 시작한 연습 자율비행은 포커스 전환에도 계속됩니다.

[v0.7.1 입력 시험과 범위](docs/releases/v0.7.1.md)에서 엔진 내부 키 이벤트 시험과 실제 키보드/USB 현장 시험을 구분합니다.

**v0.7.2 시간·착륙 개선:** 긴 프레임에서도 모터·배터리·센서·네트 힘 계산이 Chaos 장면의 같은 시간 예산을 사용합니다. 100/250ms 멈춤에서 생략된 물리 시간은 HUD·F6 기록에 표시하며 경기 시계는 게임 시간으로 계속 갑니다. 실제 upstream APF의 착륙 직전 정지 문제도 보완해10대 출발·통신 장애·복구·귀환·착륙을 다시 검사했습니다. [시간 계약](docs/time-contract.md) · [최종 시험·한계](docs/releases/v0.7.2.md).

[중단·미완료 작업 목록과 재개 지점](docs/work-status-2026-10-27.md)에서 현장 조종기 시험·다른 PC·고정주기 제어와 계획서 후속 기능을 확인합니다. 이번 릴리스는 전체326개 기능의 완료를 의미하지 않습니다.


[v0.7 변경·시험 결과](docs/releases/v0.7.0.md) · [실제 기능과 미완료 항목](docs/feature-inventory-2026-10-26.md) · [자율 시험에서 찾은 문제와 수정](docs/autonomy-feedback-v07.md)

![이전 v0.6.1 Unreal 실행 장면](docs/images/simulator-runtime-v061.gif)

실제 Windows 실행 파일의 화면을 녹화한 GIF입니다. 명시적 물리 검사 시나리오의 자동 비행 장면이며 실물 USB 조종기 비행이나 게임 내 리플레이 기능의 증거는 아닙니다. [촬영·버전·검증 기록](docs/releases/v0.6.1.md).

[다른 시뮬레이터 기능 조사](docs/simulator-benchmark.md) · [사용자 제공 경기 영상과 물리 대조 계획](docs/video-physics-reference.md)

## 사용 안내 바로가기

- [다운로드 방법](#download-steps) · [실행 방법](#run-steps) · [첫 조종기 설정](#first-controller-setup) · [첫 비행](#first-flight) · [조작키](#controls)
- [데이터 수집 순서](#data-collection) · [저장 폴더](#saved-files)
- [F6 비행 기록](#flight-recording) · [F5 CSV](#csv-export) · [조종기 진단](#device-report)
- [파일 열기·피드백 공유](#feedback-files) · [기록 문제 해결](#troubleshooting)
- [조종기 설정·F8 재연결](#controller) · [화질](#graphics) · [경기·연습](#match)
- [소스 빌드](#source-build) · [버전별 검증](#verification)

<a id="first-flight"></a>
## 처음 실행한 뒤 첫 비행

1. 배포 폴더의 **ImperialWingsLauncher.exe**를 실행합니다.
2. 키보드라면 조종기 창에서 **USE KEYBOARD → CLOSE**를 선택합니다. USB 조종기는 안내에 따라 실제 장치를 보정·저장·활성화한 뒤 창을 닫습니다.
3. 오른쪽 기체 목록 또는 **Tab**으로 기체를 고릅니다. 처음에는 파란 팀 Striker로 연습합니다.
4. 처음 비행은 **START MATCH를 누르지 않고 Practice**에서 진행합니다. **1**은 경기장 밖 LOS, **3**은 전체 관찰입니다.
5. 키보드는 **↓**로 THR을0%까지 내리고, 조종기는 스로틀을 맨 아래로 둡니다. Mode 1은 오른쪽, Mode 2는 왼쪽 스틱 위아래가 스로틀입니다.
6. **Space / ARM**으로 모터를 허용합니다. 키보드는 **↑**를 짧게 눌러 스로틀을 올립니다. 키를 놓아도 변경한 스로틀은 유지됩니다.
7. **W/S, A/D, Q/E**로 기울기·회전을 조금씩 조절합니다. ANGLE은 수평 자세를 복원하며 고도·위치를 자동 고정하지 않습니다.
8. 스로틀을 조금씩 내려 착륙하고 Space로 DISARM합니다. 선택 기체는 **Backspace**, 전체 상태는 **Home / RESET**으로 초기화합니다.
9. 종료는 상단 **EXIT** 또는 창 닫기를 사용합니다. 기록 중이면 아래 안내대로 F6를 먼저 눌러 저장을 확인합니다.

스로틀·기체 선택·입력 방식·경기 단계가 ARM 조건에 영향을 줍니다. USB 비행 중 다른 앱으로 전환하면 입력이 해제·DISARM될 수 있으므로 파일 확인은 착륙·기록 종료 후 진행합니다.

<a id="data-collection"></a>
## 시뮬레이터 데이터 수집 방법 — v0.6

현재 수집 기능은 아래 세 종류입니다. 기록은 이 PC에 저장하며 GitHub·개발자에게 자동 전송하지 않습니다.

| 목적 | 조작 | 파일·폴더 | 무엇을 확인할 수 있나 |
|---|---|---|---|
| 시간에 따른 비행·입력 변화 | **F6 / RECORD FLIGHT** 시작, 다시 눌러 종료 | `FlightLogs/*.jsonl` | 시각·선택 기체·USB 상태와10대의 위치·속도·자세·스로틀 |
| 그 순간의 상세 상태 | **F5 / EXPORT CSV** | `Reports/*-fleet.csv` |10대의 입력/비행 모드·배터리·빌드·배치·보정 상태 |
| 조종기 연결 문제 | **C → EXPORT DEVICE REPORT** | `Diagnostics/radio-*.json` | 발견 장치·선택 장치·축/버튼 수·연결/보정/활성 상태 |

**F6는 연속 상태 기록, F5는 한 번의 상태 내보내기입니다.** 파일 형식과 포함 항목이 다르므로 문제를 재현할 때 함께 사용하면 좋습니다. F6는 영상 녹화나 완성된 경기 리플레이가 아닙니다.

### 권장 수집 순서

1. 한 번에 확인할 문제를 정합니다. 예: 기체가 한쪽으로 기울어짐, 스로틀 반응 이상, USB 연결 해제.
2. 버전, 입력 방식, 기체, 화질과 재현 절차를 메모합니다. **Home** 초기화가 필요하다면 기록을 시작하기 전에 수행합니다.
3. 조종기 보정을 마치고 창을 닫습니다. 시작은 바닥·DISARM·최저 스로틀 상태로 둡니다.
4. **F6**를 한 번 누릅니다. 전체 패널에서 버튼이 **F6 STOP RECORDING**으로 바뀌는지 확인합니다. 버튼이 숨겨져 있다면 **F10**으로 패널을 표시합니다.
5. **F5**로 시작 상태 CSV를 남깁니다.
6. 평소 조종으로 증상을 재현합니다. 가능하다면 발생 직후 F5를 한 번 더 누르고 대략적인 발생 시점을 메모합니다.
7. 착륙·DISARM 후 마지막 F5를 저장하고 **F6**를 다시 누릅니다. **Flight recording saved** 안내와 **F6 RECORD FLIGHT** 버튼 복귀를 확인합니다.
8. 게임을 닫거나 조종을 멈춘 뒤 아래 저장 폴더를 엽니다.
9. 최근 JSONL과 관련 CSV를 확인합니다. 연결 문제라면 장치 진단도 추가합니다.
10. 다른 시도는 F6를 새로 시작해 별도 파일로 남깁니다. 하나의 파일에서 여러 번 초기화했다면 메모에 적습니다.

비행 중 기록 버튼을 반복해서 누르면 시작/종료가 번갈아 실행됩니다. 기록 때문에 끊김이 느껴진다면 착륙 후 기록을 중지하고 그 현상도 메모합니다. 다른 앱을 열어 파일을 확인하는 행동 자체가 USB 입력 해제를 일으킬 수 있습니다.

<a id="saved-files"></a>
### 저장 폴더 찾기

**Windows 포터블 배포 기본값**

1. **Win+R**을 누릅니다.
2. 아래 경로를 그대로 붙여넣고 Enter를 누릅니다.

```text
%LOCALAPPDATA%\ImperialWingsSimulator\Saved
```

일반적으로 `C:\Users\<Windows사용자>\AppData\Local\ImperialWingsSimulator\Saved`입니다. AppData가 숨겨져 있어도 위 방법으로 열 수 있습니다.

```text
Saved/
├─ FlightLogs/                 # F6 연속 비행 JSONL
├─ Reports/                    # F5 상태 CSV
├─ Diagnostics/                # EXPORT DEVICE REPORT의 JSON
├─ Config/
│  ├─ RadioProfile.json        # 개인 조종기 보정
│  └─ VisualQuality.txt        # 사용자 화질
└─ Runtime/
   └─ <실행별 폴더>/
      ├─ launcher.log          # 포터블 실행기·자율 연결 로그
      └─ guidance-ready.json   # 생성된 경우 연결 주소·준비 정보
```

각 폴더는 해당 기능을 사용한 뒤 생성됩니다. 배포 ZIP 옆에서 파일을 찾지 말고 **사용자 데이터 폴더**를 확인합니다. v0.6은 새 폴더에 업데이트해도 같은 Windows 사용자의 설정을 공유합니다.

에디터는 프로젝트 `Saved`를 사용하며, 시험에서 `-ImperialDataDir`을 지정했다면 그 지정 폴더를 사용합니다. 이전 v0.5 패키지의 저장 경로와는 다를 수 있습니다.

<a id="flight-recording"></a>
### F6 JSONL에 들어 있는 데이터

한 줄이 하나의 JSON 기록입니다. 파일을 시작할 때 `start`, 기록 중 `sample`, 정상 종료할 때 `stop` 사건을 남깁니다. F6를 새로 시작하면 새 파일을 만듭니다.

일반 샘플은 게임 tick에서 **0.05초 이상 간격**으로 추가합니다. 약20회/초를 상한으로 하는 방식이며 고정20Hz 보장·모든 제어 계산 기록이 아닙니다. 프레임 지연이나 저장 지연에 따라 간격이 길어질 수 있습니다.

| 필드 | 의미·단위 |
|---|---|
| `event` | `start / sample / stop` |
| `utc` | 기록 순간의 UTC 날짜·시각 |
| `wall_s` | 단조 증가 시간 기준, 초. 값의 차이로 간격을 확인하며 달력 시각이 아님 |
| `game_s` | 세션 경과 시간. Home/RESET에서 초기화될 수 있으며 세트 남은 시간이 아님 |
| `selected` | 선택 기체의0부터 시작하는 번호 |
| `source_sha256` | 실행 파일의 빌드 입력 지문 |
| `radio_connected / radio_enabled` | USB 장치 연결 / 조종 활성 여부 |
| `stick` | 보정된 USB `[Roll, Pitch, Yaw, Throttle]`. 앞3축은−1~1, 스로틀은0~1 |
| `aircraft` | 선택 기체만이 아닌10대의 상태 배열 |

`aircraft`의 각 항목에는 아래 필드가 있습니다.

| 필드 | 의미·단위 |
|---|---|
| `id` |0~9. 파란 팀0~4, 빨간 팀5~9 |
| `armed / autonomous` | 모터 허용 / 자율 제어 여부 |
| `input_source` | `KEYBOARD / USB / AUTO`. 명시적 자동 시험은 별도 fixture 표기를 사용할 수 있음 |
| `flight_mode` | 비행 제어 모드 |
| `position_m` | 경기장 좌표 `[X, Y, Z]`, 미터 |
| `speed_mps` | 속력, m/s |
| `throttle` | 정밀 곡선 적용 전 스로틀 입력0~1. USB인 경우 축 보정은 완료된 값이며 원시 HID 패킷은 아님 |
| `throttle_command / precision_throttle` | v0.7.4의 실제 스로틀 명령0~1 / 해당 수동 기체의 정밀 곡선 사용 여부 |
| `flight_model_s / sensor_model_s` | 누적 비행 모델 / 센서 모델 계산 시간, 초 |
| `battery_current_a / battery_consumed_ah` | 배터리 전류A / 누적 소모량Ah |
| `battery_delivered_power_w / battery_power_limited` | 배터리 공급 전력W / 공급 한도에 따른 제한 여부 |
| `motor_requested_power_w / motor_available_power_w / motor_applied_power_w` | 모터 요구 / 사용 가능 / 실제 적용 전력W |
| `motor_thrust_scale` | 전력 한도를 적용한 추력 비율0~1 |
| `tilt_deg` | 기체 기울기, 도 |
| `rotation_deg` | `[Roll, Pitch, Yaw]`, 도 |

USB `stick`은 원시 HID 패킷이나 키보드 키 입력 기록이 아닙니다. `radio_enabled`와 각 기체의 `input_source`를 함께 확인합니다. JSONL의 숫자 `id=0`은 CSV의 `IW-01`에 대응합니다.

**현재 F6 기록에 없는 항목:** 연속 배터리 전압·개별 모터 힘/RPM·자이로·PID 각 항·키보드 누른 키 전체·확정 득점/심판 사건·영상. 배터리 잔량·전압과 보정 상태는 F5로 순간 확인할 수 있습니다. v0.7.4의 스로틀 명령·전력 제한 항목은 이전 버전의 기록에 없을 수 있으므로 실제 기록의 버전·필드부터 확인합니다. 더 풍부한 진단·경기 사건 기록은 [개발 계획](docs/development-roadmap.md#feedback-data-plan)에 있습니다.

F6 파일명과 `utc`는 UTC 기준입니다. Windows 파일 탐색기의 표시 시간, CSV 파일명의 PC 시각과 구분합니다. 비정상 종료로 `stop`이 없거나 마지막 줄이 미완성일 수 있으며 이를 정상 종료 기록으로 바꾸어 해석하지 않습니다.

화면의 EXIT는 전체 초기화 후 종료하므로 마지막 비행 상태가 필요하다면 착륙·DISARM 후 **F6를 먼저 종료**합니다. Home/RESET도 기록 중 기체 위치·경과 시간을 초기화할 수 있습니다.

<a id="csv-export"></a>
### F5 CSV로 순간 상태 확인하기

게임에서 **F5** 또는 **EXPORT CSV**를 누릅니다. 한 번 누를 때마다 `Reports`에 새 CSV를 저장합니다. 첫 줄은 열 이름이고 그 아래는10대 기체의 상태입니다.

| 열 묶음 | 대표 열 | 읽는 방법 |
|---|---|---|
| 기체·선택 | `id, team, role, selected` | `IW-01~IW-10`, 팀·역할·선택 여부 |
| 조종 상태 | `armed, autonomous, input_source, flight_mode, throttle` | 모터·자율 여부와 입력 방식, 스로틀0~1 |
| USB 상태 | `radio_connected, radio_calibrated, radio_enabled` | 연결·보정·활성은 각각 별도 상태 |
| 비행·배치 | `x_m, y_m, z_m, speed_mps, roll_deg, pitch_deg, yaw_deg` | 위치m, 속력m/s, 자세도 |
| 전력·접촉 | `battery_soc, battery_v, net_deflection_m` | 잔량0~1, 전압V, 네트 처짐m |
| 버전 | `build_version, build_commit, build_source_sha256, build_utc` | 어떤 실행 파일에서 얻었는지 확인 |
| 부가 상태 | `radio_setup_phase, radio_setup_samples, radio_panel_open, flight_recording, visual_quality` | 보정 진행·패널·F6 기록·화질 상태 |

불리언 열은 보통 `1 / 0`입니다. 예를 들어 `radio_connected=1`이어도 `radio_enabled=0`이면 USB 조종 활성 상태가 아닙니다.

CSV의 `build_utc`는 **빌드 시각**이며 CSV를 저장한 시각이 아닙니다. CSV에는 별도의 연속 샘플 시각·확정 경기 점수 열이 없으므로 F6·메모와 함께 봅니다. UTF-8 CSV를 지원하는 스프레드시트 프로그램에서 열 수 있습니다.

<a id="device-report"></a>
### 조종기 연결 진단 파일 만들기

1. 기체를 착륙·DISARM하고 **C / RADIO / USB**를 엽니다.
2. 조종기가 안 보이면 **REFRESH**, 처음 선택은 장치 카드 또는 **NEXT DEVICE**로 확인합니다. Windows의 `joy.cpl`에서도 실제 스틱이 움직이는지 봅니다.
3. 창 아래의 **EXPORT DEVICE REPORT**를 클릭합니다.
4. 저장 성공 메시지를 확인하고 `Diagnostics`의 최근 `radio-*.json`을 엽니다.
5. 연결 전/후를 비교하려면 각 상태에서 저장하고 파일명·수정 시각을 확인합니다.

보고서에는 `utc, build, source_sha256, input_api, profile_path, connected, calibrated, enabled, devices`가 들어갑니다. 장치별로 `slot, name, driver_id, axes, buttons, selected`를 확인합니다.

이 버튼은 보정값을 새로 만들거나 ARM하지 않습니다. **보정 프로필 자체나 원시 축 시계열을 내보내는 기능은 아닙니다.** `profile_path`에 Windows 사용자 이름이 포함될 수 있으므로 공유 전에 내용을 검토합니다.

<a id="feedback-files"></a>
### 파일 열기와 개발 피드백 보내기

처음에는 `FlightLogs`·`Reports`·`Diagnostics`를 파일 탐색기에서 **수정한 날짜 순**으로 정렬합니다. JSON/JSONL은 텍스트 편집기로, CSV는 스프레드시트 프로그램으로 엽니다.

문제를 전달할 때 다음 정보를 함께 적습니다.

```text
게임 버전:
입력 방식과 조종기:
선택 기체·경기/연습 상태:
화질·해상도:
재현 순서:
기대한 동작 / 실제 동작:
문제가 발생한 대략적인 시점:
첨부 파일: 관련 JSONL·발생 전후 CSV·필요한 장치/실행기 진단
```

관련 파일만 별도 폴더에 복사하고 내용을 확인한 뒤 필요하면 Windows의 압축 기능으로 ZIP을 만듭니다. **Config/RadioProfile.json이나 Saved 전체를 그대로 공개 업로드하지 않습니다.** 장치명·`profile_path`·실행기 로그의 개인 경로도 확인합니다.

자동 개인정보 제거·진단 ZIP 일괄 생성·개발자 자동 전송·버전 호환 리플레이는 아직 지원하지 않습니다. 생성한 파일을 선택해 수동으로 공유하는 절차입니다.

**PowerShell로 최근 JSONL의 마지막 한 줄 보기 — 선택 사항**

파일을 쓰는 중에는 작업하지 말고 F6 기록을 종료한 뒤 사용합니다. 읽기만 수행하는 예시입니다.

```powershell
$flightFolder = Join-Path $env:LOCALAPPDATA 'ImperialWingsSimulator\Saved\FlightLogs'
$recentFlight = Get-ChildItem -LiteralPath $flightFolder -Filter '*.jsonl' -File |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if (-not $recentFlight) { throw 'F6로 저장한 비행 파일이 없습니다.' }

$lastRecord = Get-Content -LiteralPath $recentFlight.FullName -Tail 1 -Encoding UTF8 |
    ConvertFrom-Json
$lastRecord | Select-Object event, utc, game_s, selected, radio_connected, radio_enabled
$lastRecord.aircraft | Select-Object id, armed, input_source, flight_mode, speed_mps, throttle
```

비정상 종료 파일의 마지막 줄이 손상되어 JSON 오류가 나면 원본을 보존합니다. 미완성 줄을 통과한 기록처럼 고치지 말고, 읽을 수 있는 이전 줄과 종료 상황을 함께 확인합니다.

<a id="troubleshooting"></a>
### 데이터 수집·조종 문제 해결

| 증상 | 확인할 내용 |
|---|---|
| F6 버튼이 안 보임 | F10으로 전체 패널 표시. 게임 창에 입력 포커스가 있는지 확인 |
| F6 파일이 없음 | 버튼이 STOP RECORDING으로 바뀌었는지, 사용자 데이터 폴더가 맞는지 확인 |
| 파일을 배포 폴더에서 찾을 수 없음 | Win+R로 `%LOCALAPPDATA%\ImperialWingsSimulator\Saved` 열기 |
| F5만 눌렀는데 비행 경로가 없음 | F5는 순간 상태. 시간에 따른 변화는 F6로 기록 |
| JSONL에 배터리/PID/득점 이력이 없음 | 현재 포함 항목과 계획 항목을 구분. 배터리는 F5 순간 상태로 확인 |
| USB 값이0인데 기체는 움직임 | `radio_enabled`와 `input_source` 확인. 키보드·AUTO·관성 이동과 구별 |
| 파일 확인 후 USB 비행이 멈춤 | 다른 앱으로 포커스가 옮겨져 DISARM될 수 있음. 최저 스로틀에서 명시적으로 재연결·ARM |
| 기록 시간이 갑자기 작아짐 | Home/RESET으로 `game_s`가 초기화됐는지 확인. `utc/wall_s`와 메모를 함께 사용 |
| Device report 저장 실패 | Windows 사용자 폴더 쓰기 상태·디스크 여유와 게임 상태 메시지 확인 |
| 기록이 끊기거나 간격이 늘어남 | 게임 프레임·디스크 부하 영향. 고정20Hz 또는 USB 입력 지연 측정값으로 해석하지 않음 |
| JSON 마지막 줄을 못 읽음 | 비정상 종료·쓰기 중 파일 여부 확인, 원본과 불완전 종료 상태 보존 |

<a id="graphics"></a>
## 화질과 세부 기능

### 화질 저장

전체 패널 왼쪽 **GRAPHICS**에서 **BALANCED / HIGH / CINEMA**를 클릭합니다. 저장된 설정이 없는 포터블 첫 실행은 Balanced를 사용합니다. 화면이 끊기면 Balanced부터 확인합니다.

선택은 사용자 데이터의 `Config/VisualQuality.txt`에 저장합니다. 먼지 효과는 접촉·저고도 추력의 시각 표현이며 모터 고장이나 실제 파괴 판정이 아닙니다.

<a id="positions"></a>
### 기체 선택·시작 위치·식별 표시

- **Tab / 오른쪽 기체 목록**으로 선택합니다. 조종 기체를 바꿀 때 이전 수동 기체가 DISARM될 수 있으므로 착륙 후 전환합니다.
- Striker는 양 팀 자기 시작선 중앙입니다. 조종자 기준 왼쪽부터 **Libero → Guide → Striker → Sweeper → Keeper**이며 반대 팀은 대칭입니다.
- Striker 초록·Guide 분홍 하단 띠는 좌우 각각2개입니다. 현재는 시각 표현이며 탈락·득점 자격 연동·Guide 승계는 후속 개발입니다.
- 현재 흰색 전방/빨간색 후방 표시는 훈련 설계입니다. 공식 역할색 조건과 사용자 엄격 태그 정책의 구분은 [정밀 계획](docs/simulator-plan-review.md)을 확인합니다.

<a id="source-build"></a>

## 개발 소스 빌드 환경과 설치

필요 환경은 Windows, Unreal Engine 5.8, Visual Studio 2022 C++ 빌드 도구와 Windows SDK, Python 3.12, Git입니다. 실제 검증 PC는 UE 5.8.3 / RTX 5070 Ti / RAM 32GB입니다.

```powershell
git clone https://github.com/sun475300-sudo/Drone-Soccer-Simulator.git
Set-Location Drone-Soccer-Simulator

# 아래 두 설치 경로를 자신의 PC에 맞춥니다.
$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
$PythonExe = 'C:\Path\To\Python312\python.exe'
$SdacsRoot = [IO.Path]::GetFullPath((Join-Path $PWD '..\swarm-drone-atc'))

.\Scripts\Configure-Environment.ps1 -EngineRoot $EngineRoot -Python $PythonExe -RepoRoot $SdacsRoot
.\Scripts\Setup-Bridge.ps1
.\Scripts\Build.ps1 -Package
.\Scripts\Launch.ps1
```

`Setup-Bridge.ps1`는 외부 SDACS 저장소가 없으면 복제하고 프로젝트 `.venv`에 최소 의존성을 설치합니다. `Build.ps1`은 C++ 에디터 모듈을 빌드하며 맵이 없거나 `-GenerateContent`를 지정한 경우에만 자체 콘텐츠를 생성합니다. PC별 경로는 Git에서 제외한 `SimulatorEnvironment.local.json`에 저장하며 환경변수와 명시적 인자로 재지정할 수 있습니다.

배포 실행 파일을 만들려면 `Build.ps1`에 `-Package`를 추가합니다. 개발 패키지는 `dist/Integrated/Windows/`에 생성됩니다. 실행 배포 ZIP은 `pip install -r Bridge/requirements-portable-build.txt` 후 `Scripts/Build-Portable.ps1 -OutputDirectory <새 폴더>`로 별도 작성합니다. 실행 파일 주변의 Engine·프로젝트 폴더도 함께 필요합니다. 빌드된 PC에서는 `ImperialWings.cmd`로 브리지와 게임을 시작할 수 있습니다. 저장소에는 엔진·패키지 실행 파일·가상환경을 포함하지 않습니다.

<a id="controls"></a>
## LOS 조종과 화면

기본 조종자 카메라는 선택한 팀의 자기 진영 바깥 `x=±9.5m, z=1.7m`에 있습니다. 기체를 선택하면 그 팀·역할에 대응하는 조종석으로 이동합니다.

| 조작 | 동작 |
|---|---|
| 1 | 기본 LOS 조종자 시점 |
| 우클릭 드래그 / 마우스 휠 | LOS 시선 회전 / 시야각 확대·축소 |
| 2 / 3 | 외부 추적 관찰 / 경기장 전체 관찰 |
| 4 / 5 | 관객석 / 심판 측면, 비행 조작 제한 |
| F9 / EQUIPMENT | DISARM 후 내부 장비 확인 / LOS 복귀 |
| Tab / 오른쪽 기체 목록 | 조종할 기체 선택 |
| Space | 낮은 스로틀에서 ARM / DISARM |
| ↑ / ↓ | 키보드 스로틀 증가·감소 |
| W / S, A / D, Q / E | Pitch, Roll, Yaw |
| Backspace / Home | 선택 기체 초기화 / 전체 초기화 |
| C / RADIO USB | USB 장치·축 보정 패널 |
| F10 | 넓은 LOS 화면 / 관제 패널 전환 |
| F5 / EXPORT CSV | 실제 물리 상태를 CSV로 저장 |
| F6 | 실제 비행·입력 기록 시작/종료 |
| F8 | 최저 스로틀에서 일치하는 저장 조종기 연결/해제 |
| GRAPHICS | Balanced/High/Cinematic 화질 선택·사용자별 저장 |

수동 비행은 모든 기체에 **ANGLE 모드**를 적용합니다. Roll/Pitch는 최대35°의 목표 기울기를 명령하며 스틱이나 키를 놓으면 수평으로 복원합니다. Yaw는 각속도,고도는 스로틀로 직접 조절합니다. 기본 조종 시점은 경기장 밖 조종자의 육안 위치에 고정합니다.

<a id="boxer"></a>
<a id="controller"></a>
### 조종기 설정·저장 프로필·F8 재연결

조종기는 실제 장치 선택 후 축 배정·최대/최소·중립·반전·최저 스로틀을 확인합니다. 기본값은 키보드이며 미보정 USB 입력으로 ARM하지 않습니다. 개발 PC에서 사용한 조종기의 실제4축 보정·저장·USB 활성화·최저 스로틀 ARM은 확인했고 연속 USB 비행·케이블 분리 복구·입력 지연 검증은 남아 있습니다. 자기 PC의 장치는 직접 확인합니다. [USB 조종기 설정](docs/BOXER_SETUP.md)을 참고합니다.

**MODE 1 / MODE 2**를 선택한 뒤 **STICK SETUP**에서 각 축을 한 단계씩 실제로 움직여 축과 방향을 찾는 안내 화면입니다. 각 단계에서 Enter로 시작·끝값 확인을 할 수 있고,중립·최저 스로틀 확인 후 검토 화면에서 따로 저장합니다. 설정 중에는 비행 단축키가 차단되며 취소하면 이전 프로필이 유지됩니다. 이 기능을 추가한 것과 실물 스틱 보정·USB 비행을 완료한 것은 구분합니다.

v0.6.1의 기본 화면은 **장치 선택 → 안내 보정 → 검토/저장 → 활성화**를 상단 STEP으로 표시합니다. 수동 축 배정·반전·데드존·범위 수집·프로필 저장/불러오기는 **ADVANCED OPTIONS**에 있습니다. **SIMPLE VIEW**로 돌아올 수 있으며, 설정 창을 다시 열면 간소 화면에서 시작합니다. 이미 보정한 장치는 **CONNECT SAVED RADIO**를 누릅니다. 연결과 ARM은 별도입니다.

![실제 실행 파일의 간소 조종기 화면](docs/images/controller-simple-v061.png)

### 다음 실행에서 F8로 연결하기

1. 처음 한 번은 실제 장치를 선택해 보정하고 **CONFIRM & SAVE**를 마칩니다.
2. 다음 실행에는 조종기의 USB Joystick 연결과 최저 스로틀을 확인합니다.
3. 보정창을 닫고 **F8**을 누릅니다. 현재 연결 장치 중 저장 프로필과 일치하는 장치가 정확히 하나여야 연결됩니다.
4. 실패하면 **C**를 열어 장치를 명시적으로 선택합니다. 슬롯·펌웨어·모델 출력이 바뀌었다면 실제 축을 다시 확인합니다.
5. USB 활성화 후에도 기체는 DISARM입니다. 창을 닫고 **Space**로 별도 ARM합니다.

USB가 이미 활성화된 상태에서 F8을 누르면 USB를 해제합니다. **F8은 선택 기체를 MANUAL/DISARM으로 전환하므로 비행 중 재연결 용도로 누르지 않습니다.** 저장된 값이 있어도 자동 이륙하지 않습니다.

<a id="match"></a>
## 경기와 자율 훈련

기본 규정 프로필은 [FIDA Class40 제5차 개정, 2026-01-01 시행](https://fidasystem.com/rules.html)입니다. 권장 경기장 **16×7×4.5m**, 보호구 외경 **40cm**, 골대 내경 **60cm**이며 골대는 양 끝에서 1.5m 안쪽, 중심 높이 3.0m에 배치합니다. 현재 기체 질량 0.9kg은 규정 상한 1.1kg 이내의 시뮬레이터 설정값입니다. STRIKER V3의 실측 질량으로 주장하지 않습니다.

팀당 5명 중 스트라이커만 득점합니다. 기체 전체가 상대 골대를 앞에서 뒤로 통과해야 하며 팀 오프사이드는 활성 기체 전원이 자기 진영으로 복귀해야 해제됩니다. 경기 코어는 10초 준비, 180초 세트, 300초 정비와 최대 3세트·2선승을 관리합니다. 준비·정비·종료 단계의 ARM은 제한합니다. 경기를 시작하는 동작이 자동 이륙을 뜻하지 않습니다.

**공식 경기 모드는 수동 조종**, SDACS 자동 비행은 **Practice의 훈련 보조**로 구분합니다. AUTO FLEET/Enter는 자율 훈련, HOLD/H는 현 위치 유지, RETURN/R은 홈 패드 귀환·착륙, MANUAL은 선택 기체를 수동으로 전환하고 DISARM합니다. 완전한 인간 심판 판정, 공격수 승계, 실제 페널티 경기·연장·승부치기·VAR은 구현 범위 밖입니다. 규격과 구현 한계는 [규정 근거](docs/rules-sources-2026.md)에 정리했습니다.

v0.7부터 정규 경기의 AUTO/HOLD/RETURN·직접 자율 API를 차단하고 세트 종료 시 수동 착륙 조종을 유지합니다. [v0.7 검증 기록](docs/releases/v0.7.0.md)을 참고하세요. 페널티·역할 승계·전체 심판 기능 등 [계획서의 후속 항목](docs/simulator-plan-review.md)은 남아 있어 완전한 공식 경기 운영판으로 사용하지 않습니다.

## 기체 모델과 실험 물리

외형은 제조사 자료를 참고해 직접 작성한 **STRIKER V3 참고형 절차적 모델**입니다. 흰색40cm·42개 셀(오각형12/육각형30)의 자체 보호구와 둥근 접합부, 검은 X 프레임·적도 링, 끝이 둥근3엽 프로펠러, 모터 냉각 슬롯·하부 배터리·전원 배선·클램프·체결부를 표현합니다. 제조사 메시·사진·설명서·로고를 복제한 공식 제품 모델이 아니며 제작 치수와 비행 특성을 정확히 재현했다고 주장하지 않습니다. [공식 자료와 관찰·가정 구분](docs/model-reference.md)을 참고합니다.

고급 물리 확장은 실험 단계입니다. 엔진 독립 모델과 Unreal 통합에서 다루는 항목은 다음과 같습니다.

- 비선형 PWM-추력, 모터 응답·포화, 회전자 동력 추정
- 배터리 소모·내부저항에 따른 전압 강하·출력 제한
- 부품 질량 배분에 따른 무게중심·관성 텐서, 회전자 자이로 토크
- 상대 풍속 기반 항력, 하향류, 지면·천장·벽 근접 보정
- IMU 바이어스·잡음·지연과 PID 적분 포화 방지
- 제한된 스프링·댐퍼 접촉 및 네트·보호구 변형 근사

계수는 조정 가능한 가정값입니다. CFD, 모터 벤치 측정, 실기체 IMU·추력·충돌 측정을 대체하지 않습니다. 각 모델의 존재와 실제 게임에서 검증된 통합 범위는 구별하며 최신 결과는 [검증 기록](docs/verification.md)에 남깁니다.

<a id="sdacs"></a>
## 실제 SDACS 연동

[기반 저장소](https://github.com/sun475300-sudo/swarm-drone-atc)의 실제 `simulation/apf_engine/apf.py`를 외부 작업 사본에서 import합니다. Unreal이 위치·속도를 미터 단위로 10Hz 전송하고 upstream `batch_compute_forces`·`force_to_velocity`가 짧은 목표점 생성에 참여합니다. Unreal이 물리 상태의 원본이며 브리지는 합성 기체 위치를 보내지 않습니다.

현재 확인한 upstream 커밋은 `1e9d826265069696d7e415c7778d17483d7a8580`입니다. 개발 실행의 `http://127.0.0.1:8767/health`에 실제 origin·커밋·로드한 파일·SHA-256이 표시됩니다. 실행 중 데이터 연결은 localhost이며 외부 통신은 소스 복제·의존성 설치에 사용합니다. 연결이 끊기면 자율 임무를 로컬 위치 유지로 전환합니다. [연동 계약](docs/sdacs-integration.md)과 [외부 권리 고지](THIRD_PARTY_NOTICES.md)를 참고합니다.

<a id="verification"></a>
## 이전 버전의 실행·검증 기록

아래 R4~R6 수치는 당시 기록입니다. 최신 [v0.7.2 시간·착륙·배포 시험](docs/releases/v0.7.2.md), [v0.7.1 입력 시험](docs/releases/v0.7.1.md), [v0.7 모델·접촉·시점·자율 시험](docs/releases/v0.7.0.md), [v0.6.1 기록](docs/releases/v0.6.1.md)과 [누적 검증 기록](docs/verification.md)을 확인합니다. 수집 절차 문서 추가와 실제 USB 비행·성능 측정 결과를 구분합니다.

![Unreal 실제 경기장 렌더](docs/images/arena-r4.png)

2026-10-26 R4의 Windows 에디터 빌드·패키징·실행을 확인했습니다. 실제 키보드/마우스 조작 **96개 항목 모두 통과**: 수동 ARM·상승·높은 스로틀 ARM 거부,USB 미보정 활성화 거부,10대 AUTO/HOLD/RETURN·착륙,세 시점,기체 선택·CSV·초기화 흐름을 검사했습니다. 귀환·전원 DISARM은11.82초였습니다.

고정 게임 계산 간격30/60/120Hz에서 실제 Chaos 호버·수평 이동·바닥/벽·5m/s 네트 충돌·스트라이커 골대 통과와 오프사이드도 모두 통과했습니다. 마지막 목표 오차는0.456/0.284/0.202cm,네트 모델 최대 처짐은5.30/6.72/7.47cm였습니다. 독립 검사는 규정51개·USB 보정159개·물리83개·실제 APF HTTP8개가 통과했습니다. 이 결과가 실기체 물리 정확도나 모든 경기 상황을 인증하는 것은 아닙니다.

1920×1080 경기장 장면의 실제 패키지 렌더 기록은 **평균88.16FPS**,RTX5070Ti입니다. 오프스크린 렌더 중 측정했으며 RTX5060이나 USB 입력 지연의 측정값은 아닙니다. 로그와 이전 중단 기록을 포함한 [검증 문서](docs/verification.md)를 참고합니다.

![STRIKER V3 참고형 기체의 실제 렌더](docs/images/airframe-r4.png)

역할별 시작 패드·색상·라벨·중앙선 방향 화살표와,흰색 전방 한 줄·빨간색 후방 두 줄 LED를 추가했습니다. 역할 색상은 FIDA 기준이며 방향등과 포지션 좌우 순서는 훈련용 설계 선택입니다. [시작 위치·LED 출처와 설계](docs/position-and-led-design.md),[물리 모델별 구현 범위](docs/physics-models.md)에 근거와 한계를 정리했습니다.

```powershell
.\Tests\run-rules-tests.ps1
.\Tests\run-radio-tests.ps1
.\Tests\run-physics-tests.ps1
$env:SDACS_REPO = $SdacsRoot
.\.venv\Scripts\python.exe .\Bridge\test_sdacs_bridge.py
.\Tests\run-runtime-tests.ps1 -Packaged

# 현재 연결된 실제 Windows 조이스틱을 읽기만 합니다.
& $PythonExe .\Tests\radio_probe.py --duration 20 --output .\Saved\radio-observation.json
```

화면·CSV·실행 로그·USB 보정 데이터·다운로드 원문은 로컬 `Saved/`에 보관합니다. 공개 소스에는 자체 제작 자산·렌더와 재현 가능한 시험을 포함합니다. Boxer의 실제 채널 보정·USB 비행,독립 고정 물리 스레드 PID,실기체 계수 측정,보호구/네트 정밀 연성체·CFD와 전체 심판 운영 기능은 후속 항목입니다.
공식 경기 영상 참고 링크와 이번 실행 창에서 확인한 역할별 패드·후미 LED는 [경기 영상 참고·게임 대조 기록](docs/match-video-references.md)에 정리했습니다. 자동 재생 제한으로 영상의 장면별 분석은 미완료이며,현재 구현의 규정 근거와 구분했습니다.

R5에서는 Mode 2 안내 보정을 추가하고 새 Windows 패키지에서 **화면 조작119개·보정 수학159개 전부 통과**했습니다. 실제 Boxer를 선택해 라이브 수집·정지 입력 완료 거부·취소 시 프로필 보존을 확인했습니다. 실물의 단계별 스틱 보정과 USB 수동 비행은 남아 있습니다. [실제 보정 화면](docs/images/radio-mode2-setup.png)과 [R5 검증 기록](docs/verification.md)을 참고합니다.

R6 / v0.4.0에서는 **10대 전부 ANGLE**로 전환했습니다. 실제 Chaos에서20/30/60/120/240FPS 조건의 초기 기울기 복원·스틱 해제 후 수평 복원을 모두 통과했습니다. 이 시험은 명시적 비USB 입력 fixture와 고도 보상을 사용해 자세 제어를 분리하며 실물 조종기 비행 성적으로 주장하지 않습니다. 실제 화면의69개 검사에서10대의 시작점·홈·렌더 패드·초기 방향과 각 LOS 조종자 위치도 확인했습니다.

실물 Boxer의 Roll=X,Pitch=Y,Yaw=V,Throttle=Z 배정과 실제6,454샘플의 끝값·중립·최저 스로틀 보정을 저장했습니다. 미완료 스틱 범위가 마지막 중립에서 거부되던 안내 흐름은 시작 중립의 양쪽 범위를 먼저 확인하도록 수정했고 보정 수학161개가 통과했습니다. USB 활성화·최저 스로틀 ARM까지 실제 확인했으며 연속 USB 비행은 포커스 전환으로 완료하지 못했습니다.

화면·CSV·물리 결과에는 프로젝트 버전,빌드 시각,소스 커밋과 빌드 입력SHA-256을 기록합니다. 실행 스크립트는 현재 빌드 입력과 실행 파일의 불일치를 표시합니다. 검증한 R6 스냅샷은 `01fa67e132697ad826c30bec585ca45242e3c9df97be4347689efd63046237c9`입니다. 빌드 뒤 작업 폴더에 추가된 그래픽 효과 소스는 이 스냅샷의 검사와 구분합니다. [버전별 검증](docs/verification.md),[개발 순서](docs/development-roadmap.md)를 참고합니다.

포터블 배포 실행기는 실제 MIT APF 소스·검증 해시를 포함한 독립 Windows 런타임을 사용합니다. 설치 Python/Git을 찾지 않으며 자동으로 선택한 localhost 포트를 사용하고 게임 종료 시 해당 프로세스를 종료합니다. 원본 작업 저장소를 수정하지 않습니다.

[이전 v0.5.0 배포 기록](docs/releases/v0.5.0.md)은 당시의 별도 검증 기록입니다.

[전체 기능 명세와 완료 기준](docs/simulator-feature-plan.md), [개발 순서·멀티플레이/피드백 계획](docs/development-roadmap.md), [식별 띠 장착 조사](docs/striker-tag-mounting.md)를 보존했습니다. 목록 등록은 기능 구현 완료를 뜻하지 않습니다. [이전 v0.5 상세 실행 안내](docs/legacy-v0.5-run-guide.md)는 별도 보관본입니다.

[28개 분야·326개 기능 명세](docs/simulator-feature-plan.md)와 [자체 피드백·정밀 실행 계획](docs/simulator-plan-review.md)을 추가 보완했습니다. 판정·시간·입력·네트워크의 개선 항목과 수치 검수 초안·인수 시나리오20개를 포함합니다. 계획 등록은 구현·시험 완료를 뜻하지 않습니다.

![v0.6 기체의 실제 Unreal 렌더](docs/images/airframe-v06.png)
