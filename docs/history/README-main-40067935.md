# 이전 GitHub 첫 화면 보존본

기본 브랜치 40067935의 README와 당시 링크를 보존합니다. 현재 실행본은 [최신 README](../../README.md)를 확인하세요.

# Imperial Wings Drone Soccer Simulator

Windows에서 드론축구 조종을 연습하는 프로그램입니다. 기본 화면은 **경기장 밖에서 드론을 보며 조종하는 시점(LOS)**입니다.

**새 테스트 버전: v0.7.12-rc1 · 사전 릴리스** — [업데이트 안내](#v0712-update)

기존 **v0.7.4-r2 개발 시험판**도 그대로 받을 수 있습니다. 두 버전 모두 정식 안정판이 아닌 테스트용 배포본입니다. [기존 버전의 변경 내용과 검증 범위](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/releases/v0.7.4.md)

<a id="windows-run"></a>
<a id="download-steps"></a>
## 1. 설치

- **[새 테스트 버전 다운로드 — v0.7.12-rc1](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/download/v0.7.12-rc1/ImperialWings-Windows-v0.7.12-rc1.zip)**: 한국어 조종기 안내와 안전 저장을 확인할 때 선택합니다.
- **[기존 배포 다운로드 — v0.7.4-r2](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/download/v0.7.4/ImperialWings-Windows-v0.7.4-r2.zip)**: 이전 공개 실행본입니다. 새 버전이 이 파일을 덮어쓰지는 않습니다.

필요 환경: **Windows 10/11 64비트**, DirectX 12·Shader Model 6 지원 그래픽 카드. Unreal Engine·Python·Git은 따로 설치하지 않아도 됩니다.

1. 위에서 사용할 버전의 **Windows 실행용 ZIP**을 받습니다. 새 버전의 파일명은 `ImperialWings-Windows-v0.7.12-rc1.zip`, 기존 버전은 `ImperialWings-Windows-v0.7.4-r2.zip`입니다.
2. 다운로드가 끝나면 **이전 실행 폴더와 다른 새 폴더**에 **마우스 오른쪽 → 모두 압축 풀기**로 전체 해제합니다. 이전 폴더에 일부 파일만 덮어쓰지 않습니다.

> `Source code`나 `Code → Download ZIP`은 개발용 소스입니다. 실행하려면 위의 **Windows 실행용 ZIP**을 받으세요. EXE만 따로 꺼내거나 ZIP 안에서 바로 실행하지 마세요.

<a id="run-steps"></a>
## 2. 실행

1. 파일 탐색기에서 압축을 푼 폴더를 엽니다.
2. **`ImperialWingsLauncher.exe`**를 더블클릭합니다.
3. 조종기 설정 창에서 **키보드 입력 선택 → 창 닫기**로 시작합니다. 영어 화면의 버튼은 **USE KEYBOARD → CLOSE**입니다. USB 조종기는 [4. 조종기 연결](#first-controller-setup)을 먼저 진행하세요.

Visual C++ 구성 요소가 없다는 메시지가 나오면 같은 폴더의 `Prerequisites\vc_redist.x64.exe`를 설치하고 다시 실행합니다.

<a id="first-flight"></a>
## 3. 첫 비행

처음에는 **START MATCH를 누르지 말고 Practice(연습)**에서 시작하세요.

1. 오른쪽 목록이나 **Tab**으로 **파란 팀 Striker**를 고릅니다.
2. **키보드:** ↓를 눌러 **THR(모터 출력)을 0%**로 내립니다. **조종기:** 스로틀을 맨 아래로 둡니다.
3. **Space**를 눌러 **ARM(모터 작동 허용)**합니다.
4. **키보드:** ↑를 짧게 눌러 출력을 조금씩 올립니다. **조종기:** 스로틀을 천천히 올립니다.
5. **키보드:** W/S는 앞뒤 기울기, A/D는 좌우 기울기, Q/E는 방향 회전입니다. **조종기:** 보정한 스틱을 조금씩 움직입니다.
6. 출력을 조금씩 내려 착륙한 뒤 **Space**로 **DISARM(모터 작동 해제)**합니다.

**다시 시작:** Backspace는 선택 기체, Home / RESET은 전체 초기화입니다. **종료:** 착륙·DISARM 후 EXIT 또는 창 닫기를 사용합니다. F6 기록 중이면 F6를 다시 눌러 저장을 마친 뒤 종료하세요.

> **꼭 기억하세요**
> - 키보드는 **↑ 키를 놓아도 올린 출력이 유지**됩니다. 낮출 때는 ↓를 누릅니다.
> - ANGLE은 기체를 수평으로 돌려줄 뿐, **높이와 위치를 자동으로 유지하지 않습니다.**
> - 다른 앱으로 창을 전환하면 수동 기체가 DISARM됩니다. 파일 확인은 착륙 후에 하세요.

<a id="first-controller-setup"></a>
<a id="3-usb-조종기-처음-설정하기"></a>
## 4. 조종기 연결

처음에는 실제 스틱을 움직여 **방향·움직임 범위·중립을 보정**해야 합니다.

**v0.7.12-rc1은 처음에 한국어로 안내합니다.** C로 여는 조종기 설정 화면의 제목 오른쪽에서 **English / 한국어**를 바꿀 수 있습니다. 아래 영문 버튼명은 화면 언어에 따라 한국어로 표시될 수 있습니다.

1. 조종기의 **데이터 전송용 USB 포트**에 데이터 케이블을 연결합니다. 필요하면 전원을 켜고 **USB Joystick / 게임 컨트롤러 모드**를 선택합니다.
2. 게임에서 **실제 장치 카드**를 선택하고 **MODE 1 / MODE 2**를 고릅니다.
   - **Mode 1:** 오른쪽 스틱 위아래로 출력 조절
   - **Mode 2:** 왼쪽 스틱 위아래로 출력 조절
3. **STICK SETUP**을 누르고 화면의 **STEP** 안내대로 스틱을 움직입니다.
4. 검토 후 **CONFIRM & SAVE**로 저장합니다.
5. **스로틀 최저·나머지 축 중립**에서 **ENABLE RADIO**를 누릅니다.
6. 설정 창을 닫고 [첫 비행](#first-flight)을 진행합니다. **연결과 ARM은 별도 단계**이므로 Space로 ARM해야 합니다.

**다음 실행부터:** 같은 조종기를 연결하고 최저 스로틀에서 **F8 / CONNECT SAVED RADIO**를 사용합니다. 연결 후에도 Space로 별도 ARM합니다. **비행 중 F8은 누르지 마세요.**

모드를 바꾸면 USB가 해제되고 새 보정이 필요합니다. 다른 장치나 다른 사람의 보정 파일을 그대로 사용하지 마세요. [조종기 상세 안내](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/BOXER_SETUP.md)

## 5. 문제 해결

- **실행 파일이 없어요:** 소스 ZIP이 아닌 Windows 실행용 ZIP을 받았는지, 전체 압축을 풀었는지 확인합니다.
- **조종기가 안 보여요:** **Win+R → `joy.cpl` → Enter**로 Windows의 게임 컨트롤러 목록부터 확인합니다.
- **다른 창을 보고 돌아왔더니 조종이 안 돼요:** USB 조종기라면 최저 스로틀에서 **F8로 다시 연결**하고 **Space로 ARM**합니다. 키보드는 출력을 0으로 내린 뒤 Space를 새로 눌러 ARM합니다.
- **화면이 끊겨요:** 전체 패널의 **GRAPHICS → BALANCED**로 바꿔 비교합니다. 패널은 F10으로 표시할 수 있습니다.

해결되지 않으면 **버전·오류 문구·발생 단계**를 적고 [테스트 결과 보내기](#feedback-form)를 확인하세요. 개인 보정 파일이나 `Saved` 폴더 전체를 공개하지 마세요.

<a id="v0712-update"></a>
## 새 테스트 버전 안내 — v0.7.12-rc1

**2026-10-28 공개한 사전 릴리스입니다.** [릴리스와 전체 변경 내용](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/tag/v0.7.12-rc1)을 확인하세요.

- **한국어 조종기 안내:** 장치 선택·Mode 1/2·스틱 보정·저장·활성화와 오류 후 다음 행동을 안내합니다. 언어 선택은 Windows 사용자별로 저장합니다.
- **저장 실패 시 기존 설정 보호:** 보정·정밀 스로틀·레이트/아밍 설정을 저장하지 못했을 때 기존 파일을 먼저 삭제하지 않도록 개선했습니다.
- **포함된 조종·훈련 기능:** Mode 1/2, 레이트·Expo·아밍 버튼 설정과 AI 연습/3분 대전을 포함합니다. **AI PRACTICE → DUEL / 3 MINUTES**는 한 명이 파란 공격수를 조종하는 로컬 훈련이며, 정식 5대5 멀티플레이가 아닙니다.
- **그대로 유지되는 조종 기준:** 기본 LOS 시점, ANGLE의 최대 목표 기울기 35°, 수동 스로틀, USB 활성화와 ARM의 분리를 유지합니다. 프로필을 불러오거나 케이블을 다시 연결해도 자동 이륙하지 않습니다.

**아직 확인하지 못한 부분:** 실제 Boxer의 연속 비행·아밍 버튼·케이블 분리/재연결, 다른 물리 PC에서의 설치·보정·비행은 미검증입니다. 모든 화면의 한국어 번역은 끝나지 않았고, **Topgun 코스·LAN/온라인 5대5는 이번 배포에 포함하지 않습니다.**

새 PC에서는 실제 장치를 다시 확인하고 보정하세요. 같은 Windows 사용자·같은 조종기의 지원되는 보정은 불러올 수 있지만, 장치가 유일하게 일치하지 않으면 다시 선택·확인해야 합니다. [새 버전 실행·조종기 안내](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/v0.7.12-rc1/docs/PORTABLE_QUICK_START.ko.md) · [검증 기록](#v0712-verification)

## 자세한 사용 안내

필요한 항목만 눌러 펼쳐 보세요. 일반 사용자는 개발 자료까지 읽을 필요가 없습니다. **아래 기존 상세 설명은 v0.7.4-r2를 기준으로 보존했습니다. v0.7.12-rc1의 변경 사항은 위 신버전 안내와 해당 릴리스 자료를 우선 확인하세요.**

<a id="controller"></a>
<details>
<summary>전체 조작키 · 용어 · F8 재연결</summary>

### 자주 나오는 말부터 알아두세요

| 화면의 용어 | 쉬운 뜻 |
|---|---|
| **LOS** | 경기장 밖에서 드론을 눈으로 보며 조종하는 시점 |
| **Throttle / THR / 스로틀** | 모터 출력 조절. 높이를 유지하려면 직접 조절해야 합니다. |
| **ARM / DISARM** | 모터 작동 허용 / 해제. **조종기 연결과 ARM은 별도 단계**입니다. |
| **ANGLE** | 스틱을 놓으면 기울어진 자세를 수평으로 돌려주는 방식. **높이·위치 자동 고정은 아닙니다.** |
| **Roll / Pitch / Yaw** | 좌우 기울기 / 앞뒤 기울기 / 바라보는 방향 회전 |
| **보정 / 프로필** | 실제 스틱의 방향·움직임 범위를 확인하는 과정 / 그 결과를 저장한 설정 |
| **Practice / Match** | 연습 / 경기 |

<a id="controls"></a>
## LOS 조종과 화면

기본 화면은 경기장 밖의 조종자 시점입니다. 카메라는 선택한 팀의 자기 진영 바깥 `x=±9.5m, z=1.7m`에 있습니다. 기체를 선택하면 그 팀·역할에 대응하는 조종석으로 이동합니다.

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
| W / S, A / D, Q / E | 앞뒤 기울기(Pitch), 좌우 기울기(Roll), 방향 회전(Yaw) |
| Backspace / Home | 선택 기체 초기화 / 전체 초기화 |
| C / RADIO USB | USB 장치·축 보정 패널 |
| F10 | 넓은 LOS 화면 / 관제 패널 전환 |
| F5 / EXPORT CSV | 실제 물리 상태를 CSV로 저장 |
| F6 | 실제 비행·입력 기록 시작/종료 |
| F8 | 최저 스로틀에서 일치하는 저장 조종기 연결/해제 |
| GRAPHICS | Balanced/High/Cinematic 화질 선택·사용자별 저장 |

모든 기체의 수동 비행은 **ANGLE 모드(스틱을 놓으면 수평 자세로 복원)**입니다. Roll/Pitch는 최대35°의 목표 기울기를 명령하며 스틱이나 키를 놓으면 수평으로 복원합니다. Yaw는 각속도,고도는 스로틀로 직접 조절합니다. 기본 조종 시점은 경기장 밖 조종자의 육안 위치에 고정합니다.

<a id="boxer"></a>
### 조종기 설정·저장 프로필·F8 재연결

조종기는 실제 장치 선택 후 축 배정·최대/최소·중립·반전·최저 스로틀을 확인합니다. 기본값은 키보드이며 미보정 USB 입력으로 ARM하지 않습니다. 개발 PC에서 사용한 조종기의 실제4축 보정·저장·USB 활성화·최저 스로틀 ARM은 확인했고 연속 USB 비행·케이블 분리 복구·입력 지연 검증은 남아 있습니다. 자기 PC의 장치는 직접 확인합니다. [USB 조종기 설정](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/BOXER_SETUP.md)을 참고합니다.

**MODE 1 / MODE 2**를 선택한 뒤 **STICK SETUP**에서 각 축을 한 단계씩 실제로 움직여 축과 방향을 찾는 안내 화면입니다. 각 단계에서 Enter로 시작·끝값 확인을 할 수 있고,중립·최저 스로틀 확인 후 검토 화면에서 따로 저장합니다. 설정 중에는 비행 단축키가 차단되며 취소하면 이전 프로필이 유지됩니다. 안내 화면이 구현되어 있어도 모든 실제 조종기의 보정·비행을 검증했다는 뜻은 아닙니다.

v0.6.1의 기본 화면은 **장치 선택 → 안내 보정 → 검토/저장 → 활성화**를 상단 STEP으로 표시합니다. 수동 축 배정·반전·데드존·범위 수집·프로필 저장/불러오기는 **ADVANCED OPTIONS**에 있습니다. **SIMPLE VIEW**로 돌아올 수 있으며, 설정 창을 다시 열면 간소 화면에서 시작합니다. 이미 보정한 장치는 **CONNECT SAVED RADIO**를 누릅니다. 연결과 ARM은 별도입니다.

![실제 실행 파일의 간소 조종기 화면](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/images/controller-simple-v061.png)

### 다음 실행에서 F8로 연결하기

1. 처음 한 번은 실제 장치를 선택해 보정하고 **CONFIRM & SAVE**를 마칩니다.
2. 다음 실행에는 조종기의 USB Joystick 연결과 최저 스로틀을 확인합니다.
3. 보정창을 닫고 **F8**을 누릅니다. 현재 연결 장치 중 저장 프로필과 일치하는 장치가 정확히 하나여야 연결됩니다.
4. 실패하면 **C**를 열어 장치를 명시적으로 선택합니다. 슬롯·펌웨어·모델 출력이 바뀌었다면 실제 축을 다시 확인합니다.
5. USB 활성화 후에도 기체는 DISARM입니다. 창을 닫고 **Space**로 별도 ARM합니다.

USB가 이미 활성화된 상태에서 F8을 누르면 USB를 해제합니다. **F8은 선택 기체를 MANUAL/DISARM으로 전환하므로 비행 중 재연결 용도로 누르지 않습니다.** 저장된 값이 있어도 자동 이륙하지 않습니다.

</details>

<details>
<summary>화질 · 정밀 스로틀 · 경기와 연습</summary>

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
- 현재 흰색 전방/빨간색 후방 표시는 훈련 설계입니다. 공식 역할색 조건과 사용자 엄격 태그 정책의 구분은 [정밀 계획](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/simulator-plan-review.md)을 확인합니다.

<a id="manual-throttle"></a>
## 선택 사항: 정밀 스로틀 — v0.7.4

**작은 스틱 움직임에도 출력이 너무 크게 바뀐다고 느낄 때** 사용하는 선택 사항입니다. 새 사용자 기본값은 **THROTTLE: DIRECT**이며, 키보드 수동 스로틀에도 같은 선택이 적용됩니다.

1. 모든 수동 기체를 **착륙·DISARM**합니다.
2. 조종기 설정의 **THROTTLE: DIRECT**를 눌러 **THROTTLE: PRECISION**으로 바꿉니다. 간소·고급 화면 모두에서 선택할 수 있습니다.
3. 저장 후 USB가 해제되므로, **최저 스로틀에서 ENABLE RADIO / F8로 다시 연결**합니다.

보정 중이거나 수동 기체가 ARM 상태일 때는 변경할 수 없습니다. **자동 고도 유지 기능은 아닙니다.**

Precision은 **입력 55% 부근의 변화량을 줄이는 곡선**입니다. 최저 0%·최대 100%·입력 55%의 출력은 유지하고, 중심에서 입력 변화에 대한 출력 변화율은 0.65입니다. 55%는 모든 기체·배터리에서 실제로 호버하는 지점을 뜻하지 않습니다. **ANGLE은 자세를 수평으로 복원하며 고도는 계속 스로틀로 조절합니다.** Precision에도 자동 고도 유지가 없고 자율비행 스로틀에는 이 곡선을 적용하지 않습니다.

조종기 보정과 USB 활성화·ARM의 최저 스로틀 판정은 **정밀 곡선 적용 전 입력**을 사용합니다. 이 옵션은 모터 자체의 추력 곡선을 바꾸지 않습니다. 설정은 사용자 저장 폴더의 `Config/PilotControls.json`에 별도로 저장하며 `RadioProfile.json`의 실제 축·범위·중립을 변경하지 않습니다. 저장에 실패하면 이전 선택을 유지하고, 설정 파일이 잘못됐거나 지원하지 않는 버전이면 다음 시작에서 DIRECT를 사용합니다.

**다른 앱으로 창을 전환하면 수동 기체는 DISARM됩니다.** USB는 게임으로 돌아온 뒤 최저 스로틀에서 F8로 다시 연결하고 ARM합니다. 상세 안내는 [다른 PC에서 실행·조종기 설정](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/PORTABLE_QUICK_START.ko.md)을 확인합니다.

<a id="match"></a>
## 경기와 자율 훈련

**Practice는 자유 연습, 정규 경기는 수동 조종으로 진행하는 경기입니다.** 자동비행은 연습 보조이며 경기를 시작해도 자동으로 ARM하거나 이륙하지 않습니다.

기본 규정 프로필은 [FIDA Class40 제5차 개정, 2026-01-01 시행](https://fidasystem.com/rules.html)입니다. 권장 경기장 **16×7×4.5m**, 보호구 외경 **40cm**, 골대 내경 **60cm**이며 골대는 양 끝에서 1.5m 안쪽, 중심 높이 3.0m에 배치합니다. 현재 기체 질량 0.9kg은 규정 상한 1.1kg 이내의 시뮬레이터 설정값입니다. STRIKER V3의 실측 질량으로 주장하지 않습니다.

팀당 5명 중 스트라이커만 득점합니다. 기체 전체가 상대 골대를 앞에서 뒤로 통과해야 하며 팀 오프사이드(다시 득점하기 전에 팀이 자기 진영으로 돌아와야 하는 상태)는 활성 기체 전원이 자기 진영으로 복귀해야 해제됩니다. 경기 코어는 10초 준비, 180초 세트, 300초 정비와 최대 3세트·2선승을 관리합니다. 준비·정비·종료 단계의 ARM은 제한합니다. 경기를 시작하는 동작이 자동 이륙을 뜻하지 않습니다.

**공식 경기 모드는 수동 조종**, SDACS 자동 비행은 **Practice의 훈련 보조**로 구분합니다. AUTO FLEET/Enter는 자율 훈련, HOLD/H는 현 위치 유지, RETURN/R은 홈 패드 귀환·착륙, MANUAL은 선택 기체를 수동으로 전환하고 DISARM합니다. 완전한 인간 심판 판정, 공격수 승계, 실제 페널티 경기·연장·승부치기·VAR은 구현 범위 밖입니다. 규격과 구현 한계는 [규정 근거](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/rules-sources-2026.md)에 정리했습니다.

v0.7부터 정규 경기의 AUTO/HOLD/RETURN·직접 자율 API를 차단하고 세트 종료 시 수동 착륙 조종을 유지합니다. [v0.7 검증 기록](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/releases/v0.7.0.md)을 참고하세요. 페널티·역할 승계·전체 심판 기능 등 [계획서의 후속 항목](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/simulator-plan-review.md)은 남아 있어 완전한 공식 경기 운영판으로 사용하지 않습니다.

</details>

<details>
<summary>설치 파일 확인 · 업데이트 · 실행 오류</summary>

### 설치 파일 선택과 폴더 확인

[GitHub Releases](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases)의 **Assets(첨부 파일 목록)**에서 실행용 ZIP을 선택합니다. 개발 시험판에는 `Pre-release`가 표시되므로 `Latest` 표시만 찾지 말고 버전과 검증 기록을 확인하세요.

- **실행용:** `ImperialWings-Windows-v0.7.4-r2.zip` — **374,786,305바이트**
- **개발용:** `Source code (zip)` / `Source code (tar.gz)` / `…-source.zip` / `Code → Download ZIP`

다운로드 목록의 **폴더에서 보기**나 파일 탐색기의 **다운로드** 폴더에서 ZIP을 찾습니다. 예를 들어 `C:\Games\ImperialWings-v0.7.4`에 전체 압축을 풉니다. ZIP 이름의 하위 폴더가 한 번 더 있으면 그 안으로 들어갑니다.

실행기 옆에 **`Windows`·`Guidance`·`Prerequisites`** 폴더와 `START_HERE.ko.md`가 있는지 확인합니다. 폴더 구성을 유지하고, 기본 실행은 바깥쪽 `ImperialWingsLauncher.exe`를 사용합니다. 브라우저 주소창에 파일 경로를 붙이는 방식으로 실행하지 않습니다.

### 조종기 설정에서 더 알아둘 점

Mode 1·Mode 2 선택은 **v0.7.3부터** 지원합니다. 모드를 바꾸면 USB가 해제되고 새 보정을 저장하기 전에는 이전 모드의 축으로 활성화하지 않습니다. 스로틀·기체 선택·입력 방식·경기 단계에 따라 ARM이 제한될 수 있습니다.

| 스틱 배치 | 왼쪽 좌우 | 왼쪽 위아래 | 오른쪽 좌우 | 오른쪽 위아래 |
|---|---|---|---|---|
| Mode 1 | Yaw | Pitch | Roll | Throttle |
| Mode 2 | Yaw | Throttle | Roll | Pitch |

보정값은 각 Windows 사용자의 `%LOCALAPPDATA%\ImperialWingsSimulator\Saved`에 저장됩니다. 다른 사람의 보정값을 배포하지 않습니다. 저장된 장치 연결의 자세한 조건은 [F8 재연결](#controller)을 확인하세요.

<a id="run-troubleshooting"></a>
## 업데이트와 실행·연결 문제 해결

### 새 버전으로 업데이트하기

새 버전 ZIP을 별도 폴더에 전체 압축 해제합니다. 이전 실행본에 일부 파일만 덮어쓰지 않습니다. 사용자 보정은 `%LOCALAPPDATA%\ImperialWingsSimulator\Saved\Config\RadioProfile.json`, 정밀 스로틀 선택은 같은 폴더의 `PilotControls.json`에 따로 저장합니다. 같은 Windows 사용자·같은 조종기의 설정은 새 버전에서도 불러올 수 있으며 USB 활성화·ARM은 자동 복원하지 않습니다. 이전 버전은 새 버전을 확인한 뒤 정리합니다.

### 실행이 안 되거나 조종기가 보이지 않을 때

- 탐색기에 `file:///E:/...exe` 또는 `%20`이 들어간 경로를 붙여 넣어 “찾을 수 없습니다”가 나오면, 브라우저 주소를 실행한 것입니다. 파일 탐색기로 **실제 압축 해제 폴더**를 연 뒤 `ImperialWingsLauncher.exe`를 더블클릭합니다. 내려받은 PC에는 개발 PC의 `E:\...` 경로가 없습니다.
- 실행 파일이 없으면 `Source code`를 받았는지, 아직 ZIP 내부를 보고 있는지 확인합니다. 이름에 `ImperialWings-Windows-`가 포함된 실행용 ZIP을 다시 확인합니다.
- 파일 손상/누락은 ZIP을 다시 내려받고 새 폴더에 전체 압축을 풀어 확인합니다. GitHub 릴리스의 `SHA256SUMS.txt`로 다운로드 무결성을 확인할 수 있습니다. 확인 예: PowerShell에서 `Get-FileHash -Algorithm SHA256 -LiteralPath 'C:\Users\본인\Downloads\ImperialWings-Windows-v0.7.4-r2.zip'`을 실행하고 같은 파일명의 해시와 비교합니다. 버전·사용자 이름·저장 위치는 실제 내려받은 파일에 맞춥니다.
- 조종기가 보이지 않으면 Windows에서 **Win+R → `joy.cpl` → Enter**로 게임 컨트롤러 목록을 먼저 확인합니다. 게임의 **EXPORT DEVICE REPORT**는 진단용이며 자동으로 보정을 완료하거나 실제 스틱 입력을 만들어 주지 않습니다.
- 실행 후 문제가 있으면 **다운로드한 버전·오류 문구·발생 단계**와 사용자 저장 폴더의 진단 파일을 함께 확인합니다. 개인 보정 파일과 전체 사용자 폴더는 공개 저장소에 올리지 않습니다.

</details>

<a id="feedback-form"></a>
<details>
<summary>테스트 결과 보내기 · 비행 기록 · 저장 파일</summary>

## 테스트 결과 보내기 — v0.7.4 배포 양식

1. ZIP 전체를 압축 해제한 폴더에서 실행기 옆의 **`feedback-form.html`**을 Edge 또는 Chrome으로 엽니다. 인터넷이나 로그인이 필요하지 않습니다.
2. 실제 실행한 **버전·ZIP 이름·조종기 배치·입력 방식**과 직접 확인한 증상, 재현 순서, 기대한 동작·실제 동작을 적습니다. 정밀 스로틀 문제라면 DIRECT/PRECISION 선택도 기록합니다.
3. **보고서 미리보기 → 내용 확인란 → TXT 보고서 저장 / JSON 보고서 저장** 순서로 저장합니다. 브라우저 다운로드 목록에서 파일이 생겼는지 확인합니다. 내용을 수정했다면 미리보기를 다시 확인하고 창 닫기·새로고침 전에 저장합니다.
4. 저장한 보고서와 필요한 화면·F6 비행 기록을 배포자에게 직접 전달합니다. 양식은 자동 전송하지 않으며 첨부란은 파일 이름·설명 메모입니다. 파일을 자동으로 읽거나 보고서에 포함하지 않습니다. 개인 `RadioProfile.json`이나 사용자 `Saved` 폴더 전체는 보내지 않습니다.

HTML은 게임·조종기 설정을 변경하지 않습니다. 양식을 열거나 저장할 수 없으면 같은 내용을 텍스트로 작성해 전달합니다. 검토한 [양식 원본](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/feedback-form.html)과 배포 ZIP의 HTML·체크섬이 일치함을 확인했습니다. 실제 브라우저의 파일 연결 방식은 PC 환경에 따라 다릅니다.

<a id="data-collection"></a>
## 시뮬레이터 데이터 수집 방법 — v0.6

**v0.6부터 제공한 기록 기능이며, 아래 설명에는 v0.7.4에서 추가된 항목도 포함합니다.**

**문제를 다시 확인하거나 개발자에게 설명할 때 쓰는 선택 기능입니다.** 처음 비행할 때 반드시 켤 필요는 없습니다. 현재 수집 기능은 아래 세 종류입니다. 기록은 이 PC에 저장하며 GitHub·개발자에게 자동 전송하지 않습니다.

| 목적 | 조작 | 파일·폴더 | 무엇을 확인할 수 있나 |
|---|---|---|---|
| 시간에 따른 비행·입력 변화 | **F6 / RECORD FLIGHT** 시작, 다시 눌러 종료 | `FlightLogs/*.jsonl` | 시각·선택 기체·USB 상태와 10대의 위치·속도·자세·스로틀 |
| 그 순간의 상세 상태 | **F5 / EXPORT CSV** | `Reports/*-fleet.csv` | 10대의 입력/비행 모드·배터리·빌드·배치·보정 상태 |
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
│  ├─ PilotControls.json       # v0.7.4 정밀 스로틀 선택
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

**현재 F6 기록에 없는 항목:** 연속 배터리 전압·개별 모터 힘/RPM·자이로·PID 각 항·키보드 누른 키 전체·확정 득점/심판 사건·영상. 배터리 잔량·전압과 보정 상태는 F5로 순간 확인할 수 있습니다. v0.7.4의 스로틀 명령·전력 제한 항목은 이전 버전의 기록에 없을 수 있으므로 실제 기록의 버전·필드부터 확인합니다. 더 풍부한 진단·경기 사건 기록은 [개발 계획](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/development-roadmap.md#feedback-data-plan)에 있습니다.

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

</details>

## 현재 상태와 개발 자료

- **현재:** 키보드·보정된 USB 비행, Mode 1/2, 연습, 기본 5대5 경기 흐름, 비행 수치 기록을 제공합니다. 자동비행·위치 유지·귀환은 Practice 전용이고 정규 경기는 수동 조종입니다.
- **새 테스트 버전에 반영:** 조종기 중심의 한국어/영어 안내와 안전 저장입니다. 범위는 [v0.7.12-rc1 안내](#v0712-update)를 따릅니다.
- **계속 개발할 항목:** 나머지 화면의 한국어 안내, 더 쉬운 설정, 입문 코스, 완성된 리플레이, 멀티플레이, 전체 심판 기능 등입니다. 계획 목록은 구현 완료를 뜻하지 않습니다.
- **한계:** 모든 USB 조종기·PC의 호환성이나 실기체와 같은 조종감을 보장하지 않습니다. 실물 조종기·다른 PC·연속 USB 비행과 케이블 복구 검증은 남아 있습니다.

<details>
<summary>현재 구현과 물리 모델의 상세 범위</summary>

<a id="implementation-details"></a>
## 현재 기능의 상세 범위와 한계

모델·입력·충돌이 어떻게 동작하는지 더 알고 싶을 때 읽는 설명입니다. 공개 실행본과 개발 중인 소스·로컬 시험본은 구분합니다.

Unreal Engine 5.8 기반 Windows 드론축구 시뮬레이터입니다. 기본 시점은 **경기장 밖 조종자 위치에서 기체를 보는 LOS(육안) 조종**입니다. FIDA Class40의 경기장·기체 규격과 5대5 경기 흐름, USB 조종기 보정 UI, 실제 SDACS APF를 사용하는 자율 훈련을 함께 개발합니다.

기체는 네 모터의 개별 힘과 회전 모멘트, 중력·항력·Chaos 접촉으로 움직입니다. 정상 비행에서 위치를 강제로 갱신하지 않으며 초기화에서만 물리 위치를 되돌립니다. 실기체와 일치하도록 실측 보정된 제품은 아닙니다.

### 배터리가 부족하면 출력도 줄어듭니다 — v0.7.4

모터가 요구한 전력과 배터리에서 공급 가능한 전력을 연결해, 공급이 부족하면 실제 모터 추력·반작용 토크를 함께 낮춥니다. 항전 장비의 전력도 반영하고 빈 배터리에서는 모터 전력을 공급하지 않습니다. 화면의 **POWER LIMITED / THRUST %** 또는 **BATTERY EMPTY / NO MOTOR POWER**가 상태를 알립니다. 이는 실험용 모델이며 실제 기체의 배터리·ESC·추력 실측 보정을 완료한 것은 아닙니다. 정밀 스로틀과 전력 제한은 서로 다른 기능입니다.

### 내부 장비 보기와 접촉 표현

![v0.7 내부 기자재의 실제 Unreal 렌더](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/images/equipment-v07.png)

v0.7은 FC/ESC·수신기·모터 권선·배터리·커넥터·배선과 비틀린 프로펠러를 자체 메시로 추가했습니다. DISARM 후 **F9 EQUIPMENT**로 보호구를 숨겨 내부를 확인하고 다시 F9로 LOS에 돌아옵니다. 장비 확인 중에는 ARM하지 않습니다. 제품 CAD나 실측 디지털 트윈은 아닙니다.

**보기 분리:** 1 파일럿 LOS, 2 기체 추적 관찰자, 3 경기장 전체, 4 관객석, 5 심판 측면. 우클릭 드래그·휠은 해당 보기의 시선·확대에 적용합니다. 관객·심판 보기는 비행 조작을 제한하며, 수동 비행 중 진입하려면 먼저 DISARM합니다. 관측 카메라를 바꿔도 자율 훈련의 물리 계산은 계속됩니다.

**접촉 반응:** 충격량으로 가드 상태를 낮추고 마모 흔적·작은 파편 효과·이벤트·GUARD %를 표시합니다. 강체 충돌 형상은 유지하며 실제 부품 파단·모터 고장을 뜻하지 않습니다. Backspace/전체 RESET은 상태를 복원합니다. 골대는 규정 범위의12.5kg이며 연습에서 작은 앞뒤 충격 반응, 정규 경기에서 고정 지지를 사용합니다. 계수는 실험값입니다.

**경기/연습 구분:** AUTO·HOLD·RETURN과 기체 API의 자율 활성화는 Practice에서만 허용합니다. 정규 경기 종료는 자동 귀환을 시작하지 않으며 이미 비행 중인 수동 기체의 착륙 조종을 유지합니다. 정비·종료 단계의 새 ARM은 차단합니다.

### 창 전환·메뉴 이동 때의 입력 처리 — v0.7.1

다른 앱으로 전환해 게임 창이 입력을 받지 못하면 수동 기체를 DISARM하고 누적 키보드 스로틀·축 입력과 USB 활성화를 해제합니다. 복귀 후 키 자동 반복으로 다시 움직이거나 ARM하지 않으며 새 키 누름/ARM 클릭이 필요합니다. USB 조종은 실제 최저 스로틀에서 F8로 다시 연결합니다. 메뉴·장비 보기·기체 전환·초기화에서도 남은 키보드 입력을 제거합니다. 메뉴에서 직접 활성화한 USB 연결은 메뉴를 닫아도 유지하며 원시 축·보정값은 바꾸지 않습니다. 명시적으로 시작한 연습 자율비행은 포커스 전환에도 계속됩니다.

[v0.7.1 입력 시험과 범위](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/releases/v0.7.1.md)에서 엔진 내부 키 이벤트 시험과 실제 키보드/USB 현장 시험을 구분합니다.

### 느려진 프레임과 자율 착륙 처리 — v0.7.2

긴 프레임에서도 모터·배터리·센서·네트 힘 계산이 Chaos 장면의 같은 시간 예산을 사용합니다. 100/250ms 멈춤에서 생략된 물리 시간은 HUD·F6 기록에 표시하며 경기 시계는 게임 시간으로 계속 갑니다. 실제 upstream APF의 착륙 직전 정지 문제도 보완해10대 출발·통신 장애·복구·귀환·착륙을 다시 검사했습니다. [시간 계약](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/time-contract.md) · [최종 시험·한계](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/releases/v0.7.2.md).

[중단·미완료 작업 목록과 재개 지점](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/work-status-2026-10-27.md)에서 현장 조종기 시험·다른 PC·고정주기 제어와 계획서 후속 기능을 확인합니다. 이번 릴리스는 전체326개 기능의 완료를 의미하지 않습니다.


[v0.7 변경·시험 결과](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/releases/v0.7.0.md) · [실제 기능과 미완료 항목](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/feature-inventory-2026-10-26.md) · [자율 시험에서 찾은 문제와 수정](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/autonomy-feedback-v07.md)

[다른 시뮬레이터 기능 조사](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/simulator-benchmark.md) · [사용자 제공 경기 영상과 물리 대조 계획](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/video-physics-reference.md)

## 기체 모델과 실험 물리

**겉모습과 물리 계산을 참고용으로 구현한 모델이며, 실제 제품을 정확히 복제·실측한 모델은 아닙니다.**

외형은 제조사 자료를 참고해 직접 작성한 **STRIKER V3 참고형 절차적 모델**입니다. 흰색40cm·42개 셀(오각형12/육각형30)의 자체 보호구와 둥근 접합부, 검은 X 프레임·적도 링, 끝이 둥근3엽 프로펠러, 모터 냉각 슬롯·하부 배터리·전원 배선·클램프·체결부를 표현합니다. 제조사 메시·사진·설명서·로고를 복제한 공식 제품 모델이 아니며 제작 치수와 비행 특성을 정확히 재현했다고 주장하지 않습니다. [공식 자료와 관찰·가정 구분](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/model-reference.md)을 참고합니다.

고급 물리 확장은 실험 단계입니다. 엔진 독립 모델과 Unreal 통합에서 다루는 항목은 다음과 같습니다.

- 비선형 PWM-추력, 모터 응답·포화, 회전자 동력 추정
- 배터리 소모·내부저항에 따른 전압 강하·출력 제한
- 부품 질량 배분에 따른 무게중심·관성 텐서, 회전자 자이로 토크
- 상대 풍속 기반 항력, 하향류, 지면·천장·벽 근접 보정
- IMU 바이어스·잡음·지연과 PID 적분 포화 방지
- 제한된 스프링·댐퍼 접촉 및 네트·보호구 변형 근사

계수는 조정 가능한 가정값입니다. CFD, 모터 벤치 측정, 실기체 IMU·추력·충돌 측정을 대체하지 않습니다. 각 모델의 존재와 실제 게임에서 검증된 통합 범위는 구별하며 최신 결과는 [검증 기록](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/verification.md)에 남깁니다.

</details>

<details>
<summary>2026-10-27 개발 계획 — 당시 기록과 남은 항목</summary>

<a id="development-plan"></a>
## 현재 상태와 개발 우선순위 — 2026-10-27 갱신

> **아래는 당시 계획과 재개 예정 안내를 보존한 기록입니다.** 이후 공개된 v0.7.12-rc1의 조종기 한국어 안내·설정·저장 범위는 [신버전 안내](#v0712-update)를 따릅니다. 아래의 “현재”와 “예정”을 오늘의 전체 상태로 해석하지 않습니다.

**당시 공개 실행본은 v0.7.4-r2 개발 시험판입니다.** 아래 개선 계획은 공개 실행본에 모두 구현됐다는 뜻이 아닙니다. 다운로드·검증 범위는 [릴리스](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/tag/v0.7.4)와 [버전별 기록](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/releases/v0.7.4.md)을 기준으로 확인합니다. 개발 중인 소스·로컬 시험본과 공개 배포본은 구분합니다.

### 개발 재개 예정

**개발은 2026년 11월 3일 오후 3시 38분(15:38, 한국시간 KST·UTC+9)에 이어서 진행할 예정입니다.** 실제 재개는 사용량이 이용 가능한 상태이고 개발 컴퓨터가 연결되어 있는 경우에 진행합니다. 이는 예정 안내이며, 개발이 이미 재개되었다는 뜻은 아닙니다.

### 최우선: 한국어 안내와 쉬운 조종기 설정

기본 방향은 **경기장 밖에서 기체를 보는 LOS 드론축구 연습**입니다. 기존 Mode 1·Mode 2, 안내 보정, 프로필 저장 기능을 유지하면서 처음 사용하는 사람이 설정을 끝낼 수 있도록 개선합니다.

- **한국어 지원:** 메뉴·설정·오류 및 복구 안내의 용어를 통일하고, 한국어 글꼴과 화면 배율별 표시를 검증합니다. 한국어/영어 전환도 후속 검토합니다.
- **간편 설정 목표(아직 개발 계획):** 조종기 연결 → 그림과 안내대로 스틱 움직이기 → 아밍 스위치 지정 → 입력 확인 후 비행. 기본 화면에 필요한 행동만 보여주고 축 번호·수동 보정·레이트·Expo는 고급 설정으로 분리하는 방향입니다.
- **안전과 기존 설정 보존:** 보정값을 유지하고, 높은 스로틀·메뉴 복귀·USB 재연결만으로 자동 ARM되지 않도록 검증합니다. 연결과 ARM은 별도 단계입니다.

위 항목은 **개선 우선순위**입니다. 특히 아밍 스위치 지정·레이트·Expo를 현재 배포본에서 모두 설정할 수 있다는 뜻은 아닙니다. 현재 배포본의 영어 버튼명과 실제 설정 절차는 [다운로드·실행 안내](#windows-run)를 따릅니다. 모든 USB 조종기의 호환성이나 실물 비행 검증 완료를 보장하지 않습니다.

### 이후 검토할 기능과 화면 흐름

다음은 **후보 기능**입니다. 실제로 사용할 수 있는 기능 목록과 구분합니다.

- **연결·설정:** 연결/입력/아밍 상태 안내, USB 분리·재연결 복구, 조종기별 설정 저장·복구.
- **입문·훈련:** 입문 비행 코스, LOS 시야 보조, 빠른 재시작, 코스·충돌 회복·팀 훈련, 검증된 기체 프리셋, 선택형 조종 보조.
- **복기·경기:** 스틱 입력 포함 리플레이, 훈련 기록, 경기 알림·규칙 표시, 단계별 연습 상대 AI, 멀티플레이.

기존 기능과 겹치는 항목은 새 구현으로 중복 계산하지 않습니다.

화면 구성은 **시작/메인 메뉴 → 플레이 선택 → 싱글플레이**, 조종기 설정·환경설정·종료와 플레이 중 일시정지·재시작·메인 복귀 흐름을 검토합니다. 멀티플레이는 별도 후속 개발이며, 미구현 메뉴를 제공할 경우 **준비 중**임을 명확히 표시합니다.

### 품질 점검과 완료 기준

- **골대 충돌 반응:** 충돌 후 움직임이 둔하게 느껴진다는 피드백을 검토합니다. 연습 모드의 반응 가시성, 실험 물리의 정확도, 정규 경기의 고정 지지 정책을 구분하며 임의로 고정을 해제하지 않습니다.
- **원거리 철망 표시:** 거리·시점·화질별로 겹침이나 이상 무늬를 재현하고 메시·재질·LOD·앨리어싱 등을 점검합니다. 원인과 수정 효과는 아직 확정하지 않았습니다.
- **주요 변경마다 회귀 검사:** 관련 자동 검사와 Windows 빌드, 한국어 표시, 설정·메뉴 이동, 저장/복구를 확인합니다. 실물 조종기의 연결·보정·아밍·이륙·호버·착륙·DISARM 및 USB 재연결은 별도 현장 검증으로 구분합니다.
- 결과는 **통과 / 실패 / 미검증**과 실행 버전·재현 조건을 함께 기록합니다. 계획 등록이나 자동 검사 통과만으로 실제 조종성·전체 기능 완료를 주장하지 않습니다.

</details>

<details>
<summary>개발자용 — SDACS 연동과 소스 빌드</summary>

<a id="sdacs"></a>
## 실제 SDACS 연동

**SDACS는 Practice의 자동비행을 돕는 외부 유도 프로그램입니다.** 아래는 개발·연동 확인용 설명이며 키보드 첫 비행을 위해 직접 설정할 필요는 없습니다.

[기반 저장소](https://github.com/sun475300-sudo/swarm-drone-atc)의 실제 `simulation/apf_engine/apf.py`를 외부 작업 사본에서 import합니다. Unreal이 위치·속도를 미터 단위로 10Hz 전송하고 upstream `batch_compute_forces`·`force_to_velocity`가 짧은 목표점 생성에 참여합니다. Unreal이 물리 상태의 원본이며 브리지는 합성 기체 위치를 보내지 않습니다.

현재 확인한 upstream 커밋은 `1e9d826265069696d7e415c7778d17483d7a8580`입니다. 개발 실행의 `http://127.0.0.1:8767/health`에 실제 origin·커밋·로드한 파일·SHA-256이 표시됩니다. 실행 중 데이터 연결은 localhost이며 외부 통신은 소스 복제·의존성 설치에 사용합니다. 연결이 끊기면 자율 임무를 로컬 위치 유지로 전환합니다. [연동 계약](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/sdacs-integration.md)과 [외부 권리 고지](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/THIRD_PARTY_NOTICES.md)를 참고합니다.

<a id="source-build"></a>

## 개발 소스 빌드 환경과 설치

**이 절은 소스코드를 직접 수정·빌드할 개발자용입니다. 실행용 ZIP을 받은 일반 사용자는 건너뛰세요.**

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

</details>

## 개발 기록

현재 공개 버전과 과거 버전의 기록을 구분합니다. 아래 수치는 각 당시의 검사 결과이며, 이번 사용설명서 정리가 새로운 비행·성능 검증을 뜻하지는 않습니다.

<a id="v0712-verification"></a>
<details>
<summary>v0.7.12-rc1 — 2026-10-28 공개·검증 기록</summary>

아래는 [공개 릴리스](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/tag/v0.7.12-rc1)와 [소프트웨어 검사 요약](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/v0.7.12-rc1/docs/verification/v0.7.12-rc1-summary.json)에 기록된 결과입니다. 이 README 수정 중 프로그램이나 실물 조종기를 새로 시험한 결과는 아닙니다.

- **조종기 보정:** 독립 소프트웨어 검사 205개 통과.
- **레이트·Expo·아밍 버튼:** 독립 소프트웨어 검사 4,881개 통과.
- **한국어/영어·저장 실패·재실행:** Editor와 Windows 실행본 각각 47개 통과. 최종 실행본과 새 경로의 최종 ZIP에서도 언어/안전 저장 47개씩을 확인한 기록이 있습니다.
- **배포 ZIP:** 한글·공백 경로에 전체 해제한 최종 ZIP의 포터블 검사 13개 통과. 같은 PC의 새 경로 시험을 다른 물리 PC 검증으로 계산하지 않습니다.
- **추가 회귀 검사:** Mode 1/2·입력 안전, 응답, AI 대전, 10대 ANGLE을 확인했습니다. 전체 세부 수치와 조건은 릴리스 자료를 따릅니다.
- **미검증:** 실제 마우스의 전체 사용 흐름, 실제 Boxer 연속 비행·아밍 버튼·케이블 복구, 다른 물리 PC. 모든 화면 번역·Topgun·LAN/온라인 5대5·전체 326개 기능 완료를 뜻하지 않습니다.

**배포 식별 정보**

- 공개 태그: `v0.7.12-rc1` · 검증 소스 커밋: `df358d8388243d2b447a441e6f840a1b8c8914a6`
- Windows ZIP: `ImperialWings-Windows-v0.7.12-rc1.zip` · **374,891,896바이트**
- [SHA-256 목록](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/download/v0.7.12-rc1/SHA256SUMS.txt) · [최종 검증 요약](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/download/v0.7.12-rc1/v0.7.12-rc1-summary.json) · [업로드·재다운로드 확인 자료](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/download/v0.7.12-rc1/v0.7.12-rc1-publication.json)
- [해당 태그의 릴리스 문서](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/v0.7.12-rc1/docs/releases/v0.7.12-rc1.md)에는 공개 전 “준비 중” 표기가 남아 있습니다. 최종 공개 여부·파일 해시·추가 검사 결과는 **릴리스 본문과 첨부 자료**를 우선합니다.

이번 main 변경은 **README에 신버전 안내를 추가하는 문서 변경**입니다. 신버전 코드나 실행 파일을 main으로 병합·교체한 작업은 아니며, 소스가 필요하면 위 공개 태그를 기준으로 확인합니다.

</details>

<details>
<summary>v0.7.4 및 이전 버전 — 배포 검증·과거 시험 기록</summary>

<a id="release-verification"></a>
## 배포본의 변경·검증 범위

**v0.7.4-r2는 로컬 실행·새 경로의 배포 ZIP 검사를 통과했습니다.** [릴리스·검증 기록](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/tag/v0.7.4). 전력 제한·선택형 정밀 스로틀·득점 경계 오류 수정·피드백 양식을 포함합니다. ZIP은 374,786,305바이트이며 [해시와 검사 범위](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/releases/v0.7.4.md)를 확인할 수 있습니다.

v0.7.4의 점수 검사는 **101개 규칙 조건**과 시뮬레이터 안에서 움직이는 기체의 골대 통과를 확인했습니다. 중앙선 접촉에서 오프사이드가 먼저 풀리는 오류를 수정했습니다. [득점 검사 상세](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/scoring-test-2026-10-27.md) · [Liftoff·VelociDrone·Uncrashed·DRL 참고 비교](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/simulator-reference-comparison.md).

GitHub에 공개한 실행 ZIP·소스 ZIP을 다시 다운로드해 검증한 원본과 SHA-256이 같음을 확인했습니다. [업로드·다운로드 확인 기록](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/verification/v0.7.4-publication.json).

v0.7.4의 검사 범위는 **[v0.7.4 변경·검증 기록](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/releases/v0.7.4.md)**과 [기계 판독 결과](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/verification/v0.7.4-summary.json)에 있습니다. Mode 1·Mode 2의 이전 근거는 [v0.7.3](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/releases/v0.7.3.md), 시간 계산·자율 착륙 변경은 [v0.7.2](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/releases/v0.7.2.md)를 보존합니다. 미완료 항목은 [순차 작업 목록](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/sequential-work-2026-10-27.md)을 따릅니다. 실제 다운로드 가능 여부는 Releases의 Assets를 기준으로 확인합니다.

[다른 PC에서 실행·조종기 설정](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/PORTABLE_QUICK_START.ko.md)을 확인하세요. Windows 10/11 x64와 DirectX12 / Shader Model6 GPU가 필요합니다. 필수 Visual C++ 설치 파일과 자율 훈련용 독립 실행 환경을 포함합니다. 개발 PC에서 설치 도구 경로를 제거하고 한글·공백 경로로 옮긴 새 사용자 데이터 환경을 검사했으며, 실제 다른 PC 전체에서의 비행을 인증한 것은 아닙니다.

![이전 v0.6.1 Unreal 실행 장면](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/images/simulator-runtime-v061.gif)

실제 Windows 실행 파일의 화면을 녹화한 GIF입니다. 명시적 물리 검사 시나리오의 자동 비행 장면이며 실물 USB 조종기 비행이나 게임 내 리플레이 기능의 증거는 아닙니다. [촬영·버전·검증 기록](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/releases/v0.6.1.md).

<a id="verification"></a>
## 이전 버전의 실행·검증 기록

**아래는 과거 버전의 기록입니다. 기존 배포본은 [v0.7.4 변경·검증 기록](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/releases/v0.7.4.md), 새 테스트 버전은 [v0.7.12-rc1 검증 기록](#v0712-verification)을 확인하세요.**

아래 R4~R6 수치는 당시 기록입니다. 최신 [v0.7.2 시간·착륙·배포 시험](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/releases/v0.7.2.md), [v0.7.1 입력 시험](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/releases/v0.7.1.md), [v0.7 모델·접촉·시점·자율 시험](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/releases/v0.7.0.md), [v0.6.1 기록](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/releases/v0.6.1.md)과 [누적 검증 기록](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/verification.md)을 확인합니다. 수집 절차 문서 추가와 실제 USB 비행·성능 측정 결과를 구분합니다.

![Unreal 실제 경기장 렌더](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/images/arena-r4.png)

2026-10-26 R4의 Windows 에디터 빌드·패키징·실행을 확인했습니다. 실제 키보드/마우스 조작 **96개 항목 모두 통과**: 수동 ARM·상승·높은 스로틀 ARM 거부,USB 미보정 활성화 거부,10대 AUTO/HOLD/RETURN·착륙,세 시점,기체 선택·CSV·초기화 흐름을 검사했습니다. 귀환·전원 DISARM은11.82초였습니다.

고정 게임 계산 간격30/60/120Hz에서 실제 Chaos 호버·수평 이동·바닥/벽·5m/s 네트 충돌·스트라이커 골대 통과와 오프사이드도 모두 통과했습니다. 마지막 목표 오차는0.456/0.284/0.202cm,네트 모델 최대 처짐은5.30/6.72/7.47cm였습니다. 독립 검사는 규정51개·USB 보정159개·물리83개·실제 APF HTTP8개가 통과했습니다. 이 결과가 실기체 물리 정확도나 모든 경기 상황을 인증하는 것은 아닙니다.

1920×1080 경기장 장면의 실제 패키지 렌더 기록은 **평균88.16FPS**,RTX5070Ti입니다. 오프스크린 렌더 중 측정했으며 RTX5060이나 USB 입력 지연의 측정값은 아닙니다. 로그와 이전 중단 기록을 포함한 [검증 문서](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/verification.md)를 참고합니다.

![STRIKER V3 참고형 기체의 실제 렌더](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/images/airframe-r4.png)

역할별 시작 패드·색상·라벨·중앙선 방향 화살표와,흰색 전방 한 줄·빨간색 후방 두 줄 LED를 추가했습니다. 역할 색상은 FIDA 기준이며 방향등과 포지션 좌우 순서는 훈련용 설계 선택입니다. [시작 위치·LED 출처와 설계](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/position-and-led-design.md),[물리 모델별 구현 범위](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/physics-models.md)에 근거와 한계를 정리했습니다.

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
공식 경기 영상 참고 링크와 이번 실행 창에서 확인한 역할별 패드·후미 LED는 [경기 영상 참고·게임 대조 기록](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/match-video-references.md)에 정리했습니다. 자동 재생 제한으로 영상의 장면별 분석은 미완료이며,현재 구현의 규정 근거와 구분했습니다.

R5에서는 Mode 2 안내 보정을 추가하고 새 Windows 패키지에서 **화면 조작119개·보정 수학159개 전부 통과**했습니다. 실제 Boxer를 선택해 라이브 수집·정지 입력 완료 거부·취소 시 프로필 보존을 확인했습니다. 실물의 단계별 스틱 보정과 USB 수동 비행은 남아 있습니다. [실제 보정 화면](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/images/radio-mode2-setup.png)과 [R5 검증 기록](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/verification.md)을 참고합니다.

R6 / v0.4.0에서는 **10대 전부 ANGLE**로 전환했습니다. 실제 Chaos에서20/30/60/120/240FPS 조건의 초기 기울기 복원·스틱 해제 후 수평 복원을 모두 통과했습니다. 이 시험은 명시적 비USB 입력 fixture와 고도 보상을 사용해 자세 제어를 분리하며 실물 조종기 비행 성적으로 주장하지 않습니다. 실제 화면의69개 검사에서10대의 시작점·홈·렌더 패드·초기 방향과 각 LOS 조종자 위치도 확인했습니다.

실물 Boxer의 Roll=X,Pitch=Y,Yaw=V,Throttle=Z 배정과 실제6,454샘플의 끝값·중립·최저 스로틀 보정을 저장했습니다. 미완료 스틱 범위가 마지막 중립에서 거부되던 안내 흐름은 시작 중립의 양쪽 범위를 먼저 확인하도록 수정했고 보정 수학161개가 통과했습니다. USB 활성화·최저 스로틀 ARM까지 실제 확인했으며 연속 USB 비행은 포커스 전환으로 완료하지 못했습니다.

화면·CSV·물리 결과에는 프로젝트 버전,빌드 시각,소스 커밋과 빌드 입력SHA-256을 기록합니다. 실행 스크립트는 현재 빌드 입력과 실행 파일의 불일치를 표시합니다. 검증한 R6 스냅샷은 `01fa67e132697ad826c30bec585ca45242e3c9df97be4347689efd63046237c9`입니다. 빌드 뒤 작업 폴더에 추가된 그래픽 효과 소스는 이 스냅샷의 검사와 구분합니다. [버전별 검증](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/verification.md),[개발 순서](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/development-roadmap.md)를 참고합니다.

포터블 배포 실행기는 실제 MIT APF 소스·검증 해시를 포함한 독립 Windows 런타임을 사용합니다. 설치 Python/Git을 찾지 않으며 자동으로 선택한 localhost 포트를 사용하고 게임 종료 시 해당 프로세스를 종료합니다. 원본 작업 저장소를 수정하지 않습니다.

[이전 v0.5.0 배포 기록](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/releases/v0.5.0.md)은 당시의 별도 검증 기록입니다.

[전체 기능 명세와 완료 기준](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/simulator-feature-plan.md), [개발 순서·멀티플레이/피드백 계획](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/development-roadmap.md), [식별 띠 장착 조사](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/striker-tag-mounting.md)를 보존했습니다. 목록 등록은 기능 구현 완료를 뜻하지 않습니다. [이전 v0.5 상세 실행 안내](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/legacy-v0.5-run-guide.md)는 별도 보관본입니다.

[28개 분야·326개 기능 명세](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/simulator-feature-plan.md)와 [자체 피드백·정밀 실행 계획](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/simulator-plan-review.md)을 추가 보완했습니다. 판정·시간·입력·네트워크의 개선 항목과 수치 검수 초안·인수 시나리오20개를 포함합니다. 계획 등록은 구현·시험 완료를 뜻하지 않습니다.

![v0.6 기체의 실제 Unreal 렌더](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/40067935b6a523240ab993a2e84f9c7578bd9f5b/docs/images/airframe-v06.png)

</details>
