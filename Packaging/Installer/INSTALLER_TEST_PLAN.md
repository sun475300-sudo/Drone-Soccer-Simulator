# Installer verification plan

Scope: synthetic safety fixtures and the exact final v0.7.13-rc1 portable payload, validated in dedicated disposable QA targets. The CLI inventory suite does not launch Unreal. Installed-game results are separately recorded below; no HID, physical-controller or real personal-profile verification is claimed. All fixture targets, saved-data sentinels, reports and process logs remain under this scratch directory. Child installer processes use CREATE_NO_WINDOW and --quiet --no-register. No mouse/keyboard input, package-tool installation, permission changes or existing project edits.

## Required positive cases

1. Fresh installation into a new Unicode/space path. Compare the complete installed payload file set and each SHA-256 with SHA256SUMS; verify no flattened/missing Windows/Guidance/Prerequisites directory. Inspect receipt product/version/absolute target.
2. Recognized clean previous manual ZIP layout can be explicitly updated. New files replace old files only after staging hashes pass. An adjacent backup preserves every previous file byte. External Saved sentinels are unchanged.
3. Managed install update and reinstall policy are explicit. A fresh operation against an occupied directory is refused. Same-version update either returns an explicit safe result or follows the same transaction without silent deletion.
4. Uninstall removes only receipt-owned unchanged files and installer metadata, preserves unknown files and external Saved sentinels, and reports any remnants. Running uninstall helper self-removal uses only its own verified target and must never recurse above it.
5. Separate-folder installation leaves the existing installation byte-for-byte intact.
6. Final real payload separately: relocated launcher --inspect, game execution with an explicit isolated ImperialDataDir, entire hash manifest and source/build identity (parent-owned).

## Failure and recovery cases

- Unknown existing directory with a sentinel: update is refused and the sentinel/tree are unchanged. Never infer ownership just from an EXE filename.
- Legacy Saved directory, RadioProfile.json, PilotControls.json, PilotResponse.json or Language.txt under a selected update target: refuse unsupported migration or preserve it under an explicit documented policy; no implicit deletion.
- Unrecognized extra files in an otherwise recognized manual bundle: update refusal is acceptable, with no modification. Managed uninstall must preserve them.
- Existing package executable held open with a real Windows handle that denies delete sharing: update refuses before rename; file/tree unchanged; no process kill.
- Corrupt payload file, bad/missing manifest, duplicate paths/case collisions, unexpected unmanifested payload entry: no final target, no change to prior install.
- ZIP traversal (`../escape`, rooted path, drive path), reparse/symlink attributes, target or ancestor directory junction: reject without outside writes.
- Fault injection immediately after old-directory rename: return nonzero, restore the exact original target tree, remove or clearly identify generated staging, no dangling target absence.
- Parent is a regular file / impossible target / read-only-locked receipt: return clear nonzero and retain all existing bytes. Do not change ACLs to manufacture a test.
- Interrupted installation: staging must not be mistaken for an installed root on retry. Backup policy and recovery action must be explicit. Power-loss recovery is not proven by exception rollback alone.
- Protected paths: volume root, Windows, Program Files root, user-home root and the personal application Saved path are rejected. Test policy without writing there.
- Cancellation before commit leaves old installation intact. No UI control should report successful completion after failed verification.
- CLI error result JSON must identify action/path/reason and be written only to the requested QA result path; --quiet must not open MessageBox on error.

## Evidence format

Store one JSON row per scenario with installer artifact SHA, arguments (no private paths), exit code, result JSON, original/after tree SHA maps, backup presence, staging remnants, sentinel checks and elapsed time. Distinguish helper/test-harness assertions from independent installer assertions. Keep failed raw results and record any rerun separately.

## Known limits to report

- Unsigned EXE can trigger SmartScreen/unknown-publisher warnings; do not alter system security settings.
- No true disk-full, machine power-loss, second physical PC, all Windows ACL variants or user GUI interaction is implied by fixture success.
- Actual launch is separately tested using the real packaged runtime and isolated user data; synthetic payload binaries do not prove it.

## v0.7.13-rc1 최종 설치기 검증

2026-10-28에 최종 ZIP 전체 133개 파일을 내장한 설치기를 전용 QA 폴더에서 검증했다. 설치·업데이트·백업·일반 실패 롤백·설정과 기록 보존·정상/손상 설치 제거 검사는 14회 실행, 65/65 통과했다. 같은 소스의 작은 가상 배포본에서 현재 작업 폴더·하위 폴더 경로, 루트 정규화, 보호 경로, 제거 도우미의 부모 종료 대기는 25개 명령, 92/92 통과했다.

공개 v0.7.12 설치본 및 설치 기록이 없는 수동 ZIP 압축본을 v0.7.13으로 업데이트하는 추가 검사는 34/34 통과했다. 업데이트한 실제 게임은 별도 ImperialDataDir와 UserDir로 실행했으며 내부 한국어/영어·화면 API·안전·저장 검사는 57/57 통과했다. 이는 창을 그리지 않는 자동 검사이며 실제 화면 조작·실물 조종기·다른 PC 검증을 뜻하지 않는다.

설치기의 현재 작업 폴더가 설치 대상 안에 있으면 해당 설치기 프로세스만 대상 밖의 검증된 폴더로 옮겨 Windows 디렉터리 잠금을 피한다. 다른 프로그램이나 부모 셸의 작업 폴더는 바꾸지 않는다. 제거 도우미는 별도 임시 폴더에서 실행하고 원래 제거기가 종료될 때까지 기다린다.

최종 설치 EXE SHA-256: `04e01e7bbc2bcfe93a933476651c619bd815fc0024a074d0514aa98e058750e6`
최종 ZIP SHA-256: `b9243ad70bb3f6094cd71ed9a491dc95b7be10474688911be7aaa89565aebecd`
설치기 C# SHA-256: `656d46a254418766cc793e106c032415352a18ed110988d5535f6b5cef6f0dbf`

최종 빌드 및 검증 명령 예:

```powershell
.\Build-Installer.ps1 -BundleZip E:\Builds\ImperialWings-Windows-v0.7.13-rc1.zip -OutputPath E:\QA\ImperialWings-Setup-v0.7.13-rc1.exe
python .\test_installer_fullpayload.py --installer E:\QA\ImperialWings-Setup-v0.7.13-rc1.exe --bundle-zip E:\Builds\ImperialWings-Windows-v0.7.13-rc1.zip --qa-root E:\QA\InstallerTests --source-root . --expected-payload-files 133
```

검사용 Python은 개발자 검사에만 필요하며 사용자 설치기는 Python을 요구하지 않는다. qa-root는 미리 만든 전용 시험 폴더여야 한다. 기존 출력 EXE는 덮어쓰지 않는다. 설치 EXE는 서명되지 않았으며 Windows 경고가 표시될 수 있다. GUI에서 설치·업데이트·제거를 직접 누르는 전체 흐름, 실물 하드웨어, 다른 PC, 실제 디스크 부족/권한 거부, 전원 중단 및 강제 종료 복구는 이 자동 검사에서 검증하지 않았다. 필수 게임 파일이 없으면 설치 창의 제거 버튼이 비활성화될 수 있으나 설치된 제거기 또는 CLI를 통한 손상 설치 제거는 검증했다.