# 한국어·영어 현지화 정적 검사 범위 — 2026-10-31
## 목적과 계획 연결
이 문서는 `Scripts/check-localization.py`의 정적 검사 범위와 실제 실행 결과를 기록한다.
검사기는 Unreal 실행 기능이나 새 언어 리소스를 추가하지 않으며, 현재 `FTranslation` 표와
표 밖 직접 문자열을 발견해 이관 작업의 입력으로 제공한다.

- 사용자 20개 묶음 중 **2번 한국어·UI 가독성**의 후속 보조 검사다.
- 통합 개발 계획의 피드백 **5번 언어 지원**과 연결한다.
- 단계 표의 **D 시점·사용성**(한국어/영어·메뉴/HUD) 준비 범위다.
- 기능 명세의 `UX-01`(한국어 메뉴·용어), `UX-05`(도움말·거부 이유·해결 절차),
  `UX-08`(소리 대체 표시의 텍스트 점검)에 연결한다.
- 통합 계획 7절의 “하나의 문자열 목록·번역 키·용어표” 원칙을 위한 현황 계측이다.
- `follow-up-plan-2026-10-28.md`의 internationalization 3절에서 미구현으로 둔
  안정 키·언어별 리소스·누락 대체·형식 인수 검사 이관의 첫 정적 보조 도구다.

이 도구가 현재 영어 문장 자체를 키로 쓰는 `ImperialLocalization.cpp` 구조를 안정된 의미 키로
바꾸지는 않는다. 한국어와 영어 외 언어, 글꼴, RTL, IME, 언어별 번역 검수도 구현하지 않는다.

## 현재 구조와 검사 상태 기계

`ImperialLocalization.cpp`은 `FTranslation { English, Korean }` 배열을 보관한다.
`ImperialLocalization::Translate`는 한국어 선택 시 정확 일치 행을 먼저 찾고, 현재 지원하는
printf 형식 문자열을 해석해 중첩 문자열을 번역한다. 영어 선택 시에는 입력 영어 원문을 반환한다.
`ImperialLocalization.h`의 언어 선택과 저장은 조종기 보정과 별도의 표시 전용 상태다.

검사 상태는 다음 순서로만 전이한다.

1. **입력 확인**: `--source-root`, 선택 `--table`, JSON/Markdown 출력 경로와 선택 allowlist를 받는다.
2. **표 추출**: `FTranslation Translations[]`의 최상위 행을 읽어 영어·한국어 열과 원본 행 번호를 보존한다.
3. **표 판정**: 누락, 빈 값, 동일한 문장형 KO/EN 값, 중복 영어 키, 형식 인수 차이를 독립 분류한다.
4. **우회 탐색**: 표 파일을 제외한 `.cpp`/`.h`에서 주석을 제외한 `TEXT("...")`와 일반 문자열을 찾는다.
5. **우회 판정**: 한글 리터럴은 표 밖 리터럴로, 직접 HUD/notice 호출의 문장형 영어는 번역 우회로 기록한다.
6. **allowlist 적용**: 정확한 상대 경로 glob과 리터럴이 일치한 로그 전용 항목만 제외한다.
7. **보고/종료**: JSON과 Markdown을 쓰며 일반 모드는 0, `--strict` 모드는 발견 항목이 있으면 1로 끝난다.

이 상태 기계는 정적 문자열만 다룬다. 변수에 저장했다가 나중에 HUD가 그리는 문자열, 매크로 확장,
런타임 조합, 실제 언어 설정 저장·복구, 폰트 글리프와 화면 배치는 전이 대상이 아니다.

## 검사기 매개변수와 근거

