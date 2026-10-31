# LAN·온라인 권한 세션 코어 — 2026-10-31
## 목적과 계획 연결
이 문서는 `NetSessionCore.h`의 엔진·전송 수단 독립 권한 세션 코어를 기록한다.
소켓, Unreal 복제, NAT, 서버 프로세스 또는 온라인 방을 구현한 문서가 아니다.

- 사용자 20개 작업 묶음 **18번(LAN·온라인)**의 선행 코어 계약이다.
- 이 묶음은 통합 개발 계획에서 현재 `미구현: 접속·서버`로 남아 있다.
- 이 작업은 피드백 **12번**의 탑건 방 기록·온라인 기록방 후속 범위와도 연결된다.
- 단계 표의 **H(LAN)**: 입력 소유권·서버 판정·정상/손실/재접속 계약을 먼저 분리한다.
- 이후 I(온라인)의 비공개 원격 경기·기록방·서버 운영을 완료한 것이 아니다.
- 연결 기능 ID는 `LOC-05~08`, `NET-01~03`, `NET-08`, `NET-10~12`,
  `NET-15`, `LOB-01`, `LOB-03`, `LOB-08~10`이다.
- 위 ID 전체를 구현완료로 바꾸지 않는다. 이 코어는 해당 완료 조건의 일부 계약만 제공한다.

통합 개발 계획서 10절은 서버가 기체 소유권·득점·시간·기권·재접속 상태를 결정하고,
클라이언트 화면 시간과 좌표를 신뢰하지 말 것을 요구한다.
같은 절의 RTT 50/100/150 ms와 손실 1/3/5%는 이 문서의 결정적 모의 전송 검사에 사용했다.
이는 실제 RTT 또는 실제 패킷 손실의 측정값이 아니다.
## 범위와 데이터 모델
`ImperialNet::AuthoritativeSession`은 표준 C++17 헤더 전용 상태기계다.
전역 가변 상태, I/O, 스레드, 예외, RTTI, Unreal 형식과 네트워크 API를 사용하지 않는다.
기체 좌석은 고정 **10개**이고 클라이언트는 고정 최대 16명이며 사건 이력은 128개다.
10좌석은 FIDA 팀당 5명이라는 `rules-sources-2026.md`의 **§3-1**을 양 팀에 적용한
시뮬레이터 코어 상한이다. 접속 인원·서버 용량의 공식 규정값은 아니다.

클라이언트 ID와 역할(Player, Referee, Administrator)은 전송 계층이 인증한 뒤 전달해야 한다.
이 코어는 인증·계정·초대 코드·암호화·권한 발급을 대신하지 않는다.
플레이어만 빈 좌석을 요청할 수 있고, 이미 소유자가 있는 좌석은 빼앗을 수 없다.
소유자 자신, 심판 또는 관리자의 명시적 해제만 좌석을 비운다.

입력 패킷은 좌석 번호, 0이 아닌 순번, 클라이언트 시각(S), 정규화 Roll/Pitch/Yaw,
Throttle, ARM/DISARM 버튼 상태로 구성한다.
서버 시각은 단조 증가만 허용하며 비유한값·음수·범위 밖 값은 거부한다.
비소유자, 재점유 전 재접속자, 중복/역순 순번, 너무 과거/미래의 시각, 비유한값과
정규화 범위 밖 명령은 `Rejection` 열거값으로 명시적으로 거부한다.
거부된 입력은 적용 명령·순번·ARM 상태를 변경하지 않는다.
## 좌석·재접속 상태기계
```text
Unowned
  └─ 서버 승인 Claim ─> AwaitingInput
AwaitingInput ─ 승인 입력 ─> ReceivingInput
ReceivingInput ─ timeout ─> InputLost
ReceivingInput ─ Leave ─> ReservedForReconnect
ReservedForReconnect ─ Reconnect + explicit Reclaim ─> AwaitingInput
모든 소유 상태 ─ 소유자/심판/관리자 Release ─> Unowned
```

