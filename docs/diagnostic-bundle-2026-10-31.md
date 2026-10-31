# 로컬 진단 ZIP 도구 — 2026-10-31

## 목적과 계획 연결

`Scripts/make-diagnostic-bundle.py`는 Saved에 이미 기록된 텍스트 자료를 사용자가 검토한 뒤 로컬 ZIP으로 묶는 보조 CLI다. 게임 실행 중 조종·물리·ARM 상태를 제어하지 않으며, 네트워크 접근 코드도 없다.

| 계획 연결 | 이 단위의 관계 | 상태 해석 |
|---|---|---|
| 20개 작업 묶음 **8** | 훈련 목표·기록 비교의 보조 진단 자료 | 비교 UI·사람 기록 완료를 뜻하지 않음 |
| 피드백 **12** | 탑건 로컬 기록 문제를 전달할 수 있는 자료 준비 | S코스·닉네임/ID/국적을 구현하지 않음 |
| 단계 **C 탑건 로컬** | 로컬 기록의 저장·문제 확인을 돕는 선행 보조 | C 단계 출구 기준 충족으로 표시하지 않음 |
| 마스터 계획 10절 마지막 단락 | F5/F6, 메모, 로컬 ZIP, 검토 후 공유, 자동 전송 금지 | 본 도구의 직접 요구 근거 |
| DAT-01, DAT-03, DAT-13 | 기존 F5/F6 기록을 선택적으로 읽는 기반 | 기록 형식이나 수집 동의를 바꾸지 않음 |
| DAT-02, DAT-05, DAT-06, DAT-14 | ZIP·메모·manifest·기록된 빌드 식별자 | 사건 북마크/스키마 전체는 후속 |
| DAT-07, DAT-09, DAT-10, DAT-12 | 개인정보 제외·용량·저장 실패·피드백 처리 원칙 | 보관 UI·프레임 측정·처리 이력은 후속 |
| OPS-02, OPS-09, DOC-07 | 비밀값 제외·허용 목록·안전한 내보내기·오프라인 피드백 | 서버 전송·첨부 업로드는 하지 않음 |

묶음 8·피드백 12·단계 C는 기록 흐름의 연결 지점일 뿐이다. 이 단위는 해당 묶음·피드백·단계의 완료 상태를 승계하지 않는다. 직접 기능 식별자는 DAT-02와 DAT-07이며 DAT-10·OPS-09의 저장 실패·크기·경로 안전 원칙을 함께 적용한다.

## 실제 저장 대상과 허용 목록

`ImperialUserData::Directory()`는 패키지 실행에서 `%LOCALAPPDATA%\ImperialWingsSimulator\Saved`를 사용자 데이터 루트로 쓴다. Editor와 명시적 QA는 별도 Saved 루트를 쓸 수 있어 CLI의 `--saved-root`를 사용자가 직접 지정할 수 있다.

| Saved 상대 경로 | 실제 생성 코드와 파일 형식 | 포함 이유 |
|---|---|---|
| `FlightLogs/*.jsonl` | `AImperialMatchManager::ToggleFlightRecording()`의 F6 기록 | F6 비행 기록 JSONL |
| `Reports/*.csv` | `AImperialMatchManager::ExportTelemetry()`의 F5 내보내기 | F5 상태 CSV |
| `TrainingResults/AI/*.json` | `AImperialMatchManager::SaveAIResult()` | 저장된 AI 연습 결과 |
| `Diagnostics/*.{json,jsonl,csv,log,txt}` | 장치 보고·비행 trace 등 진단 출력 | 명시적 텍스트 진단 자료 |

`Config/RadioProfile.json`, `Config/PilotControls.json`, `Config/PilotResponse.json` 및 보정/프로필 이름은 항상 제외한다. `.env`, credential, token, secret, API key, private key 계열도 제외한다. 허용 목록 밖의 위치·확장자, 바이너리, 심볼릭 링크, 읽지 못한 파일도 제외하므로 Saved 전체나 개인 조종기 보정을 복사하지 않는다.

## 사용 흐름과 상태 전이

기본 실행은 ZIP을 만들지 않는 dry-run이다. 포함·제외한 모든 항목의 Saved 상대 경로, 원본/번들 바이트 수, 이유와 파일별 정제 횟수를 출력한다.

```text
검증된 Saved 루트
  → 심볼릭 링크를 따라가지 않는 열거
  → 허용 위치·이름·확장자 판정
  → 크기/UTF-8/바이너리 판정
  → 텍스트 정제와 포함 목록 생성
  → dry-run 검토 출력
  → --confirm이 있을 때만 임시 ZIP 작성·원자 교체
```

```powershell
python Scripts/make-diagnostic-bundle.py --saved-root "$env:LOCALAPPDATA\ImperialWingsSimulator\Saved"
python Scripts/make-diagnostic-bundle.py --saved-root "$env:LOCALAPPDATA\ImperialWingsSimulator\Saved" --output "$env:USERPROFILE\Desktop\ImperialWings-diagnostic.zip" --memo "F6 약 18초에서 입력이 멈춘 것처럼 보임" --confirm
```

첫 명령은 파일을 만들지 않고, 두 번째 명령만 명시적인 `--confirm` 뒤에 ZIP을 만든다. 출력 ZIP은 Saved 루트 바깥이어야 하며, 새 ZIP이 다음 스캔에 포함되거나 archive path가 Saved 밖을 가리키는 일을 막는다. 오류가 나면 임시 출력만 정리하고 원본 Saved 파일은 수정·삭제하지 않는다. 이 오프라인 도구의 실패는 게임 프로세스, ARM/DISARM, 조종 입력 또는 비행 기록을 멈추지 않는다.

