# 경기 연출 큐 코어 — 2026-10-31
## 목적과 계획 연결
이 문서는 `MatchPresentationCues.h`의 순수 C++ 경기 연출 큐 생성기를 기록한다.
이 코어는 기체 위치·골대 형상·충돌을 다시 판정하지 않고,
`drone_soccer::MatchRules`가 이미 승인한 프레임 스냅샷만 읽는다.

연결 대상은 [통합 개발 계획서](master-development-plan.md)의 사용자 작업 묶음 **11**,
`득점·경기 연출`이다. 이 묶음은 현재 전체 연출 범위로는 미구현 후속이며,
이번 파일은 그중 독립 가능한 결정 로직만 추가한 것이다.

- 피드백 **7**: 승인 득점과 팀색 골대 불빛·HUD·효과음을 한 사건에 연결한다.
- 피드백 **9**: 준비·시작·종료·마지막 10초 경고와 무음/감소 효과 선택을 연결한다.
- 단계 **E 경기 연출·설정**: 득점 점멸·시작/종료·10초 경고의 산출물에 해당한다.
- 단계 E의 출구 조건인 점수 사건 단일 기준과 규칙/기록전 설정 잠금은 Unreal 통합 뒤에 검사한다.

세부 기능 ID는 다음 범위를 참조한다.

| 분야 | 연결 ID | 이번 코어의 범위 |
|---|---|---|
| RUL | RUL-02, RUL-03, RUL-04, RUL-06, RUL-13 | 경기 단계·완전 통과·역방향·오프사이드·종료 시점의 **결과**만 소비 |
| ARN | ARN-01, ARN-02 | Class40 경기장/골대 프로필과 실제 골대 좌표 판정은 `MatchRules`와 경기장 구현이 담당 |
| AUD | AUD-04, AUD-05, AUD-06 | 준비·시작·종료/득점 안내, 개별 음소거의 큐 ID만 제공 |
| UX | UX-02, UX-08, UX-09, UX-11 | HUD 우선순위, 무음 시 시각 대체, 감소 효과, 간소 화면 경고의 입력 상태 제공 |

따라서 본 코어는 심판 승인·정정, 공격수 승계, 페널티 경기, 실제 음원 재생,
실제 골대 발광 렌더를 구현하거나 규정 적합성을 새로 판정하지 않는다.

## 입력과 상태 기계

`ReadSnapshot(const drone_soccer::MatchRules&)`는 매 프레임 다음을 복사한다.

- `Phase`, `CurrentSet`, `RemainingS`
- 양 팀의 `Scores`, `Offside`, `Penalties`, `SetWins`
- 호출자가 소유한 단조 증가 `ResetEpoch`

`Generator::Observe`의 첫 유효 스냅샷은 조용한 기준선으로만 저장한다.
첫 관측보다 과거에 지나간 점수나 카운트다운을 나중에 재생하지 않는다.
각 뒤의 유효 스냅샷은 아래 순서로 처리한다.

1. 비유한 시간, 음수/범위 밖 카운터, 알 수 없는 단계, 모순된 단계/시간을 거부한다.
2. `ResetEpoch`가 달라지면 새 기준선을 잡고 큐 없이 붉은 테두리를 즉시 내린다.
3. 점수가 감소하면 리셋으로 취급해 기준선을 새로 잡고 큐를 만들지 않는다.
4. 점수가 증가하면 팀 인덱스를 지닌 골 점멸(감소 효과면 고정 강조)을 증가분마다 하나씩 만든다.
5. 이전 Playing 스냅샷에서 지나간 정수 초의 카운트다운, 단계 전환 알림을 만든다.
6. 결과 프레임의 붉은 테두리는 Playing이고 `0 < RemainingS <= 10`일 때만 활성화한다.

동일 팀 점수가 한 프레임에 둘 이상 증가하면 `ScoreJumpAnomaly=true`로 표시한다.
그 경우에도 증가분마다 한 큐를 넣어 승인 사건 수를 숨기지 않는다.
점수 감소/epoch 변경은 오래된 점수나 이전 경기의 연출이 새 경기에서 재생되는 일을 막는다.

단계 전환 큐는 `NoticeReady`, 첫 세트의 `NoticeMatchStart`, 이후 세트의
`NoticeSetStart`, `NoticeSetEndBreak`, `NoticeMatchFinished`다.
관측하지 못한 중간 단계는 스냅샷만으로 복원하지 않는다. 예를 들어 매우 큰 dt가
여러 단계를 건너뛰면 관측된 최종 Finished 알림만 만들며, Unreal 통합 시 필요하면
규칙 코어의 사건 이력으로 보완해야 한다.

## 카운트다운·효과 정책

카운트다운 ID는 10부터 1까지다. 이전 Playing 잔여 시간이 해당 정수 이상이고
이번 잔여 시간이 이하로 내려간 모든 ID를 큰 수부터 작은 수 순서로 한 번씩 낸다.
20/60/240 FPS처럼 dt가 달라도, 10.5초에서 0.9초로 뛰어도 10~1을 빠뜨리지 않는다.
Playing에서 Break 또는 Finished로 나가면 끝값을 0초로 간주하여 마지막 ID를 보존한다.
같은 시간 또는 시간이 되돌아간 관측은 이미 낸 비트 마스크 때문에 중복 음향을 만들지 않는다.

`Mute=true`는 `CountdownAudio`를 `CountdownVisual`로 바꾼다.
따라서 무음이 경고 자체의 소거가 되지 않으며 UX-08의 시각 대체 재료가 남는다.
`ReducedEffects=true`는 골 `GoalFlash`를 `GoalSteadyHighlight`로 바꾼다.
둘 다 renderer·audio asset·사용자 설정 저장은 호출 측의 책임이다.