`AwaitingInput`은 ARM을 허용하는 상태가 아니라, 새 ARM 누름을 관찰할 준비 상태다.
Claim, timeout, Leave, Reclaim은 항상 DISARM하고 먼저 ARM 버튼의 해제를 본다.
그 뒤의 release→press 상승 에지에만 ARM이 된다.
따라서 재접속 패킷의 `ArmButtonDown=true`나 입력 손실 전의 ARM 상태는 자동 복원되지 않는다.
재접속자는 먼저 `ReconnectClient`, 그 다음 `ReclaimSeatOwnership`을 성공해야 입력을 보낼 수 있다.

입력 타임아웃 시 기본 `NeutralAndDisarm`은 명령을 중립으로 바꾸고 DISARM한다.
선택 가능한 `HoldLastAndDisarm`도 마지막 명령 값만 유지할 뿐 반드시 DISARM한다.
두 정책 모두 다음 비행을 자동으로 ARM하지 않는 안전 계약이다.

## 서버 사건과 동기화

서버만 `Goal`, `SetEnded`, `Forfeit`, `Disconnect` 사건을 `PublishServerEvent`로 추가한다.
각 사건에는 단조 증가 `EventId`, 서버 시각(S), 팀/좌석/클라이언트 식별자를 기록한다.
`LeaveClient`도 서버 시각의 Disconnect 사건을 같은 로그에 기록한다.
클라이언트는 마지막 확인 ID를 ACK하고, 참가·재접속 후 `GetEventCatchup`으로 이후 사건을 받는다.
128개 보존 범위를 지난 ACK는 조용히 누락시키지 않고 `EventHistoryExpired`로 거부한다.
그 경우 전송/Unreal 통합 계층은 권한 있는 전체 스냅샷을 별도 설계해야 한다.

이 코어는 득점의 공식 적법성, 세트 결과, 기권 사유를 판정하지 않는다.
FIDA 득점·오프사이드·세트 규칙은 `rules-sources-2026.md`의 **§§7~11** 및
기존 `DroneSoccerRules`가 담당해야 하며, 사건 로그는 그 서버 판정 결과만 운반한다.

## 파라미터와 출처

| 항목 | 값/범위 | 출처와 의미 |
|---|---:|---|
| 기체 좌석 | 10 | FIDA 팀당 5명, `§3-1`을 양 팀에 적용한 코어 상한 |
| 기본 입력 timeout | 0.250 S | **시뮬레이터 선택값**, 실측 USB/네트워크 지연이 아님 |
| 미래 허용창 | 0.250 S | **시뮬레이터 선택값**, 서버 시계 오염 방지용 |
| 과거 허용창 | 2.000 S | **시뮬레이터 선택값**, 재전송·지연 처리 초안 |
| 설정 허용 timeout | 0.001~60 S | **가정값**, 잘못된 구성 거부 범위 |
| 입력 채널 | Roll/Pitch/Yaw -1~1, Throttle 0~1 | **가정값**, 전송 정규화 범위이며 조종기 실측 범위가 아님 |
| 사건 이력 | 128개 | **시뮬레이터 선택값**, 초과 시 전체 스냅샷 설계 필요 |
| 모의 RTT/손실 | 50/100/150 ms, 1/3/5% | 계획서 10절의 시험 조건; 실제 측정값 아님 |

## 검사 범위

`Tests/net_session_core_tests.cpp`는 일반 흐름, 경계, 잘못된 입력과 적대적 입력을 검사한다.
포함 범위는 중복/역순 순번, 비유한·범위 밖 입력, 과거/미래 시각, 시간 역행,
비소유자 입력, 좌석 탈취, 심판/관리자 해제, timeout failsafe, 재접속의 새 ARM 누름,
단조 사건 ID, ACK 역행, 늦은 참가 catch-up과 이력 만료다.

