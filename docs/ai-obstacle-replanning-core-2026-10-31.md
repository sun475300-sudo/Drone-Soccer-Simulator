# AI 다중 장애물 재계획 코어 — 2026-10-31

## 목적과 계획 연결

이 문서는 `AIObstaclePlanner.h`의 엔진 독립 수평 웨이포인트 계획 코어를 설명한다.
이 코어는 이동/회피 계층이며, 공격·수비 전술 결정을 대신하지 않는다.

- 전체 계획의 20개 작업 묶음 중 **9번 AI 고도화: 다중 장애물**의 후속 범위다.
- 이 묶음은 기존 3/4대·난이도·수비/대전 기반이 있어도 다중 장애물 재계획은 미검증/후속이라고 기록한다.
- 이 작업은 피드백 **11번**(AI 연습/대전·3~4대·랜덤 수비·초중고급)의 회피 보강 경로다.
- 단계별 산출물에서는 **B AI**에 속한다. B의 출구 기준인 실제 이동·점수·장애·재시도 검사는 아직 Unreal 통합 뒤에 별도로 수행해야 한다.
- 마스터 계획 §5의 “APF는 이동/회피 계층, 공격·수비 의사결정과 분리” 원칙을 따른다.
- 마스터 계획 §15의 미완료 체크리스트인 “B의 다중 장애물 재계획”에만 해당하며, 사람 팀 AI 동료·5대5 협력·비교 분석은 범위 밖이다.
- 연결 기능 ID는 AI-01(실제 APF 출처 검증), AI-05(기체·골대·네트 회피), AI-06(교차·밀집·교착 복구), AI-07~AI-10(연습 상대·난이도·대전·기록), AI-11(Practice 제한)이다.

이 파일은 `ImperialAI` 네임스페이스에 추가하지 않고, 충돌을 피하기 위해 `ImperialAIPath`를 사용한다.
출력은 X/Y 단위의 웨이포인트뿐이다. 호출자가 기존 모터 힘·토크와 ANGLE 제어로 각 웨이포인트를 비행해야 하며, 이 코어는 기체 위치를 순간이동시키지 않는다.

## 범위와 입력

`PlanRequest`는 시작·목표의 수평 좌표 `PointM`, 선택 고도 `AltitudeM`, 고도 밴드, 여유, 벽 여유, 재계획 제한과 원형 장애물 목록을 받는다.
`ObstacleSet`은 최대 8개이며 다른 기체, 골대 링, 선택 기둥을 구분한다.
모든 좌표는 m, 시간은 s, 속도나 가속도를 지시하지 않는 경로 기하 정보다.

고도는 이 코어가 Z 경로를 만들지 않는 고정 밴드 입력이다.
고도·밴드·장애물·여유·재계획 값의 비유한값, 범위 오류, 잘못된 장애물 종류, 시작/목표의 keep-out 내부를 명시적 `Rejection`으로 거부한다.
잘못된 값에서 예외, RTTI, I/O, 전역 변경 상태 또는 위치 강제 갱신을 사용하지 않는다.

## 경로 계획 설계

1. 벽 여유만큼 16×7m 경기장을 축소한 직사각형을 항법 가능 수평 영역으로 만든다.
2. 각 원형 장애물을 기체 가드 반경 또는 물리 반경과 안전 여유의 합으로 확장한다.
3. 시작점·유효 목표점·네 모서리에서 각 원으로 향하는 접점을 만든다.
4. 원-원 공통 접점도 추가하여 여러 장애물 사이의 후보 통로를 만든다.
5. 직선 가시 간선과 원호 간선을 만들고 Dijkstra로 길이가 가장 짧은 후보 경로를 선택한다.
6. 원호는 작은 바깥 여유를 둔 복수 웨이포인트로 이산화한다. 웨이포인트 사이 현이 keep-out 내부를 자르지 않게 한다.
7. 완성된 모든 선분을 다시 검사하여 벽과 확장 장애물 안으로 들어가지 않은 경우에만 성공한다.

노드 생성 순서와 동률 처리 순서는 고정이다. 같은 입력은 같은 웨이포인트 수·좌표·길이를 낸다.
가시성 그래프는 작은 원형 장애물 사례에서 접선/원호 최단 후보를 직접 다루며, 단위 테스트는 별도 격자 Dijkstra 기준 길이와도 비교한다.
벽과 겹치는 확장 원 또는 서로 겹치는 복잡한 확장 원의 모든 연속 공간 최단 경로를 증명하는 범용 기하 해석기는 아니다. 이 경우 보수적으로 `NoPath`가 나올 수 있으며, 회복 감독이 처리해야 한다.

