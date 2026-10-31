# 경기 심판 판정 장부 코어 — 2026-10-31

## 목적과 계획 연결

`MatchOfficiating.h`는 자동 기하 판정과 사람 심판의 최종 결정을 분리하는 헤더 전용 C++17 장부입니다.
기존 `DroneSoccerRules`의 골대 통과·역진입·오프사이드 판정은 바꾸지 않습니다. 자동 코어가 만든
득점 후보를 심판이 승인할 때만 공식 점수에 반영하고, 취소·재검토·사유를 수정 이력으로 남깁니다.

- 사용자 20개 작업 묶음 **17번 경기 규칙·심판**의 추가 판정 범위입니다.
- 통합 개발 계획서 §8의 **판정 확장** 행과 **심판** 행을 연결합니다.
- 단계 **G 경기 운영**의 승계·기권·페널티·심판 정정·결과 확정 선행 코어입니다.
- 피드백 12개 중 **7번 득점 골대 불빛·점수**의 ‘승인된 득점 사건 하나’를 제공할 준비 계층입니다.
  이 작업 자체는 불빛·HUD·효과음을 연결하지 않았습니다.
- 기능 명세의 `RUL-05`~`RUL-17`, `REF-02`~`REF-06`, `REF-09`를 우선 연결합니다.
  `REF-01`, `REF-07`~`REF-08`, `REF-10`~`REF-12` UI·VAR·대회 운영은 후속입니다.

## 근거와 적용 경계

기준 출처는 `docs/rules-sources-2026.md`만 사용했습니다. 적용 규정은 FIDA Class40,
제5차 개정(2025-10-01), 2026-01-01 시행입니다. FAI 규정이나 다른 대회의 절차는 혼합하지 않습니다.

| 항목 | 장부 처리 | 근거·표시 |
|---|---|---|
| 팀 최소 인원 | 팀별 활성 인원 3명 미만을 명시적 기권 근거로 검사 | FIDA §3-1 |
| 득점 후보 | 자동 코어 후보는 Pending이며 Approved일 때만 공식 골 | FIDA §§7~11의 득점·경기 구조, 자동/주심 분리는 `RUL-17` 설계 |
| 경고 | 팀별 경고는 세트 사이 유지하고, 최종 확정 시 0으로 초기화 | FIDA §§7~11 |
| 페널티 사유 | 자기 골대 역진입, 오프사이드 연속득점, 누적 경고 2회 열거 | FIDA §§7~11 |
| 페널티 실행 | 세트 뒤 공격수 대 키퍼 1:1; 결과는 심판이 입력하고 창은 최대 5초 | FIDA §§7~11 |
| 정규 경기 | 3분×3세트, 2세트 선승 또는 3세트 뒤 더 많은 세트승 | FIDA §§7~11 |
| 1승 2무 | 세트 무승부를 보존한 채 세트승 수로 경기 승리를 계산 | FIDA §§7~11 |
| 연장·승부치기 | `ExtensionOrShootoutHookRequested`는 요청 표시만 제공 | **확인필요**: 대회 지침 없이 절차·점수·승자 규칙을 구현하지 않음 |

`180.0 s`의 정규 세트 상한과 `5.0 s` 페널티 창은 위 규정 근거값입니다. 종료 호각과 같은
`180.0 s` 후보는 호각 장부 입력 전이면 보존하고, 뒤의 새 후보는 거부합니다. 같은 시각의 후보
정렬에는 제출 순번을 사용합니다. 이 동률 정렬은 **시뮬레이터 선택값**이며 실제 대회 시간 계측
우선순위를 주장하지 않습니다.

후보 64개, 페널티 32개, 역할 승계 요청 16개, 수정 이력 256개, 사유 UTF-8 바이트 최대 95개는
메모리 상한을 위한 **시뮬레이터 선택값**입니다. 공식 경기의 사건/문자 수 제한이 아닙니다.

## 상태 기계와 불변식

### 득점 후보와 수정 이력

1. 자동 규칙 코어가 팀·세트·0~180초 시각·근거 텍스트로 `GoalCandidate`를 기록합니다.
2. 후보는 `Pending → Approved`, `Pending/Approved → Cancelled`로 전이합니다.
3. 승인/취소된 후보는 `UnderReview`로 되돌릴 수 있고, 재검토 뒤에만 다시 승인됩니다.
4. 중복 승인·취소, 없는 ID, 빈 사유, 비유한 시각과 범위 밖 시각은 `Rejection`으로 거부합니다.
5. `RevisionEntry`는 후보 등록·승인·취소·재검토와 이후 운영 결정을 번호순으로 추가만 합니다.

`GetOfficialSetScore`는 Approved 후보와 심판이 `Scored`로 입력한 해결된 페널티만 합산합니다.
Pending, Cancelled, UnderReview 후보는 점수에 들어가지 않습니다. HUD나 골대 연출은 이 조회값만
사용해야 합니다.

### 세트, 페널티, 기권

정규 세트는 `Open → AwaitingPenaltyShots/AwaitingDecision → PenaltyShots → AwaitingDecision →
Committed` 흐름을 사용합니다. 종료 호각 뒤에도 이미 기록된 후보의 승인·취소·재검토는 가능하지만,
미해결 후보 또는 미입력 페널티가 있으면 세트 확정을 거부합니다.

두 번째 경고는 같은 경기에서 `TwoWarnings` 페널티 하나를 자동 예약합니다. 경고는 다음 세트로
넘기며, 확정된 경기 뒤에는 조회값을 0으로 초기화합니다. 재개방 시에는 확정 직전 경고 수를 복원합니다.