결정적 LCG 의사난수 전송 모의는 `std::random_device`를 사용하지 않는다.
세 RTT와 세 손실률의 **3×3, 9개 조합**에서 세 클라이언트가 동일한 9개 사건 로그로 수렴하는지,
그리고 비소유자 입력이 한 번도 적용되지 않는지를 검사했다.
이는 메모리 안의 모의 전송이며 실제 패킷, 실제 LAN, 다른 PC 또는 서버의 결과가 아니다.

최종 Linux 명령은 다음과 같다.

```text
mkdir -p /tmp/build-netsession && g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror -ISource/ImperialWingsSimulator Tests/net_session_core_tests.cpp -o /tmp/build-netsession/t && /tmp/build-netsession/t
```

실제 최종 출력 줄은 **`164 checks, 0 failures`**이다.
추가로 `NetSessionCore.h`만 포함한 번역 단위를 `-fno-exceptions -fno-rtti`와 같은 경고 오류 옵션으로 컴파일했다.
MSVC와 Unreal Engine 5.8 빌드·실행은 이 Linux 환경에서 수행하지 않았다.
`Tests/run-net-session-core-tests.ps1`는 Windows에서 `/std:c++17 /EHsc /W4 /WX /O2 /MT`로 실행할 준비만 되어 있으며 여기서는 실행하지 못했다.

## 명시적 한계

- Unreal 통합, Unreal 빌드, Unreal 실행본 검증은 미완료다.
- 실제 소켓·패킷 직렬화·Unreal 복제·전용 서버·LAN 방·온라인 방은 구현하지 않았다.
- 실제 다른 PC, NAT, 릴레이/호스팅, 인터넷 회선, 서버 장애와 실제 네트워크 지연/손실은 미구현·미검증이다.
- 실물 Boxer/다른 조종기, USB 재연결 비행, 사람 조종, 사람 경기·사용성 검증은 미완료다.
- 실제 FIDA 대회 서버 적합성, 심판 판정·점수·시간·기권의 완전한 권한 통합도 미완료다.
- 128개를 넘는 지연 클라이언트의 전체 스냅샷·저장·복구와 계정 보안은 후속 설계다.

## 다음 Unreal 통합 절차

1. `ImperialMatchManager.h/.cpp`에 서버 전용 `ImperialNet::AuthoritativeSession`을 보유하고,
   `BeginPlay`, `Tick`, `UpdateScoring`, `HandleMatchPhaseTransition`에서 단조 시각과
   `DroneSoccerRules`의 승인 득점·세트 종료·기권 결과만 사건으로 발행한다.
2. `ImperialPlayerController.h/.cpp` 또는 새 서버 RPC 경로에서 인증된 ClientId/역할을 연결하고,
   모든 원격 입력을 먼저 `SubmitInput`으로 검증한다. 거부 이유는 HUD에 표시하되 클라이언트가
   소유권·점수·사건 ID를 직접 확정하지 못하게 한다.
3. `SoccerDronePawn.h/.cpp`에는 승인된 `SeatSnapshot`만 ARM/입력 명령으로 변환하는 서버 경로를
   추가한다. `InputLost`, Leave, Reclaim에서는 `SetArmed(false)`를 먼저 호출하고 새 ARM 에지를 요구한다.
4. Unreal 전송 계층에서 사건 ACK/catch-up과 이력 만료 전체 스냅샷을 구현한 뒤,
   `LOC-05~08`의 실제 두 PC LAN 검사와 계획서 RTT/손실 조건의 실제 측정을 별도로 기록한다.
5. 그 다음에만 `LOB-01~10`의 방·권한·NAT/호스팅·원격 회선·서버 운영과 탑건 기록방을 단계 I에서 검증한다.

현재 상태 제안: 핵심 권한 상태 로직 구현·네이티브 검사 통과(g++), Unreal 통합·실행·실제 LAN/온라인 검증 대기.