## 개인정보 정제와 manifest

포함 가능한 텍스트는 UTF-8로 읽고 새 UTF-8 바이트열로 다시 쓴다. Windows `C:\Users\<name>\` 경로, 환경의 `USERNAME`/`USER`/`LOGNAME`, 전자우편 주소, GUID형 장치 ID, HID VID/PID 직렬 문자열 및 IPv4 주소를 정제한다. IPv4 정제는 개인정보 최소화를 위한 **시뮬레이터 선택값**이다.

`manifest.json`은 ZIP 첫 항목이며 도구 버전·schema·네트워크 미접근 상태, 포함 파일의 상대 경로·선택 이유·원본/정제 후 크기·정제 후 SHA-256·정제 횟수를 기록한다. 선택 메모도 정제 후 저장하며 `Diagnostics/build-fingerprint.json` 등 알려진 후보에 안전 필드가 있으면 선택적 build fingerprint를 넣는다.

manifest의 SHA-256은 원본이 아니라 **정제 후 ZIP에 넣은 바이트**의 해시다. 사용자는 dry-run 목록과 manifest를 다시 확인한 뒤에만 외부 채팅·메일 등으로 수동 전달해야 한다. 도구에는 HTTP, 소켓, 업로드, 자동 메일 코드가 없고 외부 자동 전송을 수행하지 않는다.

## 파라미터와 출처

| 항목 | 값 | 출처와 의미 |
|---|---:|---|
| 단일 텍스트 파일 상한 | 5 MiB | **시뮬레이터 선택값**; 과도한 진단 파일을 건너뜀 |
| 총 포함 바이트 상한 | 32 MiB | **시뮬레이터 선택값**; 목록 순서대로 초과 항목 제외 |
| ZIP member 시각 | 1980-01-01 00:00:00 | **시뮬레이터 선택값**; 같은 입력의 결정적 ZIP을 돕는 고정 시각 |
| 압축 순서 | `manifest.json` 뒤 상대 경로 대소문자 비구분 정렬 | **시뮬레이터 선택값**; 검토·재현성 목적 |
| 수집 허용 위치 | FlightLogs, Reports, TrainingResults/AI, Diagnostics | 실제 저장 코드와 DAT-02/OPS-09 허용 목록 원칙 |
| 제외 항목 | 보정·전체 경로·장치 고유값·비밀값 | 마스터 계획 10절 마지막 단락 및 DAT-07 |

이 단위는 FIDA 규칙 수치를 사용하거나 새 경기 규칙을 만들지 않는다. 파일 상한·정제 범위·고정 ZIP 시각은 모두 공식 규정값이 아닌 **시뮬레이터 선택값**이며 `docs/rules-sources-2026.md` 조항을 주장하지 않는다.

## 네이티브 Python 검사

저장소 루트에서 Python 표준 라이브러리만 사용해 다음 명령을 실행했다. 바이트코드 캐시는 저장소 밖으로 유지했다.

```text
mkdir -p /tmp/build-diagbundle && PYTHONDONTWRITEBYTECODE=1 python3 Tests/test-diagnostic-bundle.py
```

실제 최종 출력 줄은 **`8 checks, 0 failures`** 이다. 합성 임시 Saved 트리에서 허용 목록/제외 이유, 경로·사용자명·전자우편·GUID·HID·IPv4 정제, `--confirm` 생성 게이트, manifest의 정제 후 해시·메모·build fingerprint, 심볼릭 링크/ZIP 경로 이탈 차단, 한글 파일명 및 결정적 ZIP 순서·시각을 검사했다.

이 Linux sandbox에는 MSVC와 Unreal Engine이 없으므로 MSVC 컴파일, Unreal 통합 빌드, 패키지 실행본, Windows PowerShell 실행은 수행하지 않았다. 이 단위는 C++ 헤더가 아니므로 exception-free C++ include 검사는 적용 대상이 아니다.

## 제한 사항과 다음 통합 단계

- Unreal 통합·빌드·실행본 검증은 미완료다. 실제 F5/F6/AI 파일을 새로 만들거나 게임 내 화면에서 이 CLI를 호출한 검증도 수행하지 않았다.
- 실물 Boxer/다른 조종기, 다른 물리 PC, 사람 비행·사람 피드백, 실제 LAN·온라인 또는 외부 자동 전송 검증은 수행하지 않았다.
- 순환 기록(DAT-04), 게임 내 문제 순간 북마크(DAT-05)의 선택 기체/시각 연결, 보관·삭제 UI(DAT-09), 저장 부하 p95 측정(DAT-10)은 구현하지 않았다.
- 패턴 밖의 민감 문구를 완전 탐지한다고 보장하지 않는다. 공유 전 dry-run과 `manifest.json`을 사람이 재확인해야 하며, 바이너리/대용량 자료는 의도적으로 빠진다.

다음 Unreal 통합에서는 `ImperialUserData.cpp`의 Saved 루트를 UI에 안전하게 전달하고 `AImperialHUD`에 포함/제외 미리 보기와 명시적 확인 버튼을 추가한다. `AImperialMatchManager.cpp`의 F5/F6 기록과 `ImperialAIResults.cpp`의 AI 결과는 읽기 전용 후보로만 연결하며 저장·조종 상태를 바꾸지 않는다. `UImperialRadioSubsystem::ExportDeviceReport()`의 `profile_path` 및 장치 식별자는 이 도구와 같은 정제 계약을 게임 내부 내보내기로 이관해야 한다. 마지막으로 `docs/feedback-form.html`의 수동 첨부 안내에 manifest 검토 절차를 연결하되 업로드·자동 전송 기능은 추가하지 않는다.