### 골대 축 접근 예외

골대 링은 기본적으로 원형 keep-out이다.
`GoalAxisApproach.Requested`가 참인 경우에만 예외를 허용한다.
이때 목표는 선택한 골대 링 중심과 일치해야 하고, `OutwardAxisM`은 링 중심에서 접근 입구로 향해야 한다.
계획기는 먼저 링 바깥의 축 입구까지 안전하게 경로를 만들고, 마지막 한 선분만 링 중심으로 향하도록 추가한다.
따라서 일반적인 옆 방향 접근에서 골대 링을 무시하지 않는다.

## 재계획 상태 기계

| 상태/사건 | 동작 | 결과 |
|---|---|---|
| `Begin` | 입력 검증 후 최초 경로 생성 | 성공 시 Active, 실패 시 `Rejection` 반환 |
| Active + 현재 경로 clear | 현재 위치부터 다음 웨이포인트 이후 선분을 새 장애물 목록으로 재검사 | `Retained` |
| Active + 차단 + cooldown 미만 | 위험을 감지하되 즉시 반복 재계획하지 않음 | `BlockedRateLimited` |
| Active + 차단 + cooldown 경과 | 현재 기체 좌표에서 목표까지 새 접선 그래프를 생성 | 성공 시 `Replanned` |
| 새 경로 없음/거부 | 실패 횟수 증가, 이전 경로를 성공 경로로 표기하지 않음 | `ReplanFailed`와 구체 `Rejection` |
| 연속 실패가 최대값 도달 | 비활성화하고 상위 복구/착륙 감독에 신호 | `LandingRequested` |

`Update`는 호출자가 현재 좌표, 다음 웨이포인트 인덱스, 최신 장애물 목록을 전달할 때만 동작한다.
이는 이동한 다른 기체가 이전 경로를 가로막는 경우를 감지하기 위한 계약이다.
`LandingRequested`는 기존 `ImperialAI::Recovery`의 실패 종료·팀 착륙 흐름에 연결할 신호이지, 헤더가 모터를 끄거나 착륙시키는 동작이 아니다.

## 매개변수와 출처

| 항목 | 값/범위 | 출처와 의미 |
|---|---:|---|
| 경기장 수평 크기 | 16m × 7m | FIDA Class40 규정 근거 문서 §1-3의 권장 경기장 |
| 경기장 높이 상한 | 4.5m | FIDA Class40 규정 근거 문서 §1-3 |
| 기체 가드 반경 | 0.20m | FIDA Class40 외곽 40±2cm, 규정 근거 문서 §2-1에서 반경으로 환산 |
| 골대 링 외곽 반경 예시 | 0.50m | 내경 60±1cm·외경 100±1cm, 규정 근거 문서 §1-5; 호출자가 링 반경으로 제공 |
| 기본 고도/밴드 | 2.0m / 0.20~4.30m | **시뮬레이터 선택값**. 수평 계획 입력일 뿐 공식 비행 고도 규정이 아님 |
| 안전 여유 | 기본 0.05m, 최소 0.01m | **시뮬레이터 선택값**. 실기체 충돌·센서 오차 실측값이 아님 |
| 벽 여유 | 기본 0.25m | **시뮬레이터 선택값**. 벽 접촉·기체 지름과 별개인 항법 경계 |
| 원호 바깥 여유 | `min(안전 여유/2, 0.02m)` | **시뮬레이터 선택값**. 이산 웨이포인트의 현이 keep-out을 자르지 않게 하는 수치 여유 |
| 재계획 cooldown | 기본 0.25s | **시뮬레이터 선택값**. 프레임·통신 성능 측정값이 아님 |
| 최대 재계획 실패 | 기본 2회 | **시뮬레이터 선택값**. 기존 `AIRecovery`의 연속 우회 2회 정책과 정합을 고려했으나 공식 규정값이 아님 |

공식 FIDA 규정은 이 코어의 AI 자율 주행을 공식 경기 적합으로 만들지 않는다.
AI 자율은 기존과 같이 Practice 보조 범위에만 연결해야 한다(AI-11).

## 독립 단위 검사

다음 g++17 명령을 저장소 루트에서 실행했다.

```text
mkdir -p /tmp/build-aiobstacle && g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror -ISource/ImperialWingsSimulator Tests/ai_obstacle_planner_tests.cpp -o /tmp/build-aiobstacle/t && /tmp/build-aiobstacle/t
```

