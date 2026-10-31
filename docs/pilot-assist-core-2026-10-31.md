# Pilot Assist 진단 코어 — 2026-10-31
## 목적과 계획 연결

- 이 문서는 `PilotAssistDiagnostics.h`의 엔진 독립 진단/명시적 요청 코어를 설명한다.
- 대상은 통합 개발 계획의 사용자 작업 묶음 **7번** “조종·재시도 편의”다.
- 묶음 7의 추가 범위인 차단 원인 가시화, 드리프트 진단, DISARM 재시작,
  실패 위치·각도 재연습을 작은 순수 C++ 코어로 분리했다.
- 계획서 3절 이주 피드백 **10번 조종기 아밍키**의 새 누름·최저 스로틀·재연결 보호와 연결한다.
- 피드백 1번의 보정 실패 이유·재시도 안내 흐름도 차단 원인 키에 연결한다.
- 계획서 4절 조종기 설정 표의 **아밍** 행과 **장애 복구** 행을 따른다.
- 아밍 행의 “누른 채 시작/재연결, 높은 스로틀, 메뉴 복귀에서 예기치 않은 ARM 0회”를
  설명 전용 판단으로 보강한다.
- 장애 복구 행의 “이유/해결 안내, 연결 복구만으로 모터 재가동 금지”를 유지한다.
- 전체 계획의 단계는 **A 조종기**다. 단계 A의 실물 기기·다른 PC 확인 대기는 그대로다.
- 상세 명세 연결은 `INP-07`(버튼·스위치 ARM), `INP-09`(중립 흔들림·축 품질 진단),
  `CTL-08`(ARM·DISARM 전이), `CTL-09`(선택·리셋·모드 전환), `RUL-02`(ARM 허용 단계),
  `ARN-01`/`ARN-06`(Class40 경기장·리셋 일치), `TRN-08`(실패 후 빠른 재시작)이다.
- 이 문서와 헤더 추가만으로 위 기능들이 Unreal 실행 경로에 통합되었다고 표시하지 않는다.

## 설계 원칙

- 네 API 모두 헤더 전용 C++17이며 Unreal 형식, I/O, 전역 가변 상태, 예외와 RTTI에 의존하지 않는다.
- 이 코어는 모터 ARM, 조종기 활성화, 보정 저장, 기체 이동 또는 리셋을 직접 수행하지 않는다.
- 모든 수치 입력은 비유한값과 범위를 검사하고 명시적 enum 거부 사유를 반환한다.
- 단위는 필드명에 `M`, `Mps`, `Deg`를 붙여 구분한다.
- 번역 문구를 넣지 않고 `pilot_assist.arm.*`의 안정된 locale-neutral 키만 반환한다.
- UI는 이 키를 한국어/영어 `ImperialLocalization` 표에서 각각 해석해야 한다.
- 위치 이동은 사용자의 명시적 재연습 리셋 요청에 대한 외부 통합 단계에서만 가능하다.

## 1. 아밍 차단 설명 상태

`ExplainArmingBlock`은 `ArmingPreconditions`를 입력받아 ARM 요청을 수행하지 않고 모든 실패를 함께 보인다.

1. 입력이 비유한값, 범위 밖 스로틀/축/데드존, 알 수 없는 경기 단계이면 `InvalidInput`으로 종료한다.
2. 유효 입력은 우선순위가 고정된 다음 이유를 모두 배열에 담는다.
3. 순서는 경기 단계 불허 → 자율조종 활성 → 입력 연결 안 됨 → 입력 활성화 안 됨 →
   보정 미저장 → 연결 전부터 ARM 버튼 유지 → 미착륙 → 비정상 자세 → 높은 스로틀 → 다른 축 비중립이다.
4. `Practice`와 `Playing`만 수동 ARM 허용 단계로 취급한다. 이는 규정 코어의 기존 계약이다.
5. `Ready`, `Break`, `Finished`는 차단 이유로 남기며, 이 함수의 호출만으로 상태를 고치지 않는다.
6. 같은 평가에서 높은 스로틀과 축 흔들림, 보정 미저장 등을 하나만 숨기지 않는다.
7. `CanExplainSafeArm()`이 참이어도 외부 기존 ARM 게이트가 별도로 ARM을 허용해야 한다.

## 2. 스틱 드리프트 진단 상태

`StickDriftCollector`는 사용자가 스틱을 놓으라는 안내를 본 상태이면서 선택 기체가 DISARM일 때만 표본을 받는다.

