# 연습 중력 조절 코어 — 2026-10-31
> 상태: 엔진 독립 헤더 코어와 g++ 단위 검사는 작성·실행했으며, Unreal 연결·빌드·실행본 검증은 아직 수행하지 않았다.
## 목적과 계획 연결
이 문서는 `PracticeGravity.h`의 연습 전용 중력 설정 코어를 설명한다.
대상은 사용자 미완료 작업 묶음 **12번 중력 조절**이다.
통합 개발 계획서 8절의 `중력 설정` 행과 직후 문단을 따른다.
화면 숫자나 기체별 임시 보정만 바꾸지 않고, 실제 중력 적용 경로를 바꾸는 것이 후속 통합 목표다.
피드백 **8번(Freerider 참고 중력 조절)**의 m/s² 표시·기본값·물리/훈련 제어 일관 적용 요구에 연결한다.
Freerider는 사용 흐름 참고 대상이며, 이 구현이 해당 제품과 같다는 주장은 하지 않는다.
20개 묶음 표의 상태는 여전히 `미구현: 중력 설정`이다.
단계는 **E 경기 연출·설정**이며, 점수 사건의 단일 기준과 규칙/기록전 설정 잠금은 Unreal 통합 뒤에 검사한다.
상세 기능 명세의 `TRN-09`(조건이 다른 성적 비교 차단), `PHY-01`(중력·모터 힘·모멘트), `PHY-12`(물리 프리셋·측정 근거)에 연결한다.
## 설계와 상태 기계
`ImperialGravity`는 Unreal 헤더나 형식을 포함하지 않는 C++17 헤더 전용 이름공간이다.
`PracticeGravity`는 현재 적용값과 아직 적용하지 않은 보류값을 분리해 보관한다.
`ChangeContext`는 `Practice`, `OfficialMatch`, `OfficialTopgun` 및 전 기체 DISARM·착륙 상태를 입력으로 받는다.
상태 전이는 `Idle → Selected → Review → Idle`이다.
`Select`는 후보를 검증해 보류값만 설정하므로 선택만으로 비행 중 중력이 바뀌지 않는다.
`Review`는 안전 문맥을 다시 검사하고 사용자의 검토 상태를 표시한다.
`Apply`는 검토 상태에서만 가능하며, 적용 순간에도 모드·전 기체 DISARM·착륙을 재검사한다.
적용 중 거부되면 상태는 `Review`에 남고 이전 중력값도 보존된다.
`Cancel`은 보류값을 현재 적용값으로 되돌리고 현재 적용값을 절대 바꾸지 않는다.
`Idle` 적용은 `NoSelection`, 검토 전 적용은 `ReviewRequired`, 검토 중 재선택은 `AlreadyReviewing`으로 거부한다.
비유한값, 범위 초과, 공식 모드, ARM, 미착륙, 순서 위반, 보조 계산 오류는 예외가 아닌 `Rejection` 열거형으로 구분한다.
## 모드와 기록 조건

중력 변경 절차는 `Practice`에서만 허용된다.

전 기체가 DISARM이고 착륙했다고 호출자가 제공한 경우에만 선택·검토·적용을 통과한다.

`OfficialMatch`와 `OfficialTopgun`에서 g0 이외의 중력 후보는 `OfficialGravityLocked`로 거부한다.

두 공식 기록은 g0만 허용하도록 `ClassifyRecord`가 분리한다.

g0 이외의 연습 기록은 `NonStandardPracticeGravity` 조건 태그를 가져 표준 조건 기록과 섞이지 않는다.

g0 연습 기록은 `StandardGravity` 태그를 유지하지만, 그것만으로 공식 경기 기록임을 뜻하지 않는다.

## 값과 출처

| 값 또는 제한 | 값 | 출처·의미 |
|---|---:|---|
| 표준 중력 `g0` | 9.80665 m/s² | SI 표준값이다. FIDA 중력 규정 조항은 `rules-sources-2026.md`에 없다. |
| 연습 허용 범위 | 0.5g0~1.5g0 | **시뮬레이터 선택값**: 4.903325~14.709975 m/s². |
| 프리셋 | 0.5g0, 1.0g0, 1.5g0 | **시뮬레이터 선택값**. |
| Unreal 변환 | `GlobalGravityZ = -g × 100` cm/s² | Unreal cm 단위와 아래 방향 Z 부호의 변환이다. |
| 기본 질량 | 0.9 kg | `model-reference.md`의 기존 기체 **가정값**이며 실측 STRIKER 질량이 아니다. |
| 질량 상한 | 1.10 kg | FIDA Class40 **§2-1**의 비행 중 총중량 상한에서 유래한다. |
| 기본 최대 총추력 | 32 N | `PhysicsModels.h`의 로터당 8 N 기본값 4개의 **시뮬레이터 가정값**이다. |
| 추력/중량 경고 | T/W < 1.5 | **시뮬레이터 선택값**이며 공식 적합성·안전 인증 기준이 아니다. |
| 자유낙하 입력 한계 | 거리 0~100 m, 시간 0~60 s | 잘못된 UI·기록 입력을 거부하는 **시뮬레이터 선택값**이다. |