## 파라미터와 출처

| 항목 | 값/정책 | 출처와 성격 |
|---|---|---|
| Ready/Playing/Break | 10초 / 180초 / 300초 | `docs/rules-sources-2026.md`의 FIDA §§7~11 및 `RuleProfileFIDA40` |
| 세트·승수 상한 | 최대 3세트, 2승 선취 | `docs/rules-sources-2026.md` FIDA §§7~11 |
| 승인 득점 입력 | `MatchRules` 점수 증가만 사용 | FIDA 전체 기체의 상대 골대 앞→뒤 통과, 역방향 무득점·오프사이드: 규정 근거 문서 30~35행 |
| 마지막 경고 구간 | Playing에서 0초 초과 10초 이하 | 피드백 9/계획 8절의 연출 요구; **시뮬레이터 선택값** |
| 기본 골 점멸 | 0.75초 | **시뮬레이터 선택값**; 규정상 광량/지속시간이 아님 |
| 허용 효과 길이 | 0초 초과, 10초 이하 | HUD 안전 상한의 **시뮬레이터 선택값** |
| 표시 점수·페널티 상한 | 팀별 99 | 고정 배열 안전 경계의 **시뮬레이터 선택값**; 공식 득점 상한이 아님 |
| 한 프레임 큐 상한 | 256개 | 다중 점프를 보존하기 위한 **시뮬레이터 선택값** |

`MatchRules`의 골대 좌표·구형 보호구 여유·전체 통과는 규정 코어가 처리한다.
이 헤더는 그 기하를 계산하지 않으므로 ARN-02의 시각 메시/충돌 영역 대조도 하지 않는다.

## 테스트와 실제 결과

`Tests/match_cues_tests.cpp`는 실제 `DroneSoccerRules.cpp`와 링크해 다음을 검사했다.

- 파란/빨간 승인 골의 정확히 한 팀색 큐와 중복 방지
- 역방향 통과, 부분 통과/되돌아 나옴, 오프사이드 반복의 무큐 처리
- 다점프 anomaly와 증가분별 큐, 점수 감소/epoch 리셋의 무큐 처리
- Ready·경기/세트 시작·세트 종료/정비·최종 종료 알림
- 10초 붉은 테두리의 경계와 Break 즉시 제거
- 20/60/240 FPS 및 큰 dt 점프에서 10~1의 일회성·내림차순 큐
- mute/reduced-effects, 비유한값·범위 밖·모순 상태 거부

실제 연결 실행의 최종 g++ 출력 줄은 **`50 checks, 0 failures`** 이다.
새 테스트 TU는 `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror`로 통과했다.
단, 사용자가 지정한 단일 결합 명령은 기존 `DroneSoccerRules.cpp`의 다수 `-Wsign-conversion`
경고가 `-Werror`로 승격되어 이 Linux 환경에서 컴파일 전에 중단됐다.
기존 파일 수정은 이 작업 단위에서 금지되어, 실제 실행 연결 때만 그 기존 경고를 비오류로 낮췄다.
이는 새 헤더/테스트의 경고 없음이나 규칙 코어의 MSVC 결과를 대신하지 않는다.

`/tmp/build-matchcues/inc.cpp`의 `-fno-exceptions -fno-rtti` 헤더 포함 검사는 별도로 수행한다.
Windows MSVC/Unreal Engine 5.8은 이 Linux 샌드박스에 없어 실행하지 않았다.
`Tests/run-match-cues-tests.ps1`은 Windows에서 `/std:c++17 /EHsc /W4 /WX /O2 /MT`로
`DroneSoccerRules.cpp`와 이 테스트를 함께 빌드하도록 작성했지만 여기서는 실행하지 않았다.

## 명시적 한계

- Unreal 통합, Unreal 빌드, 패키지 실행본, HUD/골대 발광/음향 asset 재생은 미완료다.
- 실물 Boxer 또는 다른 물리 조종기, 다른 PC/GPU/Windows 사용자 시험은 미완료다.
- 사람 플레이테스트·가독성/광과민성 검토·실제 경기 운영 검증은 미완료다.
- LAN/온라인 동기화, 서버 권위 사건 순서, 실제 네트워크 시험은 미완료다.
- 이 코어의 0.75초·10초 효과 상한·99 점수 상한은 실측값이나 공식 규정값이 아니다.

## 다음 Unreal 통합 단계

1. `ImperialMatchManager.h`에 `ImperialMatchCues::Generator`와 세션별 `ResetEpoch`을 소유하도록 추가한다.
2. `ImperialMatchManager.cpp`의 `ResetMatch`, `StartMatch`, `UpdateScoring`에서 epoch를 명시적으로 증가시키고,
   `Rules.Update(...)` 직후 `ReadSnapshot(Rules)`/`Observe(...)`를 프레임당 한 번 호출한다.
3. `AImperialMatchManager::HandleMatchPhaseTransition`의 기존 안전/착륙 잠금은 유지하며,
   `FrameResult`의 알림을 기존 `Log`/HUD 상태와 중복 없이 매핑한다.
4. `ImperialHUD.h/.cpp`에서 팀 인덱스의 골대 팀색, 붉은 10초 테두리, 무음 시각 카운트다운,
   감소 효과의 고정 강조를 렌더하고 색 외 텍스트/숫자 식별도 제공한다.
5. 실제 골대 actor와 음향 구성요소에는 오직 이 큐를 소비하는 어댑터를 두고, 그 어댑터가
   `DroneSoccerRules`의 기하·점수를 다시 판단하지 않도록 한다.
6. 통합 뒤에는 MSVC runner, UE Editor/Shipping, 20/60/240 FPS, Ready/Break/Finished/reset,
   음소거·감소 효과·HUD 가독성을 별도 증거로 재검증한다.