실제 최종 출력의 범위 문구는 다음과 같다.

```text
Scope: pure deterministic C++ waypoint/replanning tests; no Unreal build/runtime, physical controller, network, other-PC or human-flight verification
32 checks, 0 failures
```

검사 범위는 빈 경기장의 직선 경로, 단일/복수 원형 장애물, 목표의 keep-out 내부 거부, 경기장 폭보다 좁은 확장 간격의 `NoPath`, 명시 골대 축 접근, 벽 경계, 비유한 입력, 결정성, 작은 사례의 격자 Dijkstra 길이 비교, 이동 장애물 재계획, cooldown, 반복 실패 후 착륙 신호다.
별도로 `/tmp/build-aiobstacle/inc.cpp`에서 이 헤더만 포함해 `-fno-exceptions -fno-rtti`와 같은 엄격 경고 옵션으로 컴파일했다. 종료 코드는 0이었다.

`Tests/run-ai-obstacle-planner-tests.ps1`는 MSVC `/std:c++17 /EHsc /W4 /WX /O2 /MT`와 새 `Saved\Verification\ai-obstacle-planner-unit-...` 폴더, `compile.log`, `test.log`, `result.json`을 만들도록 작성했다.
이 Linux 샌드박스에는 MSVC와 Unreal Engine 5.8이 없으므로 해당 PowerShell 러너, MSVC 컴파일, Unreal 빌드·실행은 수행하지 않았다.

## 명시적 제한

- Unreal 통합, Unreal 빌드, 패키지 실행본 검증은 미완료다.
- 실제 Chaos 충돌·네트 얽힘·골대 충돌체와 이산 웨이포인트의 상호작용은 검증하지 않았다.
- 실제 Boxer 또는 다른 실물 조종기, USB 손실/재연결, 실기체 비행 검증은 미완료다.
- 다른 물리 PC, 다른 GPU, LAN/실제 네트워크, 사람 조종자·코치의 난이도/사용성 검증은 미완료다.
- 이 코어는 전술 선택, 점수 판정, 공격수 권한, 센서 가림/지연, 실제 APF 출처 검증과 별개다.
- 기본 매개변수는 시뮬레이터 선택값이며 실제 안전 거리·물리 성능·공식 경기 적합성을 보증하지 않는다.

## 다음 Unreal 통합 단계

1. `Source/ImperialWingsSimulator/ImperialMatchManager.h`에 기체별 `ImperialAIPath::Replanner` 보관소와 다음 웨이포인트 인덱스를 추가한다. 기존 `ImperialAI::Planner`, `ImperialAI::Attacker`, `std::array<ImperialAI::Recovery,4>`와 이름/상태를 섞지 않는다.
2. `Source/ImperialWingsSimulator/ImperialAIPractice.cpp`에서 현재 `AIPlanner.Step`과 `AIAttacker.Step`가 만든 전술 목표를 `PlanRequest.TargetM`으로 전달한다. 현재 기체 위치와 다른 AI 기체·골대·선택 기둥의 실제 수평 좌표를 `ObstacleSet`으로 매 Tick 제공한다.
3. 같은 파일에서 반환 웨이포인트를 기존 자율 목표 API에만 전달하고, `SetActorLocation`류의 위치 강제 갱신을 추가하지 않는다. `AIAttacker`의 속도/가속도 제한과 기존 모터 기반 제어를 그대로 적용한다.
4. `UpdateCode::LandingRequested` 또는 `ReplanFailed`의 최대 실패를 현재 `AIRecovery[I].Step`의 `RecoveryAction::Failed` 처리 및 `StopAI(TEXT("recovery_failed"))`/팀 착륙 흐름으로 연결한다. 플레이어 기체 자동 ARM·자동 착륙 금지는 유지한다.
5. `ImperialAIRecoveryVerifier.cpp`와 새 엔진 검증에서 단일/복수 이동 기체, 골대 축 예외, 벽 경계, cooldown, 2회 실패 후 실제 착륙을 실제 모터 힘과 Chaos 상태로 검사한다. 결과·F6·저장에는 재계획 횟수와 마지막 `Rejection`을 새 필드로 명시적으로 추가한다.
6. AI Practice에 한정하고 `ImperialMatchManager`의 공식 수동 경기 전환 차단(AI-11)을 회귀 검사한다. 이후 MSVC·Editor·Shipping·새 압축 해제본·다른 PC·실물/사람 검증을 각각 별도 근거로 기록한다.