1. 표본은 보정된 휴지점 대비의 4축 중심값 `[-1, 1]`이며, 스로틀도 호출자가 보정된 저점 대비값으로 제공한다.
2. ARM 상태, 안내 없이 수집, 비유한/범위 밖 표본, 꽉 찬 창, 잘못된 설정은 각각 거부한다.
3. 창마다 축별 평균, 표준편차, 최대 절댓값을 계산한다.
4. 최소 표본 전에는 `InsufficientSamples`이며 자동 보정은 절대 하지 않는다.
5. 표준편차가 한도를 넘으면 `Noisy`, 그렇지 않고 평균 또는 최대 절댓값이 데드존을 넘으면
   `DriftBeyondDeadband`, 둘 다 아니면 `Ok`다.
6. `Noisy`와 `DriftBeyondDeadband`만 재보정을 권고하고, 저장된 보정값은 변경하지 않는다.

## 3. DISARM 재시작 상태

`DisarmRestartHelper`의 상태는 `Idle → AwaitingSafeDisarm → ReadyForExplicitRestart → ExplicitRestartRequested`다.

- `Crash` 또는 `Flipped` 사건만 `AwaitingSafeDisarm`을 시작한다.
- 선택 기체는 DISARM, 낮은 스로틀, 나머지 축 중립을 모두 만족해야 제안 상태가 된다.
- 제안 상태는 재시작 실행이 아니다. `RequestExplicitRestart(true)`에서만 요청 플래그가 한 번 반환된다.
- 사용자가 명령하지 않거나 안전 조건이 미완료이면 이유만 반환하고 상태를 바꾸지 않는다.
- 이미 소비된 요청의 중복 요청도 거부한다.

## 4. 실패 위치·각도 재연습 상태

`FailurePointPractice`는 실패 시점의 위치, yaw, pitch, roll 체크포인트만 기록한다.

1. 체크포인트는 유한값, Class40 경기장 내부, 정규화한 `[-180, 180] Deg` 자세여야 저장된다.
2. 저장 뒤에도 외부에서 명시적으로 재설정 버튼/명령을 보내지 않으면 요청은 생성되지 않는다.
3. 명시 명령 뒤에는 다른 9대의 존재, DISARM, 비자율, 착륙 상태를
   `ImperialFirstFlight::CanResetSelectedAircraft`로 다시 확인한다.
4. 다른 기체의 높이·속도 안전 범위와 비유한값도 기존 첫 비행 리셋 계약으로 fail-closed 한다.
5. 모든 다른 기체는 체크포인트에서 보호구 반지름 0.20 m와 여유를 더한 거리보다 멀어야 한다.
6. 모든 조건이 맞아도 반환값은 목표 체크포인트가 든 `PracticeResetRequest`일 뿐 위치 이동은 하지 않는다.

## 파라미터와 출처

| 항목 | 값 | 출처와 의미 |
|---|---:|---|
| 경기장 경계 | 16 × 7 × 4.5 m | FIDA Class40 권장 경기장, `docs/rules-sources-2026.md` **1-3**. X/Y 원점 중심 표현은 시뮬레이터 좌표 가정값이다. |
| 보호구 반지름 | 0.20 m | FIDA Class40 외곽 40±2 cm, `rules-sources-2026.md` **2-1**에서 도출한 반지름이다. |
| 다른 기체 거리 여유 | 0.10 m | **시뮬레이터 선택값**. 따라서 기본 최소 중심거리는 0.30 m다. 공식 이격 규정으로 주장하지 않는다. |
| 낮은 ARM 스로틀 | `<= 0.06` | 기존 `PilotResponse.h`의 `ImperialInput::ArmingEdgeGate` 계약을 재사용했다. 규정값이 아니다. |
| 기본 데드존 | 0.04 | 기존 `RadioCalibration.h`의 `Profile::Deadzone` 기본값을 재사용했다. 규정값이 아니다. |
| 데드존 허용 범위 | 0.00–0.25 | 기존 `RadioCalibration.cpp::ValidateProfile` 계약을 재사용했다. 규정값이 아니다. |
| 드리프트 창/최소 표본 | 최대 64 / 기본 16 | **시뮬레이터 선택값**. 실물 조종기 잡음 측정값이 아니다. |
| 잡음 표준편차 한도 | 0.015 | **시뮬레이터 선택값**인 중심값 변동성 기준이다. |
| 다른 기체 착륙 판정 | 높이 0.05–0.35 m, 속도 0–0.25 m/s | 기존 `FirstFlightGuide.h::CanResetSelectedAircraft` 안전 계약을 직접 재사용했다. 규정값이 아니다. |
| 자세 허용 범위 | 각 축 -180–180 Deg | **시뮬레이터 선택값**인 저장 형식 검증 범위다. |

