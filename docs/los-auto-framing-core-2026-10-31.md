# LOS 자동 프레이밍 코어 — 2026-10-31
## 목적과 계획 연결
이 문서는 경기장 밖의 고정 파일럿 LOS 시점에 쓸 수 있는 엔진 독립 자동 프레이밍 수학 코어를 기록한다.
새 코어는 `Source/ImperialWingsSimulator/LosAutoFraming.h`의 header-only C++17 코드이며,
파일럿 카메라 위치를 바꾸지 않고 머리 yaw·pitch 오프셋과 FOV만 제안한다.
- 20개 작업 묶음 표의 **6번 LOS 시점** 후속 범위에 해당한다.
- 피드백 표의 **4번**(RealFlight 참고 사람 높이 시점·자동 확대·바닥 보기)에 연결한다.
- 통합 개발 계획의 **D 시점·사용성** 단계 산출물 중 LOS 자동 줌·지면 추적의 순수 계산 하위 단위다.
- `SYS-06`(사용자 설정 저장), `SYS-11`(세션 설정 묶음)과 설정의 저장·표시에 연결한다.
- `UX-02`, `UX-05`, `UX-06`, `UX-07`, `UX-09`, `UX-13`과 HUD 우선순위·설명·앞뒤 식별·가독성·선택 효과·보조 표시를 연결한다.
- `GFX-03`, `GFX-05`, `GFX-11`, `GFX-12`와 LOS에서의 조명·바닥·원거리 식별·대표 장면 검토를 연결한다.
계획서의 파일럿 LOS 행은 “자기 팀 경기장 밖 사람 눈높이”를 유지하고 기체 위치로 카메라를 이동하지
말 것을 요구한다. 이 코어는 그 제약을 코드 경계로 유지한다. `Result`에는 카메라 위치가 없고,
입력의 `PilotEyePositionM`은 읽기 전용이다.
## 현재 LOS 경로와 연결 근거
기존 `ImperialPlayerController.cpp`의 `UpdatePilotCamera()`는
`ImperialSoccerLayout::PilotPosition(Team, Slot)`에 카메라를 놓고, 상대 골대를 향하는 고정
`Facing`에 `PilotHeadYaw`·`PilotHeadPitch`만 더한다. 따라서 이 코어의 yaw·pitch 출력은
월드 회전값이 아니라 그 기존 머리 오프셋이다.
같은 파일의 `ZoomIn()`·`ZoomOut()`는 LOS FOV를 **40~85도**로 제한한다.
`ImperialPlayerController.h`의 기본 `PilotFieldOfView=85`, yaw `-60~60도`, pitch `-25~40도`와도
일치시켰다. 이 값들은 공식 FIDA 규정값이 아니라 기존 구현을 보존하는 시뮬레이터 선택값이다.
`ImperialSoccerLayout.h`의 `PilotPosition()`은 팀 0/1의 X 좌표를 각각 -950/+950cm, 눈높이를
170cm로 둔다. 코어는 SI 입력만 받으므로 통합 시 cm를 m로 변환해야 한다. 다섯 역할의 Y 좌표도
같은 파일의 배치를 그대로 사용해야 하며, 코어가 조종석을 새로 배치하지 않는다.
`SoccerDronePawn.h`는 `GetFlightSpeed()`와 `GetAltitudeMeters()`를 제공한다. 실제 통합에는
`GetActorLocation()`, `GetVelocity()`, `GetActorRotation().Yaw` 등 Unreal 상태를 m·m/s·deg로
명시적으로 변환해 `ImperialLos::Input`을 만들 필요가 있다.
## 상태 기계
상태는 전역이 아닌 호출자 소유 `ImperialLos::State` 한 개에만 둔다.
1. **Disabled**: 기본값이다. `Settings::Enabled=false`이면 입력의 현재 head yaw·pitch·FOV를
   그대로 반환하고 내부 자동 상태를 초기화한다. 기존 수동 LOS 동작을 보존한다.
2. **ManualOverride**: 우클릭 드래그 또는 휠 입력이 있으면 현재 시선 값을 그대로 반환한다.
   `ManualOverrideDurationS` 타이머를 다시 채워 자동 보정을 중단한다.
3. **Return**: 타이머가 0이 될 때까지 현재 값을 그대로 반환한다. 0이 된 tick부터 지수형
   프레임 독립 보간과 yaw/pitch/FOV 속도 상한으로 자동 목표에 부드럽게 돌아간다.
