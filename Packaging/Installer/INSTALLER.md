# Windows 단일 EXE 설치기

이 설치기는 전체 포터블 묶음의 `Windows`, `Guidance`, `Prerequisites`, 실행기와 안내를 한 EXE에 담는다. 실행기만 따로 배포하여 게임 파일을 누락하는 방식이 아니다. 설치 도구나 관리자 권한을 자동 설치·요청하지 않는다.

## 사용자 선택

- **새 폴더에 별도 설치:** 새 버전의 기본 경로 또는 직접 선택한 비어 있는 폴더에 설치한다. 기존 파일이 들어 있는 폴더는 교체하지 않는다.
- **선택한 기존 버전 업데이트:** 기존 배포 정보와 전체 SHA-256 목록을 확인한다. 선택을 한 번 더 확인한 뒤 새 파일을 임시 폴더에 준비하고, 이전 프로그램은 인접한 `.backup-...` 폴더에 보관한다. 실행 중인 프로그램은 닫은 뒤 다시 시도해야 한다. 설치기는 프로세스를 종료하지 않는다.
- **제거:** 설치 폴더의 `ImperialWingsUninstall.exe` 또는 설치기의 ‘이 설치 제거’를 사용한다. 설치한 뒤 변경되지 않은 프로그램 파일만 제거하며, 추가·변경 파일은 남긴다. 수동 압축본은 제거 목록이 없으므로 자동 제거하지 않는다.

언어·조종기 보정·기록은 `%LOCALAPPDATA%\ImperialWingsSimulator\Saved`에 있으며 설치와 제거 대상이 아니다. **별도 폴더 설치도 이 Windows 사용자의 설정·기록을 공유한다.** 버전별 설정 격리를 제공한다고 보장하지 않는다.

## 보수적인 중단 조건

연결 폴더(정션·심볼릭 링크), 경로 탈출, 해시가 다른 기존 파일, 알 수 없는 추가 파일, 수동 압축본 안의 `Saved`, 파일 잠금·실행 중인 프로그램, 쓰기 권한·공간 부족은 업데이트를 중단한다. 기존 내용을 직접 삭제하라고 자동 처리하지 않고 다른 폴더 설치를 안내한다. 모든 과거 버전의 설정 자동 이전을 보장하지 않는다.

업데이트 중 일반적인 파일 교체 실패는 이전 폴더를 복원한다. **전원 차단·프로세스 강제 종료·악성 동시 파일 변경까지 자동 복구를 보장하지 않는다.** 성공한 업데이트의 이전 버전 백업은 자동 삭제하지 않는다. 복구가 필요한 경우 게임을 종료한 뒤 백업과 현재 폴더를 확인해야 한다.

설치 위치 기록은 이 앱의 `%LOCALAPPDATA%\ImperialWingsInstaller\last-install.txt`에만 저장한다. 시스템 프로그램 등록·시작 프로그램·방화벽·권한은 변경하지 않는다. Windows ‘설치된 앱’ 목록에 등록하는 전통적인 MSI 설치기가 아니다.

설치기와 프로그램은 코드 서명되지 않았다. Windows가 ‘알 수 없는 게시자’ 또는 SmartScreen 경고를 표시할 수 있다. 릴리스의 SHA-256과 다운로드 출처를 확인한다. .NET Framework 4.8, Windows 10/11 x64가 필요하다. 실제 게임의 Visual C++ 실행 구성 요소·DX12 SM6/GPU 조건은 별개이며 설치기가 자동 설치하지 않는다. 필요한 VC 구성 요소는 동봉한 `Prerequisites/vc_redist.x64.exe`를 사용자가 선택해 설치한다.

## 검증용 명령

개발자가 지정한 새 QA 폴더에서만 사용한다. `--quiet`는 창을 열지 않고 JSON과 종료 코드로 결과를 반환하며 `--no-register`는 최근 설치 위치를 실제 사용자 설정에 쓰지 않는다.

```text
Setup.exe --install E:\QA\New --mode fresh --quiet --no-register --result E:\QA\fresh.json
Setup.exe --install E:\QA\Existing --mode update --quiet --no-register --result E:\QA\update.json
Setup.exe --verify E:\QA\Existing --quiet --result E:\QA\verify.json
Setup.exe --uninstall E:\QA\Existing --quiet --no-register --result E:\QA\remove.json
```

실패 복원 검사는 `--test-fail after-backup --test-root E:\QA`를 명시한 대상 하위에서만 허용한다. 이 옵션은 설치 개발 검수용이며 일반 사용 흐름에는 표시하지 않는다.

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