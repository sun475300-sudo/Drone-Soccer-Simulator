# Imperial Wings · Drone Soccer Simulator

[한국어](README.md) · **English**

Imperial Wings is a Windows drone soccer simulator for keyboard or compatible USB controller practice. The default view is the pilot's line of sight from outside the court, and manual flight uses ANGLE self-levelling. The aircraft use motor forces and rigid-body physics; their simulated behaviour is not a measured or certified model of a real aircraft.

**Latest public download: v0.7.15-dev-safety3, published 29 October 2026. This is a development pre-release, not a finished product.** The game code on main is still based on v0.7.4 and does not incorporate safety3. Use the [v0.7.15-dev-safety3 source tag](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/tree/v0.7.15-dev-safety3) to develop or build this release. A README update does not mean a new binary was published or all work is complete.

[Download safety3 installer EXE](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/download/v0.7.15-dev-safety3/ImperialWings-Setup-0.7.15-dev-safety3.exe) · [Download safety3 Windows ZIP](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/download/v0.7.15-dev-safety3/ImperialWings-0.7.15-dev-safety3.zip) · [SHA-256 checksums](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/download/v0.7.15-dev-safety3/SHA256SUMS-safety3.txt) · [Release and limitations](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/tag/v0.7.15-dev-safety3) · [Previous v0.7.14-rc1](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/tag/v0.7.14-rc1)

## What changed in safety3

- Explicit saves recheck controller response, button and throttle settings to avoid overwriting corrupt or unsupported-version files. Unknown fields in valid files are retained.
- Calibration saves are serialized between new versions using the same lock. Older versions and external programs do not participate; stale edits are not automatically merged.
- Exit is deferred if AI results cannot be saved. An exit request during a duel waits for AI landing and saving. F6 write-error state is cleared only after a new recording starts successfully.

## Features carried forward

- A six-step first-flight guide: takeoff, hover, movement, landing, first goal and final landing. The guide observes your flight; you choose when to advance and operate ARM and the controls yourself.
- Illustrated USB connection and calibration guidance, Mode 1 and Mode 2, basic and advanced response settings, and Korean/English selection. Some internal messages are not yet translated.
- AI defensive practice and a local three-minute duel against three or four AI aircraft, with beginner, intermediate and advanced settings.
- A read-only comparison screen for saved AI results. You explicitly select reference A and candidate B; mismatched recorded conditions, different versions or source hashes, the same session, automated tests and interrupted sessions are blocked.
- Improved HUD text/device-name display and feedback when F6 recording cannot be saved.