4. **Active**: 기체 방향, 거리, 고도와 바닥을 이용해 목표 head yaw·pitch·FOV를 계산한다.
5. **Rejected**: 비유한값, 시간 간격, 좌표·속도·각도·FOV 범위, 바닥 관계, 너무 가까운 수평 거리,
   잘못된 설정을 `Rejection` 열거값으로 거부한다. 호출자는 거부 출력값을 카메라에 적용하지 않는다.
이 코어는 카메라 액터를 갖지 않고 모터·기체·조종 입력을 명령하지 않는다. 자동 보조를 공식 수동
경기의 조종 대행 또는 실물 조종 증거로 해석하지 않는다.
## 프레이밍 계산
기체 수평 방향은 파일럿 눈에서 기체로 향하는 `atan2`로 구하고, 파일럿의 고정 기본 yaw를 뺀 뒤
기존 머리 yaw 범위로 제한한다. pitch도 고정 기본 pitch를 뺀 머리 오프셋으로 반환한다.
거리 줌은 수평 거리의 선형 목표 FOV를 사용한다. `FovHysteresisDeg`보다 작은 목표 변동은
이전 목표를 유지해 경계 왕복을 줄인다. 실제 출력은 지수 보간과 초당 최대 변화량의 작은 값을
적용하므로 프레임 수가 아니라 경과 시간에 따라 변한다.
저고도 또는 바닥 굴림 상태에서는 기체 각도와 `FloorVisibleMarginM` 높이의 바닥 각도를 함께
계산한다. 필요한 수직 FOV를 수평 FOV로 환산하고, 기체가 화면 하단 여백 아래로 내려가지 않도록
pitch 목표를 아래로 편향한다. 결과에는 기체·바닥 여백의 화면상 고도도 포함해 HUD 또는 검증기가
여백 계약을 확인할 수 있게 한다.
선택형 `DirectionAssistEnabled`는 기체의 yaw와 기체→파일럿 수평 벡터의 내적으로 기수가
파일럿 쪽, 반대쪽, 옆면인지를 반환한다. 기체의 뒤집힘은 `AircraftUpDotWorldUp<0`로 표시하되,
뒤집힌 상태 자체를 오류나 자동 카메라 이동 사유로 만들지 않는다.
## 파라미터와 출처
| 항목 | 값 또는 범위 | 출처·성격 |
|---|---:|---|
| 경기장 권장 크기 | 16×7×4.5m | FIDA Class40 §1-3, `docs/rules-sources-2026.md` |
| 외부 조종구역 | 폭 2m | FIDA Class40 §1-2. 실제 LOS 조종석 배치는 별도 프로젝트 배치다. |
| LOS FOV | 40~85deg | 기존 `ImperialPlayerController.cpp` `ZoomIn/ZoomOut`; 시뮬레이터 선택값 |
| head yaw / pitch | -60~60 / -25~40deg | 기존 `PlayerTick()` clamp; 시뮬레이터 선택값 |
| 근거리 / 원거리 줌 기준 | 3m / 16m | 시뮬레이터 선택값. 16m는 §1-3의 권장 길이와 연결하지만 FOV 규정은 아니다. |
| FOV 히스테리시스 | 1deg | 시뮬레이터 선택값 |
| smoothing 시간 | 0.22s | 시뮬레이터 선택값 |
| yaw / pitch / FOV 상한 | 120 / 90 / 40 deg/s | 시뮬레이터 선택값 |
| 수동 보류 시간 | 0.75s | 시뮬레이터 선택값 |
| 저고도 판정 | 0.75m | 시뮬레이터 선택값 |
| 굴림 판정 | 0.25m 이하, 0.15m/s 이상 또는 명시 상태 | 시뮬레이터 선택값 |
| 바닥 표시 여백 | 0.50m, 화면 가장자리 3deg | 시뮬레이터 선택값 |
| 기본 화면비 | 16:9 | 시뮬레이터 선택값. 실제 창 화면비를 통합 시 입력해야 한다. |
FIDA §1-3은 경기장 권장 크기, §1-2는 외부 조종구역을 뒷받침할 뿐 자동 줌, 사람 눈높이,
카메라 속도, FOV 또는 방향 보조를 규정하지 않는다. 위에서 시뮬레이터 선택값으로 표시한 수치를
공식 규정 또는 실물 측정값으로 주장하지 않는다.
## 네이티브 검사
`Tests/los_auto_framing_tests.cpp`는 다음 범위를 검사한다.
- 기본 OFF에서 수동 head yaw·pitch·FOV가 변경되지 않는지
- 근거리 최소 FOV, 16m 원거리 최대 FOV, 속도 상한과 1도 히스테리시스
- 20/60/240 FPS에서 2초 후 yaw·pitch·FOV가 유사한지
- 저고도·바닥 굴림에서 하단 여백, 뒤집힌 기체의 유한 출력
- 양 팀의 5개 파일럿 조종석, 16×7×4.5m 원거리 모서리, 매우 가까운 기체
- NaN, 0초 dt, FOV·head angle·속도·좌표·바닥·설정의 거부
- 기수의 파일럿 쪽/반대쪽/옆면 판정과 선택형 비활성화
- 우클릭 드래그·휠의 수동 보류와 매끄러운 자동 복귀
2026-10-31 Linux sandbox에서 실행한 실제 명령은 다음과 같다.
```text
mkdir -p /tmp/build-losframing && g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror -ISource/ImperialWingsSimulator Tests/los_auto_framing_tests.cpp -o /tmp/build-losframing/t && /tmp/build-losframing/t
```
실제 최종 출력 줄은 **`46 checks, 0 failures`**이다.
같은 sandbox에서 `LosAutoFraming.h`만 포함하는 임시 TU를 `-fno-exceptions -fno-rtti` 및 같은
경고 옵션으로 컴파일했고 종료 코드 0이었다. Windows MSVC와 Unreal Engine은 이 Linux sandbox에
없어 실행하지 않았다. `Tests/run-los-auto-framing-tests.ps1`는 MSVC `/std:c++17 /EHsc /W4 /WX /O2 /MT`
러너와 `Saved\Verification\los-auto-framing-unit-*` 로그·결과 JSON 경로를 제공하지만 여기서는
실행하지 않았다.
## 제한 사항
- Unreal 통합·빌드·실행본·실물·다른 PC·사람 검증은 미완료다.
- Unreal 렌더, 실제 화면비/DPI, HUD 클릭·포커스, ARM·입력 손실 경로와의 상호작용은 검사하지 않았다.
- Boxer 또는 다른 물리 조종기, USB 손실·복구, 실제 LOS 조종자 사용성은 검사하지 않았다.
- 실제 네트워크, LAN, 온라인 경기, 다른 GPU·Windows 사용자는 검사하지 않았다.
- `AircraftRollingOnFloor`와 뒤집힘 입력은 통합 측이 실제 Chaos 접촉·자세 상태에서 올바르게 공급해야 한다.
- 시뮬레이터 선택값은 실측 카메라·사람 눈높이·기체 식별 성능을 보증하지 않는다.
## 다음 통합 단계
1. `ImperialPlayerController.h`에 `ImperialLos::Settings`와 `ImperialLos::State`를 컨트롤러 인스턴스
   상태로 추가하되 기본 `Enabled=false`로 둔다. `PilotFieldOfView`의 기존 수동 값은 유지한다.