| 항목 | 현재 값 또는 동작 | 근거·구분 |
|---|---|---|
| 소스 루트 | 호출 시 `--source-root`로 지정 | 검사기 선택값. Unreal 모듈 경계를 호출자가 명시한다. |
| 기본 표 | 소스 루트 아래 `ImperialLocalization.cpp` | 현재 코드 구조에서 확인한 경로이며 규정값이 아니다. |
| 표 열 | 영어 원문 키 / 한국어 표시 값 | `FTranslation` 선언과 `Translate` 조회 구현에서 확인한 현재 구조다. |
| 형식 인수 | printf `%d`, `%s`, `%u`, `%.1f` 등과 `{0}`류 비교 | 현재 `FDisplayFormat`의 printf 처리와 후속 리소스 설계의 이름/위치 인수 요구를 반영한 검사기 선택값이다. |
| HUD 문장 탐색 | DrawText·notice 계열 호출 인근의 문장형 영어 | 오탐을 줄이기 위한 검사기 선택값이며 전체 데이터 흐름 분석이 아니다. |
| 요약 상한 | Markdown 상위 발견 12개 | 읽기 쉬운 보고를 위한 검사기 선택값이다. JSON에는 전체 발견을 보존한다. |
| allowlist | JSON `entries`의 path/literal/reason | 로그 전용처럼 의도적인 표 밖 문자열을 추적 가능하게 남기는 검사기 선택값이다. |
| strict | 발견 1개 이상이면 종료 코드 1 | CI 도입 전 현황 보고와 차단을 분리하기 위한 검사기 선택값이다. |

이 유틸리티에는 FIDA 경기 규정 수치나 `docs/rules-sources-2026.md`의 조항을 사용하는
매개변수가 없다. 따라서 규정 유래값은 없고, 위 임계·휴리스틱은 모두 **검사기 선택값**이다.
표의 실제 사용자 문구와 형식 인수는 현재 소스 관측값이며, 번역의 정확성·공식 용어 적합성을 인증하지 않는다.

## 실제 저장소 검사 결과

다음 명령을 저장소 루트에서 실행했다.

```text
python3 Scripts/check-localization.py --source-root Source/ImperialWingsSimulator \
  --json-report /tmp/build-l10ncheck/localization-report.json \
  --markdown-summary /tmp/build-l10ncheck/localization-summary.md
```

실제 표 행은 **572개**, 실제 결과 줄은 **`572 translation entries, 79 findings`**였다.
일반 모드의 종료 코드는 0이며, 같은 입력에 `--strict`를 붙인 실행은 의도대로 종료 코드 1이었다.

| 분류 | 실제 수 | 해석 |
|---|---:|---|
| 영어 값 누락 | 0 | 현재 표 행에서 발견하지 못함 |
| 한국어 값 누락 | 0 | 현재 표 행에서 발견하지 못함 |
| 빈 영어/한국어 값 | 0 / 0 | 현재 표 행에서 발견하지 못함 |
| 동일 KO/EN 문장형 값 | 0 | 휴리스틱 기준의 번역 누락 의심 없음 |
| 영어 키 중복 | 0 | 현재 영어 원문 키 중복 없음 |
| 형식 인수 불일치 | 1 | 표 37행의 `percentage points` / `(%p)`를 검사기가 `%p`로 해석함 |
| 표 밖 한글 리터럴 | 78 | 아래 파일 분포와 같이 후속 이관·allowlist 검토 대상 |
| 직접 HUD/notice 영어 문장 | 0 | 현재 제한된 호출 휴리스틱에서 발견하지 못함 |
| 총 발견 | 79 | strict 모드에서는 차단 결과 |

형식 인수 1건은 실제 런타임 결함으로 확정하지 않았다. 한국어 `(%p)`가 의도한 표시 문자일 수 있으므로,
`ImperialLocalization.cpp`의 표시 의도와 `FDisplayFormat` 해석 결과를 Unreal 실행 전 별도로 검토해야 한다.

표 밖 한글 78건의 파일별 분포는 다음과 같다.

