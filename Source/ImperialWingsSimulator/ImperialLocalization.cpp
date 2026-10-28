#include "ImperialLocalization.h"
#include "ImperialUserData.h"
#include "ImperialProfileStorage.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"

namespace
{
struct FTranslation { const TCHAR* English; const TCHAR* Korean; };
const FTranslation Translations[] =
{
    {TEXT("Solo goal training  /  Blue striker  /  LOS + ANGLE"),TEXT("혼자 골 연습 · 파란 공격수 · 육안 조종 + 앵글 모드")},
    {TEXT("DEFENCE PRACTICE"),TEXT("수비 상대 연습")},
    {TEXT("DUEL / 3 MINUTES"),TEXT("AI 대전 / 3분")},
    {TEXT("3 AI: STRIKER + 2 DEFENDERS"),TEXT("AI 3대: 공격수 1 + 수비 2")},
    {TEXT("4 AI: STRIKER + 3 DEFENDERS"),TEXT("AI 4대: 공격수 1 + 수비 3")},
    {TEXT("3 DEFENDERS"),TEXT("수비 3대")},
    {TEXT("4 DEFENDERS"),TEXT("수비 4대")},
    {TEXT("LAST RESULT"),TEXT("최근 결과")},
    {TEXT("DIFFICULTY"),TEXT("난이도")},
    {TEXT("BEGINNER"),TEXT("초보")},
    {TEXT("INTERMEDIATE"),TEXT("중급")},
    {TEXT("ADVANCED"),TEXT("고급")},
    {TEXT("Slower reaction / patrol"),TEXT("느린 반응 / 순찰")},
    {TEXT("Faster reaction / tracking"),TEXT("빠른 반응 / 추적")},
    {TEXT("FORMATION SEED   %u"),TEXT("배치 번호   %u")},
    {TEXT("NEW RANDOM FORMATION"),TEXT("새 랜덤 배치")},
    {TEXT("Same seed repeats tactical targets. Physics / collisions may differ."),TEXT("같은 배치 번호는 전술 목표를 반복합니다. 실제 움직임·충돌은 달라질 수 있습니다.")},
    {TEXT("You: blue striker. Red striker attacks; red defenders guard. Not official 5v5."),TEXT("파란 공격수를 직접 조종합니다. 빨간 팀은 AI이며 공식 5대5 경기가 아닙니다.")},
    {TEXT("Only red defenders launch automatically. You arm the blue striker manually."),TEXT("빨간 수비만 자동으로 이륙합니다. 파란 공격수는 직접 시동을 켜세요.")},
    {TEXT("Most valid goals in 3 minutes wins. Retreat to your own half after scoring."),TEXT("3분 동안 승인 득점이 많은 팀이 이깁니다. 득점 후 자기 진영으로 돌아오세요.")},
    {TEXT("3-minute practice limit. STOP AI lands defenders; your throttle stays manual."),TEXT("연습은 최대 3분입니다. AI 중지는 수비만 착륙시킵니다. 내 기체는 직접 착륙하세요.")},
    {TEXT("Settings lock while running. Official match play remains manual."),TEXT("진행 중에는 설정을 바꿀 수 없습니다. 공식 경기는 직접 조종합니다.")},
    {TEXT("Ready: all aircraft must be landed and disarmed before START."),TEXT("시작 전 모든 기체를 착륙시키고 모터를 꺼 주세요.")},
    {TEXT("LANDING IN PROGRESS"),TEXT("AI 착륙 중")},
    {TEXT("STOP AI / LAND OPPONENTS"),TEXT("AI 중지 / 상대 기체 착륙")},
    {TEXT("START PRACTICE / RESET PADS & SCORE"),TEXT("연습 시작 / 시작 위치·점수 초기화")},
    {TEXT("Local solo training. Your blue striker always needs manual landing; STOP does not land it."),TEXT("파란 공격수는 항상 직접 착륙해야 합니다. AI 중지로 내 기체가 자동 착륙하지 않습니다.")},
    {TEXT("AI DUEL / LAST RESULT"),TEXT("AI 대전 / 최근 결과")},
    {TEXT("AI PRACTICE / LAST RESULT"),TEXT("AI 연습 / 최근 결과")},
    {TEXT("%d AI  /  %s  /  SEED %u"),TEXT("AI %d대 · %s · 배치 번호 %u")},
    {TEXT("STOP REQUESTED"),TEXT("사용자 중단")},
    {TEXT("TIME LIMIT"),TEXT("시간 종료")},
    {TEXT("LANDED"),TEXT("착륙 완료")},
    {TEXT("APP CLOSED"),TEXT("앱 종료")},
    {TEXT("AI OVERTURNED"),TEXT("AI 뒤집힘")},
    {TEXT("AI DISABLED"),TEXT("AI 작동 중단")},
    {TEXT("RECOVERY FAILED"),TEXT("복구 실패")},
    {TEXT("MODE CHANGED"),TEXT("모드 변경")},
    {TEXT("STOP: %s   /   FINISH: %s"),TEXT("중단 이유: %s · 마무리: %s")},
    {TEXT("BLUE : RED / VALID GOALS"),TEXT("파랑 : 빨강 / 승인 득점")},
    {TEXT("VALID GOALS"),TEXT("승인 득점")},
    {TEXT("PRACTICE TIME"),TEXT("연습 시간")},
    {TEXT("AI LANDING TIME"),TEXT("AI 착륙 시간")},
    {TEXT("%.1f s"),TEXT("%.1f 초")},
    {TEXT("GUARD CONTACTS"),TEXT("가드 충돌 횟수")},
    {TEXT("GUARD REMAINING"),TEXT("남은 가드 내구도")},
    {TEXT("BLUE STRIKER"),TEXT("파란 공격수")},
    {TEXT("RED STRIKER"),TEXT("빨간 공격수")},
    {TEXT("RED DEFENDER %d"),TEXT("빨간 수비 %d")},
    {TEXT("RECOVERY ATTEMPTS   %d"),TEXT("복구 시도   %d회")},
    {TEXT("BLUE WINS"),TEXT("파란 팀 승리")},
    {TEXT("RED WINS"),TEXT("빨간 팀 승리")},
    {TEXT("DRAW"),TEXT("무승부")},
    {TEXT("INTERRUPTED"),TEXT("중도 종료 / 승패 없음")},
    {TEXT("TRAINING"),TEXT("연습 기록")},
    {TEXT("Goals and guard values stop at the stop request. Landing is timed separately."),TEXT("득점·가드 상태는 중단 요청 시 확정합니다. 이후 착륙 시간은 따로 기록합니다.")},
    {TEXT("Contacts are per-aircraft registered impacts, not unique collision pairs or scoring attempts."),TEXT("충돌 횟수는 각 기체의 감지 횟수입니다. 충돌 쌍이나 득점 시도 횟수와 다릅니다.")},
    {TEXT("AUTOMATED TEST SESSION / no physical pilot evidence"),TEXT("자동 검사 기록 / 실제 사용자 조종 기록이 아닙니다")},
    {TEXT("Local practice record / no online ranking verification"),TEXT("로컬 연습 기록 / 온라인 순위 검증 없음")},
    {TEXT("SAVED: user data / TrainingResults / AI"),TEXT("저장됨: 사용자 데이터 / TrainingResults / AI")},
    {TEXT("NOT SAVED: retry, or discard this unsaved result before starting again."),TEXT("저장 실패: 다시 저장하거나 미저장 결과를 버린 뒤 새 연습을 시작하세요.")},
    {TEXT("RETRY SAVE"),TEXT("다시 저장")},
    {TEXT("DISCARD UNSAVED"),TEXT("미저장 결과 버리기")},
    {TEXT("SETUP"),TEXT("연습 설정")},
    {TEXT("Unsaved AI result: retry saving or explicitly discard it in LAST RESULT before starting again."),TEXT("저장하지 못한 AI 결과가 있습니다. 최근 결과에서 다시 저장하거나 버려 주세요.")},
    {TEXT("Select a pilot view before starting AI practice."),TEXT("AI 연습 전에 조종자 시점을 선택해 주세요.")},
    {TEXT("Land and disarm every aircraft before starting AI practice."),TEXT("AI 연습 전에 모든 기체를 착륙시키고 모터를 꺼 주세요.")},
    {TEXT("AI duel started: red striker + defenders launching. Blue striker stays manual. 3 minutes; RESET starts over."),TEXT("3분 AI 대전 시작 · 빨간 팀이 이륙합니다. 파란 공격수는 직접 조종합니다. 초기화하면 처음부터 시작합니다.")},
    {TEXT("AI defenders launching. Blue striker stays DISARMED: close menu, then arm manually at low throttle."),TEXT("AI 수비가 이륙합니다. 파란 공격수는 모터 정지 상태입니다. 창을 닫고 최저 스로틀에서 직접 시동하세요.")},
    {TEXT("AI opponents landing vertically. Land your blue striker manually; RESET is an immediate session reset."),TEXT("AI 상대가 수직 착륙합니다. 파란 공격수는 직접 착륙하세요. 초기화는 현재 연습을 즉시 다시 설정합니다.")},
    {TEXT("AI aircraft %d cannot continue. Landing the team; see LAST RESULT for the reason."),TEXT("AI 기체 %d의 비행을 계속할 수 없어 팀을 착륙시킵니다. 최근 결과에서 원인을 확인하세요.")},
    {TEXT("AI aircraft %d is blocked: trying a short escape route."),TEXT("AI 기체 %d의 경로가 막혀 짧은 우회를 시도합니다.")},
    {TEXT("AI landing delayed by contact. Inspect the arena; RESET explicitly resets all aircraft."),TEXT("접촉으로 AI 착륙이 지연됩니다. 경기장을 확인하세요. 초기화를 누르면 모든 기체가 다시 배치됩니다.")},
    {TEXT("STAGGERED"),TEXT("엇갈린 배치")},
    {TEXT("PATROL"),TEXT("순찰")},
    {TEXT("TRACKING"),TEXT("추적")},
    {TEXT("LANDING"),TEXT("착륙 중")},
    {TEXT("%s / %d AI / SEED %u / %s / RECOVERING %d / %.0f s"),TEXT("%s · AI %d대 · 배치 %u · %s · 복구 중 %d대 · %.0f초")},
    {TEXT("AI result saved. Open AI PRACTICE > LAST RESULT to review."),TEXT("AI 결과를 저장했습니다. AI 연습 → 최근 결과에서 확인하세요.")},
    {TEXT("AI result could not be saved. LAST RESULT keeps it in memory: RETRY SAVE or explicitly DISCARD UNSAVED."),TEXT("AI 결과를 저장하지 못했습니다. 최근 결과에 임시로 남아 있습니다. 다시 저장하거나 미저장 결과를 버려 주세요.")},
    {TEXT("Unsaved result discarded by request. Saved sessions remain unchanged."),TEXT("요청한 미저장 결과를 버렸습니다. 이미 저장된 기록은 보존했습니다.")},
    {TEXT("10 aircraft ready. Keyboard input selected; USB axes are uncalibrated."),TEXT("기체 10대 준비 · 키보드 조종 중입니다. USB 조종기는 보정 후 사용하세요.")},
    {TEXT("Selected aircraft disarmed. Current input source retained; low throttle is required to arm."),TEXT("선택 기체의 모터를 껐습니다. 입력 방식은 유지됩니다. 시동 전 스로틀을 내려 주세요.")},
    {TEXT("Training reset. All aircraft disarmed; scores cleared."),TEXT("연습을 초기화했습니다. 모든 모터를 끄고 점수를 지웠습니다.")},
    {TEXT("Training reset. AI result NOT SAVED: open LAST RESULT to retry or discard."),TEXT("연습을 초기화했습니다. AI 결과가 저장되지 않았어요. 최근 결과에서 저장을 다시 시도하거나 버려 주세요.")},
    {TEXT("Disarm manual aircraft before entering an audience/referee view."),TEXT("관중·심판 시점으로 바꾸려면 먼저 수동 기체의 모터를 꺼 주세요.")},
    {TEXT("FIDA Class 40 LOS match: 10-second ready signal, then 3-minute sets. Manual control."),TEXT("FIDA Class 40 육안 경기: 준비 10초 후 3분 세트가 시작됩니다. 직접 조종해 주세요.")},
    {TEXT("Set ended. Manual landing control retained; new ARM and autonomous aids are locked."),TEXT("세트 종료 · 직접 조종해 착륙하세요. 새 시동과 자율비행은 잠겼습니다.")},
    {TEXT("READY / MOTORS LOCKED"),TEXT("경기 준비 / 시동 잠김")},
    {TEXT("SET %d / FIDA CLASS 40"),TEXT("%d세트 / FIDA Class 40")},
    {TEXT("Roll"),TEXT("롤")},
    {TEXT("Pitch"),TEXT("피치")},
    {TEXT("Yaw"),TEXT("러더")},
    {TEXT("MAINTENANCE / 5 MIN"),TEXT("정비 시간 / 5분")},
    {TEXT("MATCH FINISHED"),TEXT("경기 종료")},
    {TEXT("PRACTICE / FIDA CLASS 40"),TEXT("연습 / FIDA Class 40")},
    {TEXT("GOAL! Blue %d : %d Red. Scoring team must all retreat."),TEXT("골! 파랑 %d : %d 빨강 · 득점 팀은 모두 자기 진영으로 돌아오세요.")},
    {TEXT("Telemetry CSV exported to Saved/Reports."),TEXT("현재 기체 상태를 Reports 폴더에 CSV로 저장했습니다.")},
    {TEXT("Telemetry export failed."),TEXT("기체 상태 저장에 실패했습니다. 저장 폴더를 확인해 주세요.")},
    {TEXT("Flight recording saved in Saved/FlightLogs."),TEXT("비행 기록을 FlightLogs 폴더에 저장했습니다.")},
    {TEXT("Recording actual controls and physics. F6 stops recording."),TEXT("조종 입력과 비행 상태를 기록 중입니다. F6으로 기록을 마치세요.")},
    {TEXT("Flight recording stopped: could not write to Saved/FlightLogs."),TEXT("비행 기록이 중단됐습니다. FlightLogs 폴더에 저장할 수 없습니다.")},
    {TEXT("IW-%02d guard contact: %.0f%% integrity, %.1f J estimated dissipated work."),TEXT("IW-%02d 가드 충돌 · 내구도 %.0f%% · 추정 흡수 에너지 %.1f J")},
    {TEXT("Fleet landing complete. All motors disarmed."),TEXT("전체 착륙 완료 · 모든 모터를 껐습니다.")},
    {TEXT("Stop AI practice and land before starting an official manual match."),TEXT("공식 수동 경기 전에 AI 연습을 멈추고 착륙해 주세요.")},
    {TEXT("AI practice uses the blue striker. Stop AI or RESET before changing aircraft."),TEXT("AI 연습에서는 파란 공격수를 조종합니다. 기체 변경 전 AI를 멈추거나 초기화해 주세요.")},
    {TEXT("SDACS timeout. Local hold; reconnect does not resume mission or return."),TEXT("자율비행 연결이 끊겨 위치를 유지합니다. 재연결만으로 임무·귀환이 재개되지 않습니다.")},
    {TEXT("GUIDE READY: Enable USB explicitly again; keyboard fallback cannot resume this guide."),TEXT("USB 조종기를 명시적으로 다시 활성화해 주세요. 키보드로 자동 전환해 재개하지 않습니다.")},
    {TEXT("FIRST FLIGHT / STEP-BY-STEP GUIDE"),TEXT("첫 비행 / 한 단계씩 안내")},
    {TEXT("FIRST FLIGHT / UNGRADED INSTRUCTIONS"),TEXT("첫 비행 순서 안내 · 비행 자동 채점 없음")},
    {TEXT("START GUIDANCE / MOTORS STAY OFF"),TEXT("안내 시작 / 시동은 별도")},
    {TEXT("I CHECKED THIS / NEXT"),TEXT("직접 확인 / 다음")},
    {TEXT("GUIDE FROM START"),TEXT("처음부터 안내")},
    {TEXT("CHECK INPUT / RESTART GUIDE"),TEXT("입력 확인 / 이륙부터 다시 안내")},
    {TEXT("GUIDE NOTE: Flight steps are your own checks; only the final goal uses the rules score."),TEXT("비행 단계는 직접 확인합니다. 마지막 골만 기존 규칙의 승인 득점으로 확인합니다.")},
    {TEXT("GUIDE RETRY: No rush. Check the instruction and input state, then try again."),TEXT("괜찮아요. 안내와 입력 상태를 확인한 뒤 다시 눌러 주세요.")},
    {TEXT("GUIDE READY: Return to manual Practice first."),TEXT("준비: 수동 연습(Practice)으로 돌아와 주세요.")},
    {TEXT("GUIDE READY: Select view 1, outside-court LOS."),TEXT("준비: 경기장 밖에서 보는 1번 육안 시점을 선택해 주세요.")},
    {TEXT("GUIDE READY: Select your team's striker, IW-01 or WG-01."),TEXT("준비: 팀 공격수 IW-01 또는 WG-01을 선택해 주세요.")},
    {TEXT("GUIDE READY: Stop AI and disarm the other aircraft."),TEXT("준비: 자율비행을 멈추고 다른 기체의 모터를 꺼 주세요.")},
    {TEXT("GUIDE READY: Check input in controller settings, then close the menu."),TEXT("준비: 조종기 설정에서 입력을 확인한 뒤 설정창을 닫아 주세요.")},
    {TEXT("GUIDE READY: Land on the court and DISARM first."),TEXT("준비: 경기장 바닥에 착륙한 뒤 모터를 꺼 주세요.")},
    {TEXT("GUIDE READY: Lower throttle completely."),TEXT("준비: 스로틀을 맨 아래로 내려 주세요.")},
    {TEXT("GUIDE READY: USB active. Press START; ARM remains separate."),TEXT("USB 입력 준비: 안내를 시작해 주세요. 시동은 따로 켭니다.")},
    {TEXT("GUIDE READY: Keyboard active. Press START; ARM remains separate."),TEXT("키보드 입력 준비: 안내를 시작해 주세요. 시동은 따로 켭니다.")},
    {TEXT("GUIDE PAUSE: Aircraft or reset changed. Recheck input, then restart guidance."),TEXT("기체 또는 초기화 상태가 바뀌었어요. 입력을 확인한 뒤 안내를 다시 시작해 주세요.")},
    {TEXT("GUIDE PAUSE: Input changed. Reconnect explicitly, then restart guidance."),TEXT("입력이 바뀌었어요. 조종기를 다시 활성화한 뒤 안내를 다시 시작해 주세요.")},
    {TEXT("GUIDE PAUSE: Settings opened. Close the menu and recheck readiness."),TEXT("설정을 열어 안내를 멈췄어요. 창을 닫고 입력 준비를 다시 확인해 주세요.")},
    {TEXT("GUIDE PAUSE: Return to Practice before restarting guidance."),TEXT("수동 연습으로 돌아온 뒤 안내를 다시 시작해 주세요.")},
    {TEXT("GUIDE PAUSE: Return to LOS view 1 before restarting guidance."),TEXT("1번 육안 시점으로 돌아온 뒤 안내를 다시 시작해 주세요.")},
    {TEXT("GUIDE PAUSE: Stop automatic flight before restarting guidance."),TEXT("자율비행을 멈춘 뒤 안내를 다시 시작해 주세요.")},
    {TEXT("GUIDE PAUSE: Input was reset. Land, lower throttle and explicitly restart."),TEXT("입력 중단으로 멈췄어요. 착륙·모터 정지·입력을 확인하고 안내를 다시 시작해 주세요.")},
    {TEXT("GUIDE: Check your input and lower throttle before starting."),TEXT("입력을 확인하고 스로틀을 내린 뒤 안내를 시작해 주세요.")},
    {TEXT("GUIDE: ARM separately, then raise throttle slowly to take off."),TEXT("시동을 따로 켜고 스로틀을 천천히 올려 이륙해 보세요.")},
    {TEXT("GUIDE: Keep the aircraft in one place using throttle and small stick movements."),TEXT("스로틀과 작은 스틱 움직임으로 한곳에 떠 있어 보세요.")},
    {TEXT("GUIDE: Move toward the centre line; confirm the aircraft nose direction first."),TEXT("기체 앞쪽을 확인한 뒤 중앙선 쪽으로 천천히 이동해 보세요.")},
    {TEXT("GUIDE: Lower throttle slowly, land on the court, then DISARM."),TEXT("스로틀을 천천히 내려 바닥에 착륙한 뒤 모터를 꺼 주세요.")},
    {TEXT("GUIDE: ARM separately and fly your striker fully through the opponent's goal."),TEXT("시동을 따로 켠 뒤 공격수 기체로 상대 골대를 완전히 통과해 보세요.")},
    {TEXT("GUIDE: The rules accepted a goal. Land and DISARM before another attempt."),TEXT("규칙에 맞는 골을 확인했어요. 다시 연습하기 전에 착륙하고 모터를 꺼 주세요.")},
    {TEXT("GUIDE LIVE: %s | %s | height %.2f m | speed %.2f m/s"),TEXT("현재 %s · %s · 높이 %.2f m · 속도 %.2f m/s")},
    {TEXT("BASIC RESPONSE"), TEXT("기본 반응 설정")},
    {TEXT("ADVANCED RATES / EXPO"), TEXT("고급 레이트 / Expo")},
    {TEXT("GENTLE / LEARNING"), TEXT("부드럽게 / 연습용")},
    {TEXT("STANDARD RESPONSE"), TEXT("표준 반응")},
    {TEXT("Choose a response, then SAVE & RETURN. Your saved response stays active until you save."), TEXT("반응을 고른 뒤 ‘저장하고 돌아가기’를 누르세요. 저장 전에는 기존 설정이 유지됩니다.")},
    {TEXT("Softer near centre, slower turns."), TEXT("스틱 중앙은 부드럽게, 회전은 천천히.")},
    {TEXT("Original response, without Expo."), TEXT("기존 기본 반응입니다. Expo를 사용하지 않습니다.")},
    {TEXT("CURRENT PREVIEW / YOUR CUSTOM VALUES ARE KEPT"), TEXT("현재 미리 보기 / 직접 맞춘 설정도 그대로 유지합니다")},
    {TEXT("Throttle and 35-degree ANGLE limit are unchanged. Fine tuning is in ADVANCED RATES / EXPO."), TEXT("스로틀과 앵글 35도 한도는 그대로입니다. 세부 조정은 ‘고급 레이트 / Expo’에서 합니다.")},
    {TEXT("RADIO / USB CONTROLLER"), TEXT("조종기 / USB 설정")},
    {TEXT("ENABLE RADIO"), TEXT("조종기 사용")},
    {TEXT("THROTTLE"), TEXT("스로틀(추력)")},
    {TEXT("YAW"), TEXT("러더(방향)")},
    {TEXT("ROLL"), TEXT("롤(좌우 기울기)")},
    {TEXT("PITCH"), TEXT("피치(앞뒤 기울기)")},
    {TEXT("LEFT"), TEXT("왼쪽")},
    {TEXT("RIGHT"), TEXT("오른쪽")},
    {TEXT("FORWARD"), TEXT("앞쪽")},
    {TEXT("FORWARD / BACK"), TEXT("앞 / 뒤")},
    {TEXT("UP / DOWN"), TEXT("위 / 아래")},
    {TEXT("LEFT / RIGHT"), TEXT("좌 / 우")},
    {TEXT("DOWN (minimum)"), TEXT("맨 아래(최저)")},
    {TEXT("NORMAL"), TEXT("정방향")},
    {TEXT("INVERTED"), TEXT("반전")},
    {TEXT("CLOSE"), TEXT("닫기")},
    {TEXT("CANCEL"), TEXT("취소")},
    {TEXT("CONNECTED"), TEXT("연결됨")},
    {TEXT("DISCONNECTED"), TEXT("연결 끊김")},
    {TEXT("REFRESH"), TEXT("새로 찾기")},
    {TEXT("NEXT DEVICE"), TEXT("다음 장치")},
    {TEXT("SIMPLE VIEW"), TEXT("간단 설정")},
    {TEXT("ADVANCED OPTIONS"), TEXT("고급 설정")},
    {TEXT("CHANNEL ASSIGNMENT"), TEXT("측정한 조종 축")},
    {TEXT("LIVE STICK AXES"), TEXT("실시간 입력")},
    {TEXT("CHOOSE YOUR USB CONTROLLER"), TEXT("사용할 조종기를 선택하세요")},
    {TEXT("WAITING FOR STICK"), TEXT("스틱 확인 대기")},
    {TEXT("FOUR DISTINCT AXES ASSIGNED"), TEXT("네 조종 축이 서로 다릅니다")},
    {TEXT("ASSIGN FOUR DIFFERENT AXES"), TEXT("서로 다른 네 축을 선택하세요")},
    {TEXT("STICK SETUP FINDS THE FOUR AXES"), TEXT("스틱 설정으로 실제 네 축을 찾습니다")},
    {TEXT("CANCEL SETUP"), TEXT("설정 취소")},
    {TEXT("START THIS STEP"), TEXT("이 단계 시작")},
    {TEXT("CONFIRM HELD END / ENTER"), TEXT("끝 위치 확인 / Enter")},
    {TEXT("RETRY THIS STEP"), TEXT("이 단계 다시")},
    {TEXT("CAPTURE NEUTRAL"), TEXT("중립 / 최저 확인")},
    {TEXT("CONFIRM & SAVE"), TEXT("확인하고 저장")},
    {TEXT("MODE 1"), TEXT("모드 1")},
    {TEXT("MODE 2"), TEXT("모드 2")},
    {TEXT("5 / 5  NEUTRAL AND MINIMUM THROTTLE"), TEXT("5 / 5  스틱 중립과 최저 스로틀")},
    {TEXT("MANUAL AXIS AND RANGE SETUP"), TEXT("고급: 축과 이동 범위 직접 설정")},
    {TEXT("1  START RANGE"), TEXT("1  범위 수집 시작")},
    {TEXT("2  CAPTURE RANGE"), TEXT("2  범위 확인")},
    {TEXT("3  CAPTURE NEUTRAL"), TEXT("3  중립 확인")},
    {TEXT("4  FINISH & SAVE"), TEXT("4  완료 / 저장")},
    {TEXT("CONNECT SAVED RADIO"), TEXT("저장한 조종기 연결")},
    {TEXT("THROTTLE: PRECISION"), TEXT("스로틀: 정밀 반응")},
    {TEXT("THROTTLE: DIRECT"), TEXT("스로틀: 직접 반응")},
    {TEXT("THROTTLE: LOCKED DURING SETUP"), TEXT("설정 중 스로틀 변경 잠김")},
    {TEXT("RATES / EXPO / ARM BUTTON"), TEXT("반응 속도 / 곡선 / 시동 버튼")},
    {TEXT("USE KEYBOARD"), TEXT("키보드 사용")},
    {TEXT("SAVE PROFILE"), TEXT("설정 저장")},
    {TEXT("LOAD PROFILE"), TEXT("설정 불러오기")},
    {TEXT("USB RADIO ACTIVE"), TEXT("USB 조종 중")},
    {TEXT("KEYBOARD ACTIVE"), TEXT("키보드 조종 중")},
    {TEXT("EXPORT DEVICE REPORT"), TEXT("장치 진단 저장")},
    {TEXT("LEFT / THROTTLE + YAW"), TEXT("왼쪽: 스로틀 / 러더")},
    {TEXT("LEFT / PITCH + YAW"), TEXT("왼쪽: 피치 / 러더")},
    {TEXT("RIGHT / THROTTLE + ROLL"), TEXT("오른쪽: 스로틀 / 롤")},
    {TEXT("RIGHT / PITCH + ROLL"), TEXT("오른쪽: 피치 / 롤")},
    {TEXT("USB controller service is unavailable."), TEXT("조종기 입력 서비스를 사용할 수 없습니다.")},
    {TEXT("STEP 3 / 4  REVIEW THE MEASURED AXES, THEN CLICK CONFIRM & SAVE"), TEXT("3 / 4  실제 측정 축을 검토한 뒤 ‘확인하고 저장’을 누르세요.")},
    {TEXT("STEP 2 / 4  FOLLOW THE STICK GUIDE BELOW; ENTER CONFIRMS EACH HELD END"), TEXT("2 / 4  아래 안내대로 스틱을 움직이고 끝 위치에서 Enter를 누르세요.")},
    {TEXT("STEP 1 / 4  SELECT YOUR CONNECTED RADIO BELOW; NEVER GUESS THE AXES"), TEXT("1 / 4  연결한 조종기를 아래에서 선택하세요. 축은 실제 움직임으로 확인합니다.")},
    {TEXT("STEP 2 / 4  CHOOSE MODE 1 OR MODE 2, THEN STICK SETUP; FOLLOW EACH STICK GUIDE"), TEXT("2 / 4  실제 스틱 배치의 모드를 고른 뒤 ‘스틱 설정’을 시작하세요.")},
    {TEXT("STEP 4 / 4  LOWER THROTTLE, THEN CLICK ENABLE RADIO; MOTORS STAY DISARMED"), TEXT("4 / 4  스로틀을 최저로 내리고 ‘조종기 사용’을 누르세요. 모터는 정지 상태입니다.")},
    {TEXT("READY  CLOSE THIS PANEL, THEN SPACE ARMS AT LOW THROTTLE"), TEXT("준비 완료: 창을 닫고 최저 스로틀에서 Space로 별도 시동합니다.")},
    {TEXT("No joystick: connect USB DATA and select USB Joystick on the radio."), TEXT("USB 데이터 케이블을 연결하고 송신기에서 USB Joystick을 선택하세요.")},
    {TEXT("The device name shown here comes directly from Windows."), TEXT("Windows가 제공하는 장치 이름입니다.")},
    {TEXT("1. Power on the radio and use a USB DATA cable."), TEXT("1. 송신기를 켜고 USB 데이터 케이블로 연결하세요.")},
    {TEXT("2. Select USB JOYSTICK on the radio screen."), TEXT("2. 송신기 화면에서 USB Joystick을 선택하세요.")},
    {TEXT("3. Click REFRESH when Windows detects it."), TEXT("3. Windows가 인식하면 ‘새로 찾기’를 누르세요.")},
    {TEXT("Windows test: Win+R, then joy.cpl"), TEXT("Windows 확인: Win+R → joy.cpl")},
    {TEXT("More devices: use NEXT DEVICE above."), TEXT("다른 장치는 위의 ‘다음 장치’로 선택하세요.")},
    {TEXT("Press START THIS STEP or ENTER when ready."), TEXT("준비되면 ‘이 단계 시작’ 또는 Enter를 누르세요.")},
    {TEXT("Move only this direction through its full travel. Hold EACH end for 1 second."), TEXT("안내한 방향만 양 끝까지 움직이고, 각 끝에서 1초씩 유지하세요.")},
    {TEXT("Keep still for 1 second, then CAPTURE NEUTRAL or ENTER."), TEXT("1초간 멈춘 뒤 ‘중립 / 최저 확인’ 또는 Enter를 누르세요.")},
    {TEXT("The measured axes and directions appear above. Keep throttle fully down."), TEXT("위의 측정 축과 방향을 검토하세요. 스로틀은 계속 최저로 두세요.")},
    {TEXT("CONFIRM & SAVE stores this device profile. ENABLE RADIO follows separately."), TEXT("‘확인하고 저장’ 후 ‘조종기 사용’을 별도로 눌러야 합니다.")},
    {TEXT("CLOSE cancels unfinished setup. Previous saved calibration is retained until final save."), TEXT("닫으면 미완료 설정을 취소합니다. 최종 저장 전까지 기존 보정을 보존합니다.")},
    {TEXT("1  START, then move all four sticks through their full ranges. CAPTURE RANGE when finished."), TEXT("1  범위 수집을 시작하고 네 조종 축을 양 끝까지 움직인 뒤 범위를 확인하세요.")},
    {TEXT("2  Center roll / pitch / yaw and move throttle fully down. CAPTURE NEUTRAL, then FINISH & SAVE."), TEXT("2  롤·피치·러더 중립, 스로틀 최저에서 중립을 확인하고 완료 / 저장하세요.")},
    {TEXT("YOUR CONTROLLER IS CALIBRATED. Lower throttle, then ENABLE RADIO."), TEXT("보정이 완료됐습니다. 스로틀을 최저로 내리고 ‘조종기 사용’을 누르세요.")},
    {TEXT("Choose your transmitter's physical layout, then STICK SETUP > REVIEW / SAVE > ENABLE."), TEXT("실제 스틱 배치를 고른 뒤 스틱 설정 → 검토 / 저장 → 조종기 사용 순서로 진행하세요.")},
    {TEXT("A layout change needs fresh setup. To reuse saved settings, keep the saved mode."), TEXT("모드를 바꾸면 다시 보정합니다. 기존 보정을 쓰려면 저장한 모드를 유지하세요.")},
    {TEXT("Low throttle required. This does not arm the aircraft."), TEXT("최저 스로틀이 필요합니다. 기체 시동은 별도입니다.")},
    {TEXT("First use: SELECT DEVICE > CHOOSE MODE > STICK SETUP > SAVE > ENABLE RADIO."), TEXT("처음 사용: 장치 선택 → 모드 선택 → 스틱 설정 → 저장 → 조종기 사용")},
    {TEXT("Rudder turns the nose; throttle controls motor thrust."), TEXT("러더는 기수 방향, 스로틀은 모터 추력을 조절합니다.")},
    {TEXT("Rudder turns the nose; throttle controls motor thrust. Recheck every new PC or device."), TEXT("러더는 기수 방향, 스로틀은 추력입니다. 새 PC·조종기는 다시 확인하세요.")},
    {TEXT("Axis assignment appears only after actual measurement or a valid saved profile."), TEXT("축 배정은 실제 측정이나 유효한 저장 보정 후에 표시됩니다.")},
    {TEXT("Language could not be saved. Check your Windows user-data folder permissions."), TEXT("언어를 저장하지 못했습니다. Windows 사용자 폴더 쓰기 권한을 확인하세요.")},
    {TEXT("CONTROL RESPONSE / ANGLE"), TEXT("조종 반응 / ANGLE")},
    {TEXT("Roll/Pitch rate limits tilt response; full stick still targets 35 deg. Yaw rate is deg/s."), TEXT("롤·피치 속도는 기울기 반응을 조절합니다. 최대 목표 기울기는 35°, 러더 단위는 °/초입니다.")},
    {TEXT("Expo softens the centre; minimum/maximum stick positions stay unchanged."), TEXT("곡선(Expo)은 중립 부근을 부드럽게 합니다. 양 끝 입력은 유지합니다.")},
    {TEXT("ARM BUTTON: NOT ASSIGNED"), TEXT("시동 버튼: 미지정")},
    {TEXT("SELECTED CONTROLLER"), TEXT("선택한 조종기")},
    {TEXT("SAVED OTHER CONTROLLER"), TEXT("다른 조종기에 저장됨")},
    {TEXT("CANCEL LEARNING"), TEXT("버튼 학습 취소")},
    {TEXT("LEARN ARM BUTTON"), TEXT("시동 버튼 학습")},
    {TEXT("REMOVE BINDING"), TEXT("버튼 배정 삭제")},
    {TEXT("TOGGLE BUTTON"), TEXT("누를 때마다 전환")},
    {TEXT("2-POSITION SWITCH"), TEXT("2단 스위치")},
    {TEXT("Release the ARM button / switch OFF, then press it ON. Keep other switches unchanged."), TEXT("시동 버튼을 놓거나 스위치를 OFF로 둔 뒤 ON으로 바꾸세요. 다른 스위치는 유지하세요.")},
    {TEXT("Each press toggles ARM/DISARM. ARM needs low throttle and a fresh press."), TEXT("누를 때마다 시동 / 정지합니다. 시동에는 최저 스로틀과 새 버튼 누름이 필요합니다.")},
    {TEXT("Switch ON arms at low throttle. Switch OFF disarms. OFF required after reconnect."), TEXT("ON: 최저 스로틀에서 시동 / OFF: 정지. 재연결 후 먼저 OFF로 두세요.")},
    {TEXT("RESET RATES / EXPO"), TEXT("반응 속도 / 곡선 초기화")},
    {TEXT("SAVE & RETURN"), TEXT("저장하고 돌아가기")},
    {TEXT("30-360 deg/s / Expo 0-80%. USB is disabled after save; enable again at low throttle."), TEXT("속도 30~360°/초 / 곡선 0~80%. 저장 후 USB 해제: 최저 스로틀에서 다시 사용하세요.")},
    {TEXT("Changes are a preview until SAVE. CANCEL keeps your saved settings."), TEXT("저장 전에는 미리 보기입니다. 취소하면 기존 설정을 보존합니다.")},
    {TEXT("Controller disconnected or changed. Previous binding kept."), TEXT("조종기가 끊기거나 변경됐습니다. 이전 버튼 배정을 보존합니다.")},
    {TEXT("Button learned. Choose TOGGLE or SWITCH, then SAVE. Motors stay disarmed."), TEXT("버튼을 확인했습니다. 전환 방식 또는 스위치를 고른 뒤 저장하세요. 모터는 정지 상태입니다.")},
    {TEXT("Move only the intended ARM button: OFF / release, then ON / press."), TEXT("배정할 시동 버튼만 조작하세요: OFF / 놓기 → ON / 누르기.")},
    {TEXT("Connect and select a controller before learning an ARM button."), TEXT("조종기를 연결하고 선택한 뒤 시동 버튼을 학습하세요.")},
    {TEXT("OK"), TEXT("확인")},
    {TEXT("Joystick disconnected or input stale. Select/reconnect and start again."), TEXT("입력이 끊기거나 만료됐습니다. 조종기를 선택·재연결하고 다시 시작하세요.")},
    {TEXT("Select an axis from X, Y, Z, R, U, V for each channel."), TEXT("각 조종 채널에 X·Y·Z·R·U·V 중 실제 확인한 축을 배정하세요.")},
    {TEXT("Roll, pitch, yaw and throttle must use four different axes."), TEXT("롤·피치·러더·스로틀은 서로 다른 네 축이어야 합니다.")},
    {TEXT("A mapped axis is not reported by the selected joystick."), TEXT("배정한 축을 선택 조종기가 제공하지 않습니다.")},
    {TEXT("Deadzone must be finite and between 0.00 and 0.25."), TEXT("중립 무시 구간은 0~25%의 유효한 값이어야 합니다.")},
    {TEXT("Move the sticks through their full travel for longer before capturing neutral."), TEXT("중립 확인 전 스틱을 양 끝까지 더 천천히 움직여 주세요.")},
    {TEXT("Each mapped axis needs at least 25% of the driver range. Sweep every stick fully."), TEXT("각 축에 충분한 이동 범위가 필요합니다. 모든 스틱을 양 끝까지 움직이세요.")},
    {TEXT("Center roll, pitch and yaw; neutral must be within the middle 60% of measured travel."), TEXT("롤·피치·러더를 중립에 두세요. 중립은 측정 범위 가운데 60% 안이어야 합니다.")},
    {TEXT("Move physical throttle to minimum. Check the throttle axis and inversion."), TEXT("스로틀을 물리적 최저로 내리세요. 스로틀 축과 반전 방향을 확인하세요.")},
    {TEXT("Start range capture before collecting calibration."), TEXT("먼저 범위 수집을 시작하세요.")},
    {TEXT("Full stick travel has not been captured."), TEXT("스틱 전체 이동 범위를 아직 확인하지 못했습니다.")},
    {TEXT("Capture centered roll/pitch/yaw with physical throttle at minimum."), TEXT("롤·피치·러더 중립, 스로틀 최저에서 중립을 확인하세요.")},
    {TEXT("Finish and save calibration before enabling USB input."), TEXT("보정을 완료하고 저장한 뒤 조종기를 사용하세요.")},
    {TEXT("Input contains an invalid value; calibration was rejected."), TEXT("유효하지 않은 입력이 있어 보정을 거부했습니다.")},
    {TEXT("More than one axis moved. Retry with only the indicated stick direction."), TEXT("여러 축이 함께 움직였습니다. 안내한 스틱 방향만 움직여 다시 시도하세요.")},
    {TEXT("Hold the indicated final end, then confirm this step."), TEXT("안내한 마지막 끝 위치를 유지한 채 확인하세요.")},
    {TEXT("Keep the sticks still for a moment before confirming."), TEXT("스틱을 잠시 멈춘 뒤 확인하세요.")},
    {TEXT("Sweep BOTH sides of the starting center. Retry from a centered stick."), TEXT("중립의 양쪽 끝까지 모두 움직여야 합니다. 중립에서 다시 시작하세요.")},
    {TEXT("Choose Mode 1 or Mode 2, then calibrate the actual stick layout."), TEXT("모드 1 또는 2를 고르고 실제 스틱 배치를 보정하세요.")},
    {TEXT("Unknown calibration error."), TEXT("알 수 없는 보정 오류입니다.")},
    {TEXT("USB input stopped."), TEXT("USB 입력을 중지했습니다.")},
    {TEXT("Joystick input timed out. USB disabled; lower throttle and enable again."), TEXT("입력이 만료되어 USB를 해제했습니다. 최저 스로틀에서 다시 사용하세요.")},
    {TEXT("Selected joystick disconnected. USB disabled; reconnect then enable explicitly."), TEXT("선택한 조종기가 끊겨 USB를 해제했습니다. 재연결 후 별도로 사용을 누르세요.")},
    {TEXT("Joystick identity changed. Select the device and calibrate again."), TEXT("조종기 식별값이 바뀌었습니다. 장치를 선택하고 다시 보정하세요.")},
    {TEXT("No connected USB joystick. Keyboard input remains active."), TEXT("연결된 USB 조종기가 없습니다. 키보드를 사용합니다.")},
    {TEXT("USB joystick found. Select the actual device; keyboard input remains active."), TEXT("USB 조종기를 찾았습니다. 사용할 장치를 선택하세요. 현재 키보드 조종입니다.")},
    {TEXT("The requested joystick is not connected. Keyboard input remains active."), TEXT("선택한 조종기가 연결되지 않았습니다. 키보드를 사용합니다.")},
    {TEXT("Selecting joystick. Keyboard input remains active."), TEXT("조종기를 선택합니다. 현재 키보드 조종입니다.")},
    {TEXT("Selected joystick unavailable. USB disabled."), TEXT("선택한 조종기를 사용할 수 없어 USB를 해제했습니다.")},
    {TEXT("Stick setup interrupted by input loss. Previous saved profile retained; restart STICK SETUP."), TEXT("입력 손실로 설정이 중단됐습니다. 기존 보정을 보존했습니다. 스틱 설정을 다시 시작하세요.")},
    {TEXT("Joystick live. USB disabled; calibrate or lower throttle and enable explicitly."), TEXT("실시간 입력이 수신됩니다. 보정하거나 최저 스로틀에서 조종기 사용을 누르세요.")},
    {TEXT("Native USB joystick input is available in the Windows build."), TEXT("USB 조종기 입력은 Windows 실행본에서 지원합니다.")},
    {TEXT("Joystick disconnected or stale. USB disabled; keyboard input required."), TEXT("조종기 입력이 끊기거나 만료되어 USB를 해제했습니다. 키보드로 복귀하세요.")},
    {TEXT("Pilot controls file invalid or unsupported. THROTTLE: DIRECT remains active."), TEXT("조종 반응 설정 파일이 유효하지 않습니다. 직접 스로틀 반응을 유지합니다.")},
    {TEXT("Finish or cancel calibration before changing throttle response."), TEXT("보정을 완료하거나 취소한 뒤 스로틀 반응을 바꾸세요.")},
    {TEXT("DISARM every manually controlled aircraft before changing throttle response."), TEXT("모든 수동 기체의 모터를 정지한 뒤 스로틀 반응을 바꾸세요.")},
    {TEXT("Could not save pilot controls. Previous throttle response remains unchanged."), TEXT("조종 반응을 저장하지 못했습니다. 이전 스로틀 반응을 유지합니다.")},
    {TEXT("Response settings invalid. Default rates/Expo and unassigned ARM button retained."), TEXT("반응 설정이 유효하지 않습니다. 기본 속도·곡선과 미지정 시동 버튼을 유지합니다.")},
    {TEXT("Invalid rate, Expo or ARM binding. Previous settings retained."), TEXT("속도·곡선 또는 시동 버튼이 유효하지 않습니다. 이전 설정을 유지합니다.")},
    {TEXT("Finish or cancel stick setup before editing response."), TEXT("스틱 설정을 완료하거나 취소한 뒤 반응을 바꾸세요.")},
    {TEXT("DISARM every manual aircraft before saving response or ARM binding."), TEXT("모든 수동 기체를 정지한 뒤 반응 또는 시동 버튼을 저장하세요.")},
    {TEXT("Select the connected controller and learn a button it reports."), TEXT("연결된 조종기를 선택하고 실제 입력되는 버튼을 학습하세요.")},
    {TEXT("Could not save response. Previous settings retained."), TEXT("반응 설정을 저장하지 못했습니다. 이전 설정을 유지합니다.")},
    {TEXT("Rates / Expo / ARM saved. USB disabled. Release ARM, lower throttle, then enable again."), TEXT("속도·곡선·시동 버튼을 저장했습니다. 버튼 해제와 최저 스로틀 후 다시 조종기를 사용하세요.")},
    {TEXT("Finish or cancel calibration before changing the stick layout."), TEXT("보정을 완료하거나 취소한 뒤 스틱 모드를 바꾸세요.")},
    {TEXT("Finish or cancel stick setup before editing channels."), TEXT("스틱 설정을 완료하거나 취소한 뒤 축을 바꾸세요.")},
    {TEXT("Mapping changed. USB disabled; capture full ranges and neutral again."), TEXT("축 배정이 바뀌어 USB를 해제했습니다. 전체 범위와 중립을 다시 보정하세요.")},
    {TEXT("Finish or cancel stick setup before changing deadzone."), TEXT("스틱 설정을 완료하거나 취소한 뒤 중립 무시 구간을 바꾸세요.")},
    {TEXT("Deadzone updated. USB disabled; save the profile and enable explicitly."), TEXT("중립 무시 구간이 바뀌었습니다. 저장한 뒤 조종기 사용을 별도로 누르세요.")},
    {TEXT("Stick setup cancelled. Previous calibration retained; keyboard active."), TEXT("스틱 설정을 취소했습니다. 이전 보정을 보존하고 키보드를 사용합니다.")},
    {TEXT("Capturing ranges: sweep every mapped stick fully both ways, then CAPTURE RANGE."), TEXT("범위 수집 중: 각 스틱을 양 끝까지 움직인 뒤 범위를 확인하세요.")},
    {TEXT("Ranges captured. Center roll/pitch/yaw, hold throttle minimum, then CAPTURE NEUTRAL."), TEXT("범위를 확인했습니다. 롤·피치·러더 중립, 스로틀 최저에서 중립을 확인하세요.")},
    {TEXT("Neutral and physical minimum throttle captured. FINISH & SAVE to confirm calibration."), TEXT("중립과 최저 스로틀을 확인했습니다. 완료 / 저장으로 보정을 확정하세요.")},
    {TEXT("Calibration saved for this device. Keyboard active; lower throttle and ENABLE USB explicitly."), TEXT("보정을 저장했습니다. 현재 키보드 조종입니다. 최저 스로틀에서 조종기 사용을 누르세요.")},
    {TEXT("Finish and save stick setup before enabling radio."), TEXT("스틱 설정을 완료하고 저장한 뒤 조종기를 사용하세요.")},
    {TEXT("Selected stick layout needs calibration. Complete STICK SETUP and save, or select the saved layout."), TEXT("선택한 스틱 모드는 보정이 필요합니다. 스틱 설정을 저장하거나 저장된 모드로 돌아가세요.")},
    {TEXT("USB input enabled. Aircraft remains disarmed until the pilot explicitly arms at low throttle."), TEXT("USB 조종을 사용합니다. 최저 스로틀에서 별도로 시동하기 전에는 모터가 정지 상태입니다.")},
    {TEXT("Finish or cancel calibration before connecting saved radio."), TEXT("보정을 완료하거나 취소한 뒤 저장한 조종기를 연결하세요.")},
    {TEXT("Stick layout changed. Complete STICK SETUP, or select the saved layout before CONNECT SAVED RADIO."), TEXT("스틱 모드가 바뀌었습니다. 다시 보정하거나 저장된 모드를 고른 뒤 연결하세요.")},
    {TEXT("No saved radio profile. Open C / RADIO to select and calibrate the device."), TEXT("저장한 조종기 보정이 없습니다. C / 조종기에서 장치를 선택하고 보정하세요.")},
    {TEXT("Select the connected radio explicitly in C / RADIO; no unique saved device found."), TEXT("저장 장치를 하나로 확인하지 못했습니다. C / 조종기에서 사용할 장치를 직접 선택하세요.")},
    {TEXT("Saved layout differs from your choice. Select the device and calibrate its actual layout before enabling."), TEXT("저장 모드와 선택한 모드가 다릅니다. 실제 장치를 선택하고 스틱 배치를 보정하세요.")},
    {TEXT("Keyboard input selected. USB calibration retained; enabling requires low throttle."), TEXT("키보드를 사용합니다. USB 보정은 보존되며 재사용하려면 최저 스로틀이 필요합니다.")},
    {TEXT("Device report saved in your user-data Diagnostics folder. No calibration was changed."), TEXT("사용자 데이터 Diagnostics 폴더에 장치 진단을 저장했습니다. 보정값은 유지됩니다.")},
    {TEXT("Cannot write user data. Check your Windows user folder permissions."), TEXT("사용자 데이터를 쓰지 못했습니다. Windows 사용자 폴더 권한을 확인하세요.")},
    {TEXT("Use CONFIRM & SAVE after completing stick setup."), TEXT("스틱 설정을 완료한 뒤 ‘확인하고 저장’을 누르세요.")},
    {TEXT("Cannot save another layout's calibration. Complete STICK SETUP for the selected mode."), TEXT("다른 스틱 모드의 보정을 저장할 수 없습니다. 선택한 모드로 스틱 설정을 완료하세요.")},
    {TEXT("Existing radio profile file is malformed or unsupported; it was left unchanged."), TEXT("기존 보정 파일이 손상됐거나 지원하지 않는 형식입니다. 파일을 보존했습니다.")},
    {TEXT("Device calibration saved for this Windows user. USB activation is never saved."), TEXT("현재 Windows 사용자용 보정을 저장했습니다. USB 사용 상태는 저장하지 않습니다.")},
    {TEXT("Could not save calibration. Check your Windows user-data folder permissions."), TEXT("보정을 저장하지 못했습니다. Windows 사용자 데이터 폴더 권한을 확인하세요.")},
    {TEXT("No saved calibration for this device. Keyboard active; map channels and start range capture."), TEXT("이 조종기의 저장 보정이 없습니다. 키보드 상태에서 스틱 설정을 시작하세요.")},
    {TEXT("Saved profile invalid or belongs to another device. Keyboard active; calibrate this device."), TEXT("보정이 유효하지 않거나 다른 장치용입니다. 현재 조종기를 새로 보정하세요.")},
    {TEXT("Guided capture is running. Sweep the indicated stick both ways, then hold the requested end."), TEXT("안내 수집 중입니다. 해당 스틱을 양 끝까지 움직인 뒤 안내한 끝을 유지하세요.")},
    {TEXT("Last axis captured. Center roll/pitch/yaw and hold throttle fully down."), TEXT("마지막 축을 확인했습니다. 롤·피치·러더 중립, 스로틀 최저로 두세요.")},
    {TEXT("Axis captured. Return sticks to center and throttle low before starting the next step."), TEXT("축을 확인했습니다. 다음 단계 전에 스틱 중립과 최저 스로틀로 돌아가세요.")},
    {TEXT("Neutral and low throttle captured. Check the four assignments above, then CONFIRM & SAVE."), TEXT("중립과 최저 스로틀을 확인했습니다. 위의 네 배정을 검토하고 확인 / 저장하세요.")},
    {TEXT("Precision throttle enabled. USB disabled; enable again at low throttle."), TEXT("정밀 스로틀을 사용합니다. USB를 해제했습니다. 최저 스로틀에서 다시 사용하세요.")},
    {TEXT("Direct throttle enabled. USB disabled; enable again at low throttle."), TEXT("직접 스로틀을 사용합니다. USB를 해제했습니다. 최저 스로틀에서 다시 사용하세요.")},
    {TEXT("%d device(s) found. Choose your radio below, or use NEXT DEVICE."), TEXT("장치 %d개를 찾았습니다. 사용할 조종기를 아래에서 선택하세요.")},
    {TEXT("%d AXES  /  %d BUTTONS  /  DRIVER ID %04X:%04X"), TEXT("%d축 / %d버튼 / 장치 ID %04X:%04X")},
    {TEXT("SLOT %d  /  %04X:%04X  /  %d AXES  /  %d BUTTONS"), TEXT("슬롯 %d / %04X:%04X / %d축 / %d버튼")},
    {TEXT("%s  /  %s"), TEXT("%s / %s")},
    {TEXT("AXIS %s"), TEXT("축 %s")},
    {TEXT("%d / 5  %s - %s STICK %s"), TEXT("%d / 5  %s - %s 스틱 %s")},
    {TEXT("REVIEW MODE %d ASSIGNMENTS"), TEXT("모드 %d 배정 검토")},
    {TEXT("MODE %d  /  %s THROTTLE"), TEXT("모드 %d / %s 스로틀")},
    {TEXT("Center roll / pitch / yaw. Hold %s throttle DOWN before starting."), TEXT("롤·피치·러더를 중립, %s 스로틀을 최저로 두고 시작하세요.")},
    {TEXT("Finish by holding the %s stick fully %s."), TEXT("마지막에는 %s 스틱을 %s 끝에 유지하세요.")},
    {TEXT("Release roll / pitch / yaw to their centers. Hold %s throttle fully DOWN."), TEXT("롤·피치·러더를 중립, %s 스로틀을 최저로 두세요.")},
    {TEXT("MODE %d  /  %d CURRENT-STEP SAMPLES"), TEXT("모드 %d / 현재 단계 실측 %d개")},
    {TEXT("MODE %d / FINISH RANGE SETUP FIRST"), TEXT("모드 %d / 먼저 범위 보정을 완료하세요")},
    {TEXT("STICK SETUP / MODE %d"), TEXT("스틱 설정 / 모드 %d")},
    {TEXT("DEADZONE  %.0f %%"), TEXT("중립 무시 %.0f %%")},
    {TEXT("MODE %d: LEFT %s + YAW  /  RIGHT %s + ROLL"), TEXT("모드 %d: 왼쪽 %s + 러더 / 오른쪽 %s + 롤")},
    {TEXT("RATE %.0f deg/s"), TEXT("반응 속도 %.0f °/초")},
    {TEXT("EXPO %.0f%%"), TEXT("곡선 %.0f%%")},
    {TEXT("IN %+.2f  OUT %+.2f"), TEXT("입력 %+.2f / 출력 %+.2f")},
    {TEXT("ARM BUTTON: %d  /  %s"), TEXT("시동 버튼: %d / %s")},
    {TEXT("Joystick read failed (%u). USB disabled; reconnect and enable explicitly."), TEXT("조종기 읽기 실패(%u). 재연결 후 조종기 사용을 별도로 누르세요.")},
    {TEXT("Mode %d selected. USB disabled; verify low throttle before enabling."), TEXT("모드 %d 선택. USB를 해제했습니다. 최저 스로틀을 확인하고 다시 사용하세요.")},
    {TEXT("Mode %d selected. Run STICK SETUP and save before enabling. Previous saved profile is unchanged."), TEXT("모드 %d 선택. 스틱 설정 후 저장하세요. 이전 저장 보정은 보존됩니다.")},
    {TEXT("Mode %d setup ready. Center roll/pitch/yaw, lower %s throttle, then START THIS STEP."), TEXT("모드 %d 설정 준비. 롤·피치·러더 중립, %s 스로틀 최저에서 이 단계를 시작하세요.")},
    {TEXT("Mode %d setup saved. Keyboard active; ENABLE RADIO is a separate action at low throttle."), TEXT("모드 %d 저장 완료. 현재 키보드 조종입니다. 최저 스로틀에서 조종기 사용을 누르세요.")},
    {TEXT("Mode %d device calibration loaded. Keyboard active; minimum throttle and explicit ENABLE USB required."), TEXT("모드 %d 보정을 불러왔습니다. 최저 스로틀에서 조종기 사용을 별도로 누르세요.")},
    {TEXT("IMPERIAL WINGS"), TEXT("IMPERIAL WINGS")},
    {TEXT("DRONE SOCCER  /  GWANGJU TRAINING CENTRE"), TEXT("드론축구 / 광주 훈련장")},
    {TEXT("BLUE"), TEXT("파랑")},
    {TEXT("RED"), TEXT("빨강")},
    {TEXT("OFFSIDE"), TEXT("오프사이드")},
    {TEXT("SESSION / SET CLOCK"), TEXT("세트 시간")},
    {TEXT("GUIDANCE SOURCE"), TEXT("훈련 유도 연결")},
    {TEXT("SDACS / APF LINKED"), TEXT("SDACS / APF 연결됨")},
    {TEXT("SDACS / OFFLINE"), TEXT("SDACS / 연결 안 됨")},
    {TEXT("LOCAL PHYSICS READY"), TEXT("로컬 물리 준비됨")},
    {TEXT("EXIT"), TEXT("종료")},
    {TEXT("AUTO FLEET"), TEXT("자율 훈련")},
    {TEXT("HOLD"), TEXT("위치 유지")},
    {TEXT("RETURN"), TEXT("귀환")},
    {TEXT("MANUAL"), TEXT("수동 조종")},
    {TEXT("RESET"), TEXT("초기화")},
    {TEXT("EXPORT CSV"), TEXT("CSV 저장")},
    {TEXT("ARM"), TEXT("시동")},
    {TEXT("DISARM"), TEXT("모터 정지")},
    {TEXT("RADIO / USB"), TEXT("조종기 / USB")},
    {TEXT("START MATCH"), TEXT("경기 시작")},
    {TEXT("TRAINING TELEMETRY"), TEXT("훈련 상태")},
    {TEXT("Aircraft in arena"), TEXT("경기장 기체")},
    {TEXT("Closest separation"), TEXT("가장 가까운 간격")},
    {TEXT("Close pairs < 0.9 m"), TEXT("0.9m 미만 근접 쌍")},
    {TEXT("16 x 7 x 4.5 m COURT"), TEXT("경기장 16 × 7 × 4.5m")},
    {TEXT("CAMERA"), TEXT("시점")},
    {TEXT("CAMERA VIEWS"), TEXT("시점 선택")},
    {TEXT("1  LOS"), TEXT("1 육안")},
    {TEXT("2  OBS"), TEXT("2 관찰")},
    {TEXT("3  ARENA"), TEXT("3 전체")},
    {TEXT("4 AUDIENCE"), TEXT("4 관중")},
    {TEXT("5 REFEREE"), TEXT("5 심판")},
    {TEXT("GRAPHICS"), TEXT("화질")},
    {TEXT("BALANCED"), TEXT("균형")},
    {TEXT("HIGH"), TEXT("높음")},
    {TEXT("CINEMA"), TEXT("최상")},
    {TEXT("F8 CONNECT SAVED RADIO"), TEXT("F8 저장한 조종기 연결")},
    {TEXT("F6 STOP RECORDING"), TEXT("F6 비행 기록 종료")},
    {TEXT("F6 RECORD FLIGHT"), TEXT("F6 비행 기록 시작")},
    {TEXT("5 v 5  /  TEAM ROSTER"), TEXT("5 대 5 / 팀 목록")},
    {TEXT("AIRCRAFT"), TEXT("기체")},
    {TEXT("CONTROL"), TEXT("조종")},
    {TEXT("ALT"), TEXT("고도")},
    {TEXT("AUTO"), TEXT("자율")},
    {TEXT("ANGLE"), TEXT("수평 복원")},
    {TEXT("DISARMED"), TEXT("모터 정지")},
    {TEXT("DISARM FIRST"), TEXT("먼저 정지")},
    {TEXT("F9 BACK TO LOS"), TEXT("F9 육안 복귀")},
    {TEXT("F9 EQUIPMENT"), TEXT("F9 기체 점검")},
    {TEXT("FLIGHT SPEED"), TEXT("비행 속도")},
    {TEXT("ALTITUDE"), TEXT("고도")},
    {TEXT("MOTORS"), TEXT("모터")},
    {TEXT("ARMED"), TEXT("시동됨")},
    {TEXT("SAFE"), TEXT("정지")},
    {TEXT("DISARM SELECTED"), TEXT("선택 기체 모터 정지")},
    {TEXT("ARM SELECTED"), TEXT("선택 기체 시동")},
    {TEXT("BATTERY EMPTY / NO MOTOR POWER"), TEXT("배터리 소진 / 모터 출력 없음")},
    {TEXT("SESSION EVENTS"), TEXT("진행 기록")},
    {TEXT("SPACE ARM   ARROWS THROTTLE   WASD PITCH / ROLL   QE YAW"), TEXT("Space 시동 / 정지   ↑↓ 스로틀   WASD 기울기   QE 러더")},
    {TEXT("TAB SELECT   1 LOS / 2 OBSERVER / 3 ARENA"), TEXT("Tab 기체 선택   1 육안 / 2 관찰 / 3 전체")},
    {TEXT("ENTER AUTO   H HOLD   R RETURN   C RADIO"), TEXT("Enter 자율   H 유지   R 귀환   C 조종기")},
    {TEXT("F10 PANELS  |  C USB"), TEXT("F10 패널 | C 조종기")},
    {TEXT("AI PRACTICE / SETTINGS & STOP"), TEXT("AI 훈련 / 설정 및 종료")},
    {TEXT("AI PRACTICE / UNSAVED RESULT"), TEXT("AI 훈련 / 미저장 결과")},
    {TEXT("AI PRACTICE / 3-4 DEFENDERS"), TEXT("AI 훈련 / 수비 3~4대")},
    {TEXT("Striker"), TEXT("스트라이커")},
    {TEXT("Guide"), TEXT("가이드")},
    {TEXT("Libero"), TEXT("리베로")},
    {TEXT("Sweeper"), TEXT("스위퍼")},
    {TEXT("Keeper"), TEXT("키퍼")},
    {TEXT("LOS PILOT"), TEXT("육안 조종자")},
    {TEXT("OBSERVER"), TEXT("외부 관찰")},
    {TEXT("ARENA OVERVIEW"), TEXT("경기장 전체")},
    {TEXT("AUDIENCE"), TEXT("관중")},
    {TEXT("REFEREE"), TEXT("심판")},
    {TEXT("KEYBOARD"), TEXT("키보드")},
    {TEXT("Precision throttle saved: softer near 55% input; height stays manual. USB disabled; reconnect explicitly."), TEXT("정밀 스로틀 저장: 입력 55% 부근을 부드럽게 합니다. 고도는 수동이며 조종기 사용은 별도입니다.")},
    {TEXT("Direct throttle saved. USB disabled; reconnect explicitly at low throttle."), TEXT("직접 스로틀을 저장했습니다. 최저 스로틀에서 조종기 사용을 별도로 누르세요.")},
    {TEXT("Move only the indicated direction. Hold both ends 1 second; hold the final end and confirm."), TEXT("안내한 방향만 움직이세요. 양 끝 1초 유지 후 마지막 끝 위치에서 확인하세요.")},
    {TEXT("BLUE  %d   :   %d  RED"), TEXT("파랑 %d : %d 빨강")},
    {TEXT("SETS  %d"), TEXT("세트 %d")},
    {TEXT("BAT %.0f%% %.1fV   GUARD %.0f%%"), TEXT("배터리 %.0f%% %.1fV / 보호구 %.0f%%")},
    {TEXT("POWER LIMITED / THRUST %.0f%%"), TEXT("출력 제한 / 추력 %.0f%%")},
    {TEXT("PHYSICS LIMITED: %llu frames | %.3f s omitted | match clock continues"), TEXT("물리 시간 제한: %llu프레임 / %.3f초 제외 / 경기 시간은 계속됩니다")},
    {TEXT("%s   %s   ALT %.2f m   THR %.0f%%   BAT %.0f%% / %.1f V"), TEXT("%s %s / 고도 %.2fm / 스로틀 %.0f%% / 배터리 %.0f%% / %.1fV")},
    {TEXT("STRIKER"), TEXT("스트라이커")},
    {TEXT("GUIDE"), TEXT("가이드")},
    {TEXT("LIBERO"), TEXT("리베로")},
    {TEXT("SWEEPER"), TEXT("스위퍼")},
    {TEXT("KEEPER"), TEXT("키퍼")},
    {TEXT("PILOT LOS"), TEXT("조종자 육안 시점")},
    {TEXT("OBSERVER TRACK"), TEXT("외부 관찰 시점")},
    {TEXT("AUDIENCE STANDS"), TEXT("관중석 시점")},
    {TEXT("REFEREE SIDELINE"), TEXT("심판 시점")},
    {TEXT("EQUIPMENT"), TEXT("기체 점검")},
    {TEXT("UNKNOWN"), TEXT("알 수 없음")},
    {TEXT("MANUAL TRAINING"), TEXT("수동 훈련")},
    {TEXT("LOS MANUAL MATCH"), TEXT("육안 수동 경기")},
    {TEXT("POSITION HOLD"), TEXT("위치 유지")},
    {TEXT("RETURNING"), TEXT("귀환 중")},
    {TEXT("SDACS AUTONOMOUS"), TEXT("SDACS 자율 훈련")},
    {TEXT("AI DEFENCE PRACTICE"), TEXT("AI 수비 훈련")},
    {TEXT("AI LANDING"), TEXT("AI 착륙 중")},
    {TEXT("AI DUEL LANDING"), TEXT("AI 대결 착륙 중")},
    {TEXT("AI DUEL / TRAINING VARIANT"), TEXT("AI 대결 / 훈련 규칙")},
    {TEXT("AI DUEL FINISHED / RESET TO TRAIN"), TEXT("AI 대결 종료 / 초기화 후 훈련")},
    {TEXT("AUTONOMOUS / ANGLE"), TEXT("자율 훈련 / ANGLE")},
    {TEXT("ANGLE / MANUAL"), TEXT("ANGLE / 수동 조종")},
    {TEXT("ANGLE / DISARMED"), TEXT("ANGLE / 모터 정지")},
    {TEXT("ARM BLOCKED: WAIT FOR SET START"), TEXT("시동 대기: 세트 시작을 기다리세요")},
    {TEXT("ARM BLOCKED: USB CONNECTION / CALIBRATION"), TEXT("시동 불가: USB 연결과 조종기 보정을 확인하세요")},
    {TEXT("ARM BLOCKED: CALIBRATE BOXER"), TEXT("시동 불가: 조종기 스틱을 먼저 보정하세요")},
    {TEXT("ARM BLOCKED: LOWER THROTTLE"), TEXT("시동 불가: 스로틀을 최저로 내리세요")},
    {TEXT("USB INPUT LOST / DISARMED"), TEXT("USB 입력 손실 / 모터 정지")},
    {TEXT("Switch to PILOT view before arming."), TEXT("조종자 시점으로 돌아간 뒤 시동하세요.")},
    {TEXT("Close equipment inspection before arming."), TEXT("기체 점검 화면을 닫은 뒤 시동하세요.")},
    {TEXT("AUTONOMY BLOCKED: PRACTICE ONLY"), TEXT("자율 조종은 연습 모드에서만 사용할 수 있습니다")},
    {TEXT("INPUT RESET / DISARMED. RELEASE KEYS, THEN ARM AGAIN."), TEXT("입력 초기화 / 모터 정지. 키를 놓고 다시 시동하세요.")},
    {TEXT("F1 FIRST FLIGHT GUIDE"),TEXT("F1 첫 비행 안내")},
    {TEXT("FIRST FLIGHT / KEYBOARD GUIDE"),TEXT("첫 비행 / 키보드 안내")},
    {TEXT("The first-flight guide is preparing. Try again when the arena is ready."),TEXT("첫 비행 안내를 준비하고 있어요. 경기장이 준비되면 다시 열어 주세요.")},
    {TEXT("Start with keyboard, the blue striker, and every aircraft landed and disarmed."),TEXT("키보드와 파란 공격수를 선택하고 모든 기체를 착륙·모터 정지한 뒤 시작하세요.")},
    {TEXT("Choose START GUIDE. It only observes: you control all motors and flight."),TEXT("‘안내 시작’을 눌러 주세요. 안내는 관찰만 하며 모터와 비행은 직접 조종합니다.")},
    {TEXT("MEASURED %.0f%%"),TEXT("측정 %.0f%%")},
    {TEXT("ALT %.2f m / SPEED %.2f m/s"),TEXT("높이 %.2fm / 속도 %.2fm/s")},
    {TEXT("NEXT STEP"),TEXT("다음 단계")},
    {TEXT("START GUIDE AGAIN"),TEXT("안내 다시 시작")},
    {TEXT("START GUIDE / MOTORS STAY OFF"),TEXT("안내 시작 / 시동은 별도로")},
    {TEXT("RESET BLUE STRIKER & RESTART"),TEXT("파란 공격수 초기화 / 다시 시작")},
};

struct FLanguageState
{
    bool bKorean = true;
    FLanguageState()
    {
        FString Value;
        if (FFileHelper::LoadFileToString(Value, *ImperialUserData::Path(TEXT("Config/Language.txt"))))
        {
            Value = Value.TrimStartAndEnd().ToLower();
            if (Value == TEXT("en")) bKorean = false;
        }
        // Launch overrides are session-only and never rewrite the saved preference.
        if (FParse::Value(FCommandLine::Get(), TEXT("ImperialLanguage="), Value))
        {
            Value = Value.TrimStartAndEnd().ToLower();
            if (Value == TEXT("ko") || Value == TEXT("en")) bKorean = Value == TEXT("ko");
        }
    }
};
FLanguageState& Language() { static FLanguageState State; return State; }

const TMap<FString, FString>& ExactTranslations()
{
    static const TMap<FString, FString> Values = []()
    {
        TMap<FString, FString> Result;
        for (const auto& Row : Translations) Result.Add(Row.English, Row.Korean);
        return Result;
    }();
    return Values;
}

/** Parse printf fields without evaluating a format or interpreting device text as code.
 *  The table accepts both format strings and already formatted runtime diagnostics.
 */
struct FDisplayFormat
{
    TArray<FString> Literals;
    TArray<TCHAR> Types;
    int32 LiteralLength = 0;
    bool Parse(const TCHAR* Pattern)
    {
        FString Literal;
        const FString Format(Pattern);
        for (int32 I = 0; I < Format.Len(); ++I)
        {
            if (Format[I] != TEXT('%')) { Literal += Format[I]; continue; }
            if (I + 1 < Format.Len() && Format[I + 1] == TEXT('%')) { Literal += TEXT('%'); ++I; continue; }
            Literals.Add(Literal); LiteralLength += Literal.Len(); Literal.Empty();
            int32 End = I + 1;
            // Only accept the printf syntax used by the display table. Do not scan
            // across arbitrary prose after a literal percent, such as "25% of".
            while (End < Format.Len() && FCString::Strchr(TEXT("-+0#"), Format[End])) ++End;
            while (End < Format.Len() && FChar::IsDigit(Format[End])) ++End;
            if (End < Format.Len() && Format[End] == TEXT('.'))
            {
                ++End;
                while (End < Format.Len() && FChar::IsDigit(Format[End])) ++End;
            }
            if (End < Format.Len() && FCString::Strchr(TEXT("hljztL"), Format[End]))
            {
                const TCHAR Length = Format[End++];
                if ((Length == TEXT('h') || Length == TEXT('l')) &&
                    End < Format.Len() && Format[End] == Length) ++End;
            }
            if (End == Format.Len() || !FCString::Strchr(TEXT("diuoxXfFeEgGaAcsp"), Format[End])) return false;
            Types.Add(Format[End]); I = End;
        }
        Literals.Add(Literal); LiteralLength += Literal.Len();
        return !Types.IsEmpty();
    }
    bool Match(const FString& Value, TArray<FString>& Arguments) const
    {
        if (!Literals[0].IsEmpty() && !Value.StartsWith(Literals[0], ESearchCase::CaseSensitive)) return false;
        int32 Position = Literals[0].Len();
        for (int32 I = 0; I < Types.Num(); ++I)
        {
            const FString& Following = Literals[I + 1];
            const int32 End = Following.IsEmpty() ? Value.Len()
                : Value.Find(Following, ESearchCase::CaseSensitive, ESearchDir::FromStart, Position);
            if (End < Position) return false;
            FString Argument = Value.Mid(Position, End - Position);
            if (Argument.IsEmpty()) return false;
            if (Types[I] != TEXT('s') && Types[I] != TEXT('c'))
                for (const TCHAR Character : Argument)
                    if (!FCString::Strchr(TEXT("0123456789abcdefABCDEFxX+-.eE"), Character)) return false;
            Arguments.Add(MoveTemp(Argument));
            Position = End + Following.Len();
        }
        return Position == Value.Len();
    }
};
struct FFormattedTranslation { FDisplayFormat English, Korean; };
const TArray<FFormattedTranslation>& FormattedTranslations()
{
    static const TArray<FFormattedTranslation> Values = []()
    {
        TArray<FFormattedTranslation> Result;
        for (const auto& Row : Translations)
        {
            FFormattedTranslation Candidate;
            if (Candidate.English.Parse(Row.English) && Candidate.Korean.Parse(Row.Korean) &&
                Candidate.English.Types.Num() == Candidate.Korean.Types.Num())
                Result.Add(MoveTemp(Candidate));
        }
        Result.Sort([](const auto& A, const auto& B) { return A.English.LiteralLength > B.English.LiteralLength; });
        return Result;
    }();
    return Values;
}
FString TranslateDisplay(const FString& Value, int32 Depth)
{
    if (const FString* Exact = ExactTranslations().Find(Value)) return *Exact;
    if (Depth >= 3) return Value;
    for (const auto& Row : FormattedTranslations())
    {
        TArray<FString> Arguments;
        if (!Row.English.Match(Value, Arguments)) continue;
        FString Result = Row.Korean.Literals[0];
        for (int32 I = 0; I < Arguments.Num(); ++I)
        {
            Result += Row.English.Types[I] == TEXT('s') ? TranslateDisplay(Arguments[I], Depth + 1) : Arguments[I];
            Result += Row.Korean.Literals[I + 1];
        }
        return Result;
    }
    return Value;
}
}

bool ImperialLocalization::IsKorean() { return Language().bKorean; }

bool ImperialLocalization::SetKorean(bool bKorean)
{
    const FString Path = ImperialUserData::Path(TEXT("Config/Language.txt"));
    // Shared Windows atomic replacement keeps an existing preference intact on failure.
    if (!ImperialProfileStorage::Save(Path, bKorean ? TEXT("ko") : TEXT("en")))
        return false;
    Language().bKorean = bKorean;
    return true;
}

FString ImperialLocalization::Translate(const FString& Value)
{
    return IsKorean() ? TranslateDisplay(Value, 0) : Value;
}