자유낙하 도우미는 초기 수직 속도 0, 항력·양력·표면효과·접촉 없음이라는 해석식 **가정값**이다.

따라서 Chaos 접촉, 실제 비행 궤적 또는 실기체 낙하 측정값이 아니다.

`HoverThrustN`은 수평 자세·수직가속도 0의 무게만 계산하며 모터 손실과 배터리 제한을 포함하지 않는다.

기존 0.9 kg·32 N·g0 가정에서 무게는 8.825985 N, T/W는 약 3.62565765, 정적 여유는 23.174015 N이다.

이는 기존 모델 상수를 사용한 코어 계산일 뿐 실측 추력여유가 아니다.

## 검증 범위

저장소 루트에서 실행한 최종 명령은 다음과 같다.

```text
mkdir -p /tmp/build-gravity && g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror -ISource/ImperialWingsSimulator Tests/practice_gravity_tests.cpp -o /tmp/build-gravity/t && /tmp/build-gravity/t
```

실제 최종 출력 줄은 **`60 checks, 0 failures`**이다.

검사는 기본 g0·세 프리셋·선택/검토/명시 적용/취소와 순서 위반을 다룬다.

적용 직전 ARM 또는 미착륙으로 바뀐 경우 이전 값이 유지되는지도 확인한다.

연습·공식 경기·공식 Topgun의 잠금과 표준/비표준 기록 조건 분리를 포함한다.

하한·상한, 범위 밖, NaN·무한대 중력, 질량·추력·경고 한계, 자유낙하 입력의 적대 사례를 검사한다.

Unreal cm/s² 변환, 호버 추력, 자유낙하 왕복, 기존 0.9 kg·32 N 추력여유도 검사한다.

`-fno-exceptions -fno-rtti`와 같은 경고 정책에서 `PracticeGravity.h`만 포함한 최소 번역 단위 컴파일도 통과했다.

`Tests/run-practice-gravity-tests.ps1`는 작성했지만 Windows/MSVC 전용이므로 이 Linux sandbox에서는 실행하지 않았다.

Unreal Engine 5.8·MSVC가 없어 Unreal 컴파일, 패키지, 실행본 검사는 수행하지 않았다.

## 후속 Unreal 통합 단계

1. `ImperialMatchManager.h/.cpp`에 `ImperialGravity::PracticeGravity` 인스턴스와 `ChangeContext` 생성 함수를 추가한다.
2. 문맥은 `drone_soccer::Phase::Practice` 및 `AImperialMatchManager::Drones` 전체의 `IsArmed()`·명시적 착륙 판정으로 만든다.
3. UI는 기존 설정 패턴처럼 `Select → Review → Apply/Cancel`만 호출하고 `Rejection`을 한국어 안내로 표시한다.
4. `Apply` 성공 시 `GetWorld()->GetWorldSettings()`의 `WorldGravityZ`에 `ToUnrealGlobalGravityZCmps2` 결과를 적용하고 Chaos 월드가 새 전역 중력을 쓰게 갱신한다.
5. `ASoccerDronePawn.cpp`의 `PhysicsStep` 885행과 `CalculateAutonomousRate` 917~918행은 이미 `GetWorld()->GetGravityZ()`를 읽으므로 월드 적용 뒤 g0 상수로 되돌리지 않는다.
6. `ImperialMatchManager.cpp`의 `AngleSmokeTick` 610행 하드코딩 `9.81`은 현재 적용 중력값으로 바꿔 검증용 수직 피드포워드도 같은 조건을 쓰게 한다.
7. F6 `RecordFlightSample` JSONL 루트에 `gravity_mps2`, `global_gravity_z_cmps2`, `gravity_record_condition`을 기록한다.
8. `AIPracticeResult.h`의 `FImperialAIResult`와 `ImperialAIResults.cpp`의 parse/serialize/snapshot/save/load에 같은 값·태그를 버전 호환 방식으로 추가한다.
9. 향후 Topgun 기록 모델에도 g0 잠금과 동일 조건 필드를 넣고, 비표준 연습 기록을 공식 표준 기록과 비교·순위 집계하지 않는다.
10. 통합 뒤 실제 자유낙하·호버·AI 이동·기록 저장·공식 모드 거부를 Unreal 실행본에서 다시 검사한다.

## 명시적 한계

Unreal 통합, Unreal 빌드, 패키지 실행본 검증은 아직 하지 않았다.

실물 Boxer 또는 다른 물리 조종기 검증은 하지 않았다.

다른 PC·GPU·Windows 사용자 검증은 하지 않았다.

사람 조종·사람 플레이테스트·훈련 체감 평가는 하지 않았다.

실제 네트워크·LAN·온라인 기록 서비스 검증은 하지 않았다.

중력 범위·프리셋·경고 임계값은 시뮬레이터 선택값이며 FIDA 공식 중력 규정으로 제시하지 않는다.

현재 0.9 kg·모터 추력·항력·배터리 값은 실측 보정이 아니라 기존 모델의 가정값이다.
