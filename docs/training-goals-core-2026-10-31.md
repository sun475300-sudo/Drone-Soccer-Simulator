# 훈련 목표 코어 — 2026-10-31

## 목적과 계획 연결

`TrainingGoals.h`는 Unreal 타입·I/O·기체 명령 없이 훈련 시도의 궤적 표본을 판정하는 순수 C++17 코어입니다.

이 결과는 시뮬레이터 연습 피드백일 뿐 공식 실력 인증·실기체 자격·경기 판정이 아닙니다.

- 20개 작업 묶음 **8번**의 남은 정확도/속도 목표, 구간 저장, 동일 조건 반복, 선택형 음향을 대상으로 합니다.
- 피드백 **11번**(AI 연습/대전·3~4대·난이도)의 연습 결과·피드백 후속 범위와 연결하되 AI를 조종하거나 난이도를 바꾸지 않습니다.
- 통합 계획서 §5 AI 연습의 평가 열(유효 골·골 시도·충돌·복귀 시간·훈련 지속 시간)에 호버 지속·게이트 오프셋·구간 시간을 보조 지표로 더할 수 있습니다.
- 단계 표의 **B AI**(연습/대전·결과, 실제 이동·점수·장애·재시도 검사)에서 사용할 엔진 비의존 판정 하위 코어입니다.
- `TRN-02` 호버 훈련, `TRN-03` 게이트·통과 시간·경로 오차, `TRN-08` 재시작, `TRN-09` 조건 다른 성적 비교 차단에 직접 연결됩니다.
- 사실 기반 결과 해석은 `TRN-12`, 같은 설정 비교는 `TRN-13`, 사용자 과제 정의·거부는 `TRN-14`, 실기체 자격 대체 금지는 `TRN-15`를 따릅니다.

기존 `AIResultComparison`의 보수적 원칙도 유지합니다. 다른 조건·버전, 자동 검사, 중단 시도, 같은 시도 ID는 비교할 수 없습니다.

## 설계와 상태 기계

`GoalEvaluator::Begin`은 과제 정의, 동일 조건 키, 0이 아닌 시도 ID를 검증한 뒤 `Active` 상태로 시작합니다.

첫 표본은 기준 표본으로만 저장하고, 이후 `AddSample`이 시간 증가·최대 공백·유한 수·좌표·자세 범위를 모두 확인합니다.

시간 역행, 비유한 값, 범위 초과, 정의보다 긴 표본 공백은 `Invalidated`와 명시적 `Rejection`을 남기며 성공을 부여하지 않습니다.

호버 목표는 이전/현재 표본 모두 수평 반경과 고도 띠 안에 있을 때만 경과 초를 누적합니다.

경계 밖 표본은 연속 시간을 0으로 되돌리며, 가장 긴 보유 시간은 실패 시에도 피드백 지표로 보존합니다.

게이트 목표는 이전 위치와 현재 위치를 잇는 선분-평면 교차 비율을 계산해, 프레임 사이의 통과 시각과 평면 내 오프셋을 보간합니다.

방향은 양방향·음에서 양·양에서 음 중 과제 정의가 선택한 값만 허용합니다.

구간 속도 목표는 사용자가 저장할 `Start`/`End` 체크포인트를 사용하며, 시작 뒤 끝을 통과해야 합니다.

하나의 표본 구간에서 양쪽 평면을 지나도 보간 시각 순서를 비교하고, 끝을 먼저 지나는 역순 통과는 완주가 아닙니다.

끝 시각−시작 시각이 목표 시간 이하이면 `Succeeded`, 명시 종료 때 목표를 못 채우면 `Failed`, 호출자가 중단하면 `Aborted`입니다.

이 상태 기계는 자동 ARM·자동 조종·AI 명령·물리 위치 이동·파일 저장을 수행하지 않습니다.

## 동일 조건·비교·진행 요약

`SameConditionKey`는 drill version, physics profile, gravity (`GravityMps2`), assist option mask, input mode, build fingerprint를 함께 보관합니다.

`CanCompare`는 완료된 수동 시도 둘만 허용하고 같은 ID·자동 검사·중단·다른 과제·다른 조건 키를 각각 차단 사유로 반환합니다.

이는 `AIResultComparison.cpp`의 source hash/build version/mode/opponents/difficulty/seed/test-session 차단 원칙과 맞춥니다.

`BuildProgressSummary`는 같은 과제·같은 조건의 수동 완료 시도만 집계합니다.

요약은 `SuccessCount`와 `AttemptCount`를 분리하며 백분율 필드가 없습니다.

UI는 반드시 `성공 수 / 시도 수`처럼 두 수를 표시하고 성공률 단독 표기를 피해야 합니다.

최고 기록은 호버 보유 시간은 큰 값, 게이트 오프셋과 구간 시간은 작은 값을 더 좋게 봅니다.

중앙값은 호출자가 시간순으로 공급한 기록에서 최근 `N`개의 유효 지표만 쓰며, `N≤32`는 고정 메모리를 위한 시뮬레이터 선택값입니다.