2. `ImperialPlayerController.cpp`의 `PlayerTick()` 또는 `UpdatePilotCamera()`에서 ActiveView 1일 때만
   `PilotPosition()`·기본 `Facing`·현재 head/FOV·DeltaSeconds와 선택 기체의 상태로 `Input`을 구성한다.
3. 같은 파일의 우클릭 드래그와 `ZoomIn/ZoomOut()` 경로를 `RightDragActive`·`WheelInputActive`로
   전달한다. HUD 모달(`HasRadioModal()`), 읽기 전용 시점, 장비 보기에서는 보조 계산이나 적용을 하지 않는다.
4. `UpdatePilotCamera()`는 `Step()` 결과가 `Active`일 때만 `PilotHeadYaw`, `PilotHeadPitch`,
   `PilotFieldOfView`에 출력값을 적용하고, `Rejected`이면 현재 수동 값을 보존하며 이유를 진단한다.
5. `SoccerDronePawn.h/.cpp`, `ImperialSoccerLayout.h`, `ImperialHUD.h/.cpp`를 사용해 선택 기체 상태,
   고정 조종석, opt-in 토글·방향 보조 표시·거부 이유를 연결한다. 저장은 `SYS-06` 범위에서 명시적
   저장/복원·실패 안내를 갖추고, HUD에는 `UX-13`에 맞춰 보조가 켜졌음을 표시한다.
6. 통합 뒤에는 양 팀 5개 조종석, 저고도·바닥 굴림·뒤집힘·원거리·수동 전환을 Unreal Editor/Shipping과
   실제 대표 해상도에서 별도 검증한다. 이 문서의 순수 g++ 결과를 그 검증 결과로 대체하지 않는다.