![Korean AI result comparison screen](https://raw.githubusercontent.com/sun475300-sudo/Drone-Soccer-Simulator/v0.7.14-rc1/docs/images/v0.7.14-compare-ko.png)

The screenshot is from the previous v0.7.14-rc1 Windows package and uses synthetic test records, not human performance results. It is not evidence that safety3's pending screen checks passed.

## Requirements and installation

Windows 10 or 11 x64 and a DirectX 12, Shader Model 6 capable GPU are required. The installer requires .NET Framework 4.8. The game requires the Microsoft Visual C++ x64 runtime; a redistributable is included in the package for you to install if needed. Unreal Engine, Python and Git do not need to be installed to run the package.

The installer and program are unsigned, so Windows or your browser may show an unknown publisher or SmartScreen warning.

**Installer:** Run the EXE and choose either a new folder or an update of a recognized existing installation. Updating verifies the existing files, prepares the replacement and retains a backup of the previous program. Close the game before updating. See the [installer, backup and removal guide](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/blob/v0.7.15-dev-safety3/Packaging/Installer/INSTALLER.md) (Korean) for the supported conditions and recovery limits.

**ZIP:** Extract the entire archive to a new folder, then run ImperialWingsLauncher.exe. Keep the Windows, Guidance and Prerequisites folders alongside it. Do not extract only the launcher. GitHub's automatically generated “Source code” ZIP is for development and is not the runnable Windows package.

Configuration, controller calibration and records are normally stored in %LOCALAPPDATA%/ImperialWingsSimulator/Saved. Separate installation folders share this Windows user's data; they do not isolate settings by version. Installation and removal do not intentionally delete that user-data folder. Keep your calibration files private.

## Keyboard first flight

Start with the keyboard, close controller setup and press F1 to open the guide. All aircraft start disarmed. The guide does not arm or fly for you.

| Key | Action |
|---|---|
| F1 | Open or close the first-flight guide |
| Space | ARM / DISARM; start with throttle at minimum |
| Up / Down | Increase / decrease throttle |
| W / S | Tilt forward / backward |
| A / D | Tilt left / right |
| Q / E | Turn left / right |
| 1 / 2 / 3 | Pilot line-of-sight / external observer / arena overview |
| C | Controller setup |
| F6 | Start / stop a flight recording |

**Throttle remains at its last value when you release Up or Down.** ANGLE returns the aircraft toward level attitude; it does not hold altitude. Land and disarm when finished. After the landing stage, lower the throttle, release the controls and explicitly ARM again before taking off for the first-goal stage. Reaching the goal and completing the final landing are separate steps.

## USB controller

1. Connect a data-capable USB cable and select USB joystick mode on the controller, if applicable.
2. Open C, select the actual detected device, and choose its stick layout. In Mode 1 the right vertical stick is throttle; in Mode 2 the left vertical stick is throttle.
3. Follow the stick setup instructions to capture the axes, full ranges, neutral positions and minimum throttle. Review the result before confirming and saving it.
4. Keep throttle at minimum and other sticks centred, then explicitly enable the controller. ARM is a separate action; connection alone does not start flight.
5. On later launches, use the saved-controller connection option or F8 and check the device and controls again. Recalibrate when the controller, mapping, firmware or computer requires it.

The current input path uses Windows WinMM. Compatibility with every controller is not guaranteed. Physical Boxer stick direction, continuous USB flight, cable loss and reconnection have not been fully tested in this release. No controller is required for keyboard practice.

## AI practice and records

In defensive practice, the player controls the blue striker against three or four AI defenders. In the three-minute duel, the player controls one striker against three or four opposing AI aircraft. The AI stop action lands the AI aircraft; you must land your own aircraft yourself. Official 5v5 play remains manual.

From AI settings or a saved recent result, open saved-record comparison, choose A and B, then compare. Differences are B minus A. The list is limited to 100 files in filename order and is read on opening or refresh. It supports AI result schema 1 and 2, not F6 JSONL or F5 CSV. It does not edit, delete or transmit source records. Matching recorded fields do not prove all training conditions match or certify skill.

## Verification and known limits

The [safety3 release record](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/tag/v0.7.15-dev-safety3) reports isolated Unreal automated checks, AI save-failure/exit retries, settings-file preservation, ZIP CRC/hash checks, portable checks (13/13) and installer no-registration checks (65/65). Synthetic input/records and nullrhi tests are not physical-controller or rendered-UI validation. Fixed-timestep checks are not measurements of actual rendering FPS.

- Physical Boxer calibration/flight, cable loss/reconnection, another physical PC, a complete human first flight, human record comparisons and high-DPI coverage remain unverified.
- A focus-safety interruption left the actual Korean/English F6 error warning and overlap checks incomplete. The complete installer GUI flow, a real upgrade from public v0.7.14, power loss, disk-full/permission-denied conditions and SmartScreen remain unverified.
- Real-browser feedback-form testing was blocked by local-file access. Unsaved AI results are not guaranteed to survive forced OS termination.
- Changes and tests in later isolated local candidates are not counted as included in, or validation of, this public package. Online play, country/world leaderboards, more languages and a full AI coach remain future work.

The public tag points to [`c0787aba9b3db4f7f41224adfaeada6aa6be9512`](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/commit/c0787aba9b3db4f7f41224adfaeada6aa6be9512). The release record gives build-input SHA-256 `8187bde6de435f866f33792ff1663aeb2b81ad71bd782403f42481cd0ed3b167` across 169 inputs; test scripts and documentation are outside that fingerprint. Verify downloads against the linked SHA-256 list. See the [previous v0.7.14-rc1 record](docs/releases/v0.7.14-rc1.md) for historical checks; they are not fresh safety3 results.

## Feedback

Please use [GitHub Issues](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/issues) to report an issue or suggestion. Include your version, steps, and expected and actual behaviour. Review what you are making public; do not attach personal calibration files or your entire Saved folder. The package also includes an offline feedback-form.html to help prepare a report. It does not send the report automatically.