선택형 음향은 `SuccessCueId`·`FailureCueId`·`NewBestCueId`만 고르고, ID 0은 미선택이며 `Muted=true`면 모든 큐를 막습니다.

코어는 음향 재생 API를 호출하지 않습니다.

## 파라미터 근거

| 항목 | 값/처리 | 근거 |
|---|---|---|
| 호버 반경·고도 띠·보유 시간 | 사용자 저장 과제 정의 | 시뮬레이터 선택값 |
| 게이트 평면·방향·최대 오프셋 | 사용자 저장 과제 정의 | 시뮬레이터 선택값 |
| 시작/끝 체크포인트·목표 시간 | 사용자 저장 구간 정의 | 시뮬레이터 선택값 |
| 최대 표본 공백 | 과제별 0.001~1.0 s | 시뮬레이터 선택값; 누락 자료를 성공으로 보지 않는 안전 경계 |
| 좌표/자세/시도 시간 한계 | 1000 m, ±720 Deg, 86400 s | 시뮬레이터 선택값/가정값 |
| gravity 조건 값 | 0~30 m/s² 유한값 | 시뮬레이터 선택값; 실측 중력·기체 보정값 아님 |
| 입력/보조/빌드 비교 키 | 정확 일치만 허용 | `TRN-09`의 조건 다른 성적 비교 차단 |
| FIDA 경기장·골대 수치 | 이 코어에는 미적용 | `docs/rules-sources-2026.md` §1-3, §1-5는 별도 경기 프로필 근거 |

FIDA 규정은 호버 반경·과제 보유 시간·게이트 오프셋·연습 구간 목표 시간을 제공하지 않습니다.

따라서 위 임계값에 임의의 규정 조항을 붙이거나 공식 규정값으로 표기하지 않습니다.

## 검사 범위

실행한 명령은 다음과 같습니다.

```text
mkdir -p /tmp/build-traininggoals && g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror -ISource/ImperialWingsSimulator Tests/training_goals_tests.cpp -o /tmp/build-traininggoals/t && /tmp/build-traininggoals/t
```

실제 최종 출력 줄은 **`81 checks, 0 failures`** 입니다.

정상 호버, 경계 고도/반경, 연속성 리셋, 표본 공백, 시간 역행, 비유한·범위 밖 입력, 선분-평면 스윕, 방향/오프셋 경계, 구간 순서/목표 시간, 비교 차단, 진행 요약, 음소거를 검사했습니다.

호버·게이트·구간 모두 20/60/240 FPS 입력에서 동일한 성공 및 보간 지표를 확인했습니다.

`/tmp/build-traininggoals/inc.cpp`의 헤더 단독 포함 TU도 `-fno-exceptions -fno-rtti`와 동일한 엄격 경고 설정으로 컴파일했습니다.

Windows MSVC `/W4 /WX`, Unreal Engine 5.8 컴파일, Unreal 실행본, 패키징은 실행하지 않았습니다.

`Tests/run-training-goals-tests.ps1`은 Windows에서 MSVC·SDK를 찾고 매 실행마다 `Saved/Verification/training-goals-unit-<timestamp>-<guid8>`에 compile/test 로그와 result JSON을 남깁니다.

## 제한과 다음 통합

Unreal 통합·Unreal 빌드·실행본·패키징 검증은 미완료입니다.

실물 Boxer/기타 조종기, 실기체, 다른 PC, 실제 LAN/네트워크, 사람 조종·사람 사용성 검증은 미완료입니다.

20/60/240 FPS 검사는 이 헤더에 공급한 합성 선형 궤적의 순수 C++ 검사이며 Chaos 물리나 서로 다른 FPS의 실제 Unreal 궤적 일치를 주장하지 않습니다.

다음에는 `ImperialAIPractice.cpp`가 게임 시뮬레이션 시간·기체 위치·자세를 `TrajectorySample`으로 변환해 활성 `GoalEvaluator::AddSample`에 공급합니다.

`ImperialAIResults.cpp`와 `AIPracticeResult.h`에는 과제 정의 버전, 동일 조건 키, 시도 ID, 상태/거부 사유, 지표를 새 스키마로 명시 저장해야 합니다.

`ImperialAIComparison.cpp`와 `AIResultComparison.h/.cpp`는 저장된 새 조건 키를 읽어 기존 자동 검사·중단·조건 차이 차단과 함께 `CanCompare`를 적용해야 합니다.

`ImperialAIPracticeHUD.cpp`와 `ImperialHUD.cpp`는 진행 상태, 성공 수/시도 수, 최고값, 최근 N 중앙값, 차단 이유, 음소거된 선택형 큐를 표시하되 성공률 단독 표기를 피해야 합니다.

`ImperialGameMode.cpp`는 과제 시작/재시작/명시 중단을 연결하되 기존 수동 ARM·LOS·ANGLE 계약을 유지하고 이 코어에 자동 조종 권한을 주지 않아야 합니다.

실제 통합 뒤에는 새 저장 형식의 손상/호환/보존, AI/수동 조건 전환, 실제 Unreal 프레임, Windows MSVC, 실행본, 사람·실물·다른 PC 검증을 각각 별도로 해야 합니다.