| 파일 | 수 | 현재 해석 |
|---|---:|---|
| `ImperialFirstFlightGuide.cpp` | 46 | 직접 안내 문구; 표 이관 후보를 실제 UX 경로와 함께 검토 |
| `ImperialRadioUXVerifier.cpp` | 12 | 검증용 한국어 기대 문자열; 자동 검사 목적과 allowlist/표 조회 전환을 구분 |
| `ImperialAIResultsVerifier.cpp` | 9 | 검증용 번역 결과 기대 문자열; 동작 검증 자료와 사용자 표시를 구분 |
| `ImperialHUD.cpp` | 9 | HUD의 직접 한국어 리터럴; 표시 경로를 표와 연결할 후보 |
| `ImperialAIPracticeHUD.cpp` | 1 | 언어 식별 리터럴; 사용자 표시 여부를 확인 |
| `ImperialFirstFlightVerifier.cpp` | 1 | 검증용 기대 문자열; 런타임 사용자 문구와 구분 |

## 단위 테스트와 검증 범위

합성 소스에서 누락/빈 값, 중복, printf·중괄호 인수 차이, 동일 문장, 한글 우회,
직접 HUD 영어 우회, 주석 제외, 정확 allowlist, 일반/strict 종료 코드를 검사했다.

```text
python3 -m py_compile Scripts/check-localization.py Tests/test-check-localization.py
python3 Tests/test-check-localization.py
```

실제 최종 테스트 결과 줄은 **`24 checks, 0 failures`**였다.
테스트의 범위 고지 줄은 “Static localization checker tests only; Unreal compilation and runtime were not run.”이다.
이는 Python 정적 검사 단위 테스트이며, C++ 헤더가 없으므로 g++ 예외 비활성 헤더 검사는 적용 대상이 아니다.
MSVC와 Unreal Engine 5.8은 이 Linux 환경에 없으므로 실행하지 않았고, Unreal 빌드·패키징·게임 실행,
실물 Boxer, 다른 PC, 사람 조종 또는 실제 네트워크 검증을 수행하지 않았다.

## 제한과 다음 통합 단계

- 이 작업은 기존 Unreal 소스를 수정하지 않았으므로 현재 79개 발견을 해결하지 않는다.
- 리터럴 탐색은 정적 휴리스틱이다. 변수 전달·매크로·실행 시 조합 경로와 모든 사용자 문구를 완전하게 증명하지 않는다.
- 78개 한글 항목에는 검증 fixture도 섞여 있다. allowlist는 정확한 사유 없이 실제 사용자 문구를 숨기는 용도로 쓰면 안 된다.
- 0건의 영어 HUD 우회는 현재 탐색 패턴에서만 0건이다. 모든 HUD 화면이 번역되었다는 뜻이 아니다.
- 해상도, 100/150/200% DPI, 긴 번역 줄바꿈, CJK 글꼴, IME, RTL, 화면 입력 포커스는 미검증이다.
- 한국어/영어 외 언어는 여전히 미구현이며, 자동 검사 통과만으로 새 언어 지원 완료로 표시할 수 없다.

다음 통합은 기존 파일을 다음 순서로 변경·검증해야 한다.

1. `ImperialFirstFlightGuide.cpp`와 `ImperialHUD.cpp`의 실제 사용자 한글을
   `ImperialLocalization.cpp` 행과 `ImperialLocalization::Translate` 경로로 이관하고, 원문/용어를 `UX-01/05/08` 기준으로 검토한다.
2. `ImperialAIResultsVerifier.cpp`, `ImperialRadioUXVerifier.cpp`, `ImperialFirstFlightVerifier.cpp`의 기대 문자열은
   표 조회 기반 기대값으로 바꾸거나, 로그 전용일 때만 사유 있는 allowlist JSON 항목을 별도 관리한다.
3. 표 37행의 `(%p)` 의도를 확인해 리터럴 퍼센트라면 안전한 표기 방식으로 고치고, 형식 인수라면 영어·한국어 인수를 일치시킨다.
4. `ImperialLocalization.h/.cpp`의 영어 문장 키를 후속 계획의 안정된 의미 키와 언어별 리소스로 단계적으로 이관한다.
5. Windows MSVC/Unreal 빌드와 실제 720p·DPI·IME·언어 저장/재실행 화면 검증을 별도 실행본·지문으로 수행한다.

현재 상태 제안: **핵심 정적 검사 구현·Python 단위 검사 통과, Unreal 통합·빌드·실행 검증 대기**.