`SchedulePenalty`는 `OwnGoalReverseEntry`, `OffsideConsecutiveScoring`, `TwoWarnings`만 받습니다.
세트 종료 뒤 `BeginPenaltyShots`로 1:1 단계에 들어가고, 심판이 득점/실패와 0~5초 경과를 기록합니다.
이 장부는 스트라이커·키퍼의 실제 배치·조종·물리·영상 검토를 실행하지 않습니다.

`DeclareForfeit`는 세트/경기 범위를 구분합니다. `BelowMinimumActivePlayers`는 활성 3명 이상이면
거부됩니다. `RefereeDeclaredOtherGuidanceRequired`는 다른 기권 사유를 자동 규정화하지 않기 위한
명시적 **대회 지침 확인 필요** 표식입니다.

### 역할 승계와 최종 결과

가이드를 공격수로 넘기는 정확한 절차는 보관된 규정 출처에 상세하지 않습니다. 따라서 기본 정책은
비활성이며, 활성화해도 현재 Guide/Striker ID가 맞는 요청을 주심이 승인해야 `CurrentStriker`를
바꿉니다. 승인된 요청은 항상 `TournamentGuidanceRequired=true`로 남습니다. 초기 참가자 ID 0/1,
5/6은 **시뮬레이터 선택값**이며 팀·기체 등록 계층이 실제 ID를 공급해야 합니다.

세트 확정은 세트승/무승부를 저장하고, 2세트 선승 또는 세 번째 세트 뒤 세트승 수를 비교합니다.
총 골 수로 경기 승자를 정하지 않습니다. `FinalizeMatch`만 결과를 잠그며, 잠긴 뒤 정정은 빈 사유를
허용하지 않는 `ReopenFinalizedMatch`와 새 이력 항목이 있어야 합니다. 재개방 뒤 다시 확정할 수 있습니다.

## 독립 검사

저장소 루트에서 실행한 명령은 다음과 같습니다.

```sh
mkdir -p /tmp/build-officiating && g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror -ISource/ImperialWingsSimulator Tests/match_officiating_tests.cpp -o /tmp/build-officiating/t && /tmp/build-officiating/t
```

실제 최종 출력 줄은 **`93 checks, 0 failures`** 입니다. 검사는 다음을 포함합니다.

- 종료 직전 179.999초/정확히 180초 후보의 순서, 호각 뒤 후보 거부와 종료 뒤 승인
- 승인 뒤 취소, 취소 뒤 재검토·재승인, 이중 승인/취소와 미해결 검토의 세트 확정 차단
- 세트를 넘는 경고 누적, 2경고 페널티, 1:1 결과와 1승 2무 경기 승리
- 비유한/5초 초과 페널티 시각, 잘못된 팀·결과·사유·설정의 거부
- 3명 최소 인원 경계, 세트패·경기패, 정책 기반 역할 승계
- 최종 잠금, 사유 없는 재개방 거부, 사유 있는 재개방 뒤 점수 정정·재확정

`Tests/run-match-officiating-tests.ps1`는 Windows에서 `vswhere`로 MSVC를 찾아
`/std:c++17 /EHsc /W4 /WX /O2 /MT`로 같은 테스트를 빌드하고,
`Saved\Verification\match-officiating-unit-<timestamp>-<guid8>`에 `compile.log`, `test.log`,
`result.json`을 남깁니다. 이 Linux 샌드박스에는 MSVC와 Unreal Engine이 없어 PowerShell/MSVC 실행,
Unreal 컴파일, Editor/Shipping 빌드와 실행본 검사는 실행하지 않았습니다. 별도로 `-fno-exceptions
-fno-rtti` 단독 포함 컴파일은 통과했습니다.

## 제한과 다음 통합 단계

- Unreal 통합·Unreal 빌드·Windows 실행본 검증은 미완료입니다.
- 실제 Boxer/다른 물리 PC/사람 플레이테스트/실제 네트워크 시험은 수행하지 않았습니다.
- 실제 경기 시계, 골대 통과, 오프사이드, 태그, 물리 충돌, 실기체 조종과 VAR 영상은 이 헤더가
  검증하거나 재현하지 않습니다.
- 연장·승부치기, 기타 기권 근거, Guide 승계의 접촉·표시·복귀 절차는 대회 지침 확인 전 공식 적합
  기능으로 표시하지 않습니다.

통합 시 `Source/ImperialWingsSimulator/ImperialMatchManager.h`에 `Ledger` 소유 필드를 추가하고,
`ImperialMatchManager.cpp`의 `AImperialMatchManager::UpdateScoring()`에서
`drone_soccer::MatchRules::Update()`가 탐지한 변화만 후보 입력으로 변환합니다. 후보의 원래 경기
시각과 기체/팀 증거를 전달하되 `DroneSoccerRules.h/.cpp`를 수정하거나 즉시 HUD 점수를 올리면 안 됩니다.

다음으로 주심 UI는 후보·사유·수정 이력과 `Rejection`을 표시하고,
`GetOfficialSetScore`/`GetMatchOutcome`만 `AImperialMatchManager::GetScore`, `GetSetWins`, 결과 화면에
반영해야 합니다. 승인 사건만 `MatchPresentationCues.h`의 골 신호/점멸 소비 경로에 넘기고,
기존 자동 `Rules.Score()` 표시와 중복 연출되지 않게 전환 회귀를 추가합니다. 활성 인원은 팀/기체 등록
계층에서, 승계 정책·현재 역할은 역할 LED/태그와 함께 동기화해야 합니다.

상태 제안: **핵심 로직 구현·네이티브 검사 통과(g++), Unreal 통합·실행 검증 대기**.