## 검사 범위와 실제 결과

- 실행 명령: `mkdir -p /tmp/build-pilotassist && g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror -ISource/ImperialWingsSimulator Tests/pilot_assist_tests.cpp -o /tmp/build-pilotassist/t && /tmp/build-pilotassist/t`
- 실제 최종 출력: **`46 checks, 0 failures`**.
- 단위 검사는 정상 ARM 설명, 모든 동시 차단의 우선순위, 임계값, 비유한/범위 밖 입력을 다룬다.
- 드리프트는 DISARM/명시적 안내 게이트, 충분하지 않은 표본, 정상·오프셋·잡음, 잘못된 설정을 다룬다.
- 재시작은 충돌/뒤집힘, DISARM·스로틀·중립, 명시 명령, 중복 명령과 비유한 입력을 다룬다.
- 재연습은 명시 명령 부재, 안전한 9대, 보호구 이격, 다른 기체 ARM/미착륙/비유한 상태,
  경기장·자세 경계를 다룬다.
- `/tmp/build-pilotassist/inc.cpp`에서 `-fno-exceptions -fno-rtti` 포함 전용 컴파일도 통과했다.
- Windows MSVC 러너 `Tests/run-pilot-assist-tests.ps1`은 작성했지만 이 Linux 환경에서 실행하지 못했다.
- MSVC 컴파일, Unreal 컴파일, Unreal 통합 실행본 검증은 수행하지 않았다.

## 명시적 제한

- Unreal 통합은 미완료다. `PilotAssistDiagnostics.h`는 아직 게임 HUD나 입력 흐름에서 호출되지 않는다.
- Unreal 빌드와 실행본, 패키징, 화면/HUD 조작 검증은 미완료다.
- 실물 Boxer 또는 다른 USB 조종기의 스틱·ARM 버튼·드리프트·분리/재연결 검증은 미완료다.
- 다른 물리 PC, 다른 GPU, 다른 Windows 사용자 검증은 미완료다.
- 사람 조종사·초보자 사용성·사람 비행/재시도 검증은 미완료다.
- 실제 LAN·네트워크·다중 사용자 검증은 수행하지 않았다.
- 드리프트/거리/자세의 시뮬레이터 선택값은 실기체 보정값이나 공식 규정이 아니다.

## 다음 Unreal 통합 단계

1. `ImperialPlayerController.cpp::UpdateRadioArming`에서 `UImperialRadioSubsystem`, 선택 `ASoccerDronePawn`,
   `ImperialMatchManager`의 실제 상태를 `ArmingPreconditions`로 매핑하되, 기존 `ArmingEdgeGate`와
   실제 ARM 호출보다 먼저 진단만 생성한다.
2. `ImperialHUD.cpp`에 차단 이유 목록을 표시하고, `ImperialLocalization.cpp`의 한국어/영어 표에
   `MessageKey` 반환값을 추가한다. 키가 없을 때는 ARM을 허용하는 대체 경로를 만들지 않는다.
3. `ImperialRadioSubsystem`의 실제 보정 중심값으로 드리프트 표본을 변환하고, DISARM 및 사용자의
   “스틱 놓기” 진단 시작 명령에서만 `StickDriftCollector::AddSample`을 호출한다.
4. 충돌/뒤집힘 사건에서 `DisarmRestartHelper`를 시작하고, HUD의 명시적 재시작 버튼이 반환한
   단일 요청만 기존 선택 기체 재시작 경로에 전달한다.
5. `AImperialFirstFlightGuide::ResetAircraftAndRestart`와 `FirstFlightGuide.h::CanResetSelectedAircraft`의
   다른 9대 fail-closed 계약을 보존한 채, `FailurePointPractice::RequestReset`의 안전한 명시 요청만
   `ASoccerDronePawn::ResetDrone` 이전에 소비한다.
6. 통합 뒤에는 MSVC `/W4 /WX` 러너, Unreal Editor/Shipping, 실제 화면, 실물 조종기,
   다른 PC와 사람 사용성 검사를 별도 증거로 다시 수행한다.
