#include "ImperialFirstFlightGuide.h"
#include "ImperialMatchManager.h"
#include "ImperialRadioSubsystem.h"
#include "ImperialPlayerController.h"
#include "InputCoreTypes.h"
#include "ImperialLocalization.h"
#include "SoccerDronePawn.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

namespace
{
FString GuideText(const TCHAR* Korean,const TCHAR* English)
{return ImperialLocalization::IsKorean()?Korean:English;}
}
AImperialFirstFlightGuide::AImperialFirstFlightGuide()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bStartWithTickEnabled=false;
    PrimaryActorTick.TickGroup=TG_PostPhysics;
}
AImperialMatchManager* AImperialFirstFlightGuide::GetMatch()
{
    if(!Match.IsValid())Match=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));
    return Match.Get();
}
void AImperialFirstFlightGuide::ClearMessage()
{MessageKorean.Empty();MessageEnglish.Empty();}
void AImperialFirstFlightGuide::SetMessage(const TCHAR* Korean,const TCHAR* English)
{MessageKorean=Korean;MessageEnglish=English;}
bool AImperialFirstFlightGuide::ObserveWorld(ImperialFirstFlight::Observation& Observation,bool* bAllGrounded)
{
    auto* Manager=GetMatch();auto* Drone=Manager?Manager->GetDrone(0):nullptr;
    if(!Manager||!Drone||!Drone->GetBodyMesh())return false;
    auto* Instance=GetGameInstance();const auto* Radio=Instance?Instance->GetSubsystem<UImperialRadioSubsystem>():nullptr;
    Observation.AIActive=Manager->IsAIPracticeActive();
    Observation.Practice=Manager->IsAutonomyAllowed()&&(Manager->GetModeName()==TEXT("MANUAL TRAINING")||Observation.AIActive);
    Observation.SelectedIndex=Drone->GetPlayerRole()==0?Manager->GetSelected():INDEX_NONE;
    const auto* Pilot=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    Observation.Keyboard=Radio&&Pilot&&!Radio->IsEnabled()&&!Radio->IsCalibrating()&&!Pilot->IsReadOnlyView()&&!Pilot->IsEquipmentInspection()&&
        (Drone->GetInputSource()==TEXT("KEYBOARD")||Drone->IsAutonomousEnabled());
    Observation.Throttle=Drone->GetThrottle();Observation.NeutralInput=Pilot!=nullptr;
    if(Pilot)for(const FKey Key:{EKeys::Up,EKeys::Down,EKeys::W,EKeys::A,EKeys::S,EKeys::D,EKeys::Q,EKeys::E,EKeys::SpaceBar})
        Observation.NeutralInput&=!Pilot->IsInputKeyDown(Key);
    Observation.Autonomous=Drone->IsAutonomousEnabled();Observation.Armed=Drone->IsArmed();
    Observation.AllDisarmed=Manager->GetDrones().Num()==10;
    bool AllGrounded=Manager->GetDrones().Num()==10;
    for(const auto& Aircraft:Manager->GetDrones())
    {
        if(!Aircraft){Observation.AllDisarmed=false;AllGrounded=false;continue;}
        Observation.AllDisarmed&=!Aircraft->IsArmed();Observation.Autonomous|=Aircraft->IsAutonomousEnabled();
        const double Altitude=Aircraft->GetAltitudeMeters(),Speed=Aircraft->GetFlightSpeed();
        AllGrounded&=FMath::IsFinite(Altitude)&&FMath::IsFinite(Speed)&&Altitude>=.05&&Altitude<=.35&&Speed<=.25;
    }
    const FVector Position=Drone->GetActorLocation()/100.;
    const FVector Velocity=Drone->GetBodyMesh()->GetPhysicsLinearVelocity()/100.;
    Observation.AltitudeM=Drone->GetAltitudeMeters();Observation.SpeedMps=Drone->GetFlightSpeed();
    Observation.HorizontalSpeedMps=Velocity.Size2D();Observation.VerticalSpeedMps=Velocity.Z;
    Observation.TiltDeg=Drone->GetTiltDegrees();Observation.X=Position.X;Observation.Y=Position.Y;Observation.TeamScore=Manager->GetScore(0);
    if(bAllGrounded)*bAllGrounded=AllGrounded;
    return true;
}
bool AImperialFirstFlightGuide::BeginGuide()
{
    if(Guide.IsActive())
    {
        SetMessage(TEXT("안내가 진행 중입니다. 현재 과제를 마친 뒤 다음 단계를 누르세요."),TEXT("A guide is already active. Finish this task, then choose NEXT STEP."));return false;
    }
    ClearMessage();ImperialFirstFlight::Observation Observation;bool AllGrounded=false;
    if(!ObserveWorld(Observation,&AllGrounded))
    {
        Guide.Stop(ImperialFirstFlight::Rejection::StateInvalid);SetMessage(TEXT("경기장을 준비하고 있습니다. 잠시 후 안내 시작을 다시 누르세요."),TEXT("The arena is not ready. Choose START GUIDE again in a moment."));SetActorTickEnabled(false);return false;
    }
    bool Started=Guide.Begin(Observation);
    if(Started&&!AllGrounded){Guide.Stop(ImperialFirstFlight::Rejection::LandFirst);Started=false;}
    SetActorTickEnabled(Started);return Started;
}
bool AImperialFirstFlightGuide::AdvanceStep()
{
    ClearMessage();ImperialFirstFlight::Observation Observation;
    if(!ObserveWorld(Observation)){Guide.Stop(ImperialFirstFlight::Rejection::StateInvalid);SetActorTickEnabled(false);return false;}
    const bool Advanced=Guide.Advance(Observation);SetActorTickEnabled(Guide.IsActive());return Advanced;
}
void AImperialFirstFlightGuide::RetryStep()
{
    ClearMessage();ImperialFirstFlight::Observation Observation;
    if(ObserveWorld(Observation))Guide.Retry(Observation);else Guide.Stop(ImperialFirstFlight::Rejection::StateInvalid);
    SetActorTickEnabled(Guide.IsActive());
}
void AImperialFirstFlightGuide::StopGuide()
{
    ClearMessage();
    if(Guide.GetStep()!=ImperialFirstFlight::Step::Complete)Guide.Stop();
    SetActorTickEnabled(false);
}
void AImperialFirstFlightGuide::ResetAircraftAndRestart()
{
    // This is the sole command issued by this actor, behind an explicit reset
    // button. Selection, radio settings, scores and the other nine bodies stay intact.
    ClearMessage();ImperialFirstFlight::Observation Observation;
    auto* Manager=GetMatch();auto* Drone=Manager?Manager->GetDrone(0):nullptr;
    if(!Manager||!Drone||!ObserveWorld(Observation)){Guide.Stop(ImperialFirstFlight::Rejection::StateInvalid);SetActorTickEnabled(false);return;}
    if(!Observation.Practice||Observation.AIActive||Observation.SelectedIndex!=0||!Observation.Keyboard||Observation.Autonomous)
    {
        Guide.Begin(Observation);SetActorTickEnabled(false);return;
    }
    for(int32 Index=1;Index<Manager->GetDrones().Num();++Index)
    {
        const auto* Other=Manager->GetDrone(Index);
        if(!Other||Other->IsArmed()||Other->GetAltitudeMeters()>.35||Other->GetFlightSpeed()>.25)
        {
            Guide.Stop(ImperialFirstFlight::Rejection::LandFirst);SetMessage(TEXT("다른 기체가 착륙하고 모터가 정지한 뒤 다시 시작하세요."),TEXT("Land and disarm the other aircraft before restarting this guide."));SetActorTickEnabled(false);return;
        }
    }
    Drone->ResetDrone();
    if(ObserveWorld(Observation))Guide.Begin(Observation);else Guide.Stop(ImperialFirstFlight::Rejection::StateInvalid);
    SetActorTickEnabled(Guide.IsActive());
}
void AImperialFirstFlightGuide::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);if(!Guide.IsActive()){SetActorTickEnabled(false);return;}
    ClearMessage();ImperialFirstFlight::Observation Observation;
    if(ObserveWorld(Observation))Guide.Observe(DeltaSeconds,Observation);else Guide.Stop(ImperialFirstFlight::Rejection::StateInvalid);
    if(!Guide.IsActive())SetActorTickEnabled(false);
}
FString AImperialFirstFlightGuide::GetTitle() const
{
    switch(Guide.GetStep())
    {
    case ImperialFirstFlight::Step::Takeoff:return GuideText(TEXT("첫 비행 1/6 · 이륙"),TEXT("FIRST FLIGHT 1/6 - TAKE OFF"));
    case ImperialFirstFlight::Step::Hover:return GuideText(TEXT("첫 비행 2/6 · 제자리 비행"),TEXT("FIRST FLIGHT 2/6 - HOVER"));
    case ImperialFirstFlight::Step::Move:return GuideText(TEXT("첫 비행 3/6 · 이동"),TEXT("FIRST FLIGHT 3/6 - MOVE"));
    case ImperialFirstFlight::Step::Land:return GuideText(TEXT("첫 비행 4/6 · 착륙"),TEXT("FIRST FLIGHT 4/6 - LAND"));
    case ImperialFirstFlight::Step::Goal:return GuideText(TEXT("첫 비행 5/6 · 첫 득점"),TEXT("FIRST FLIGHT 5/6 - SCORE"));
    case ImperialFirstFlight::Step::FinalLand:return GuideText(TEXT("첫 비행 6/6 · 마지막 착륙"),TEXT("FIRST FLIGHT 6/6 - FINAL LANDING"));
    case ImperialFirstFlight::Step::Complete:return GuideText(TEXT("첫 비행 완료"),TEXT("FIRST FLIGHT COMPLETE"));
    }
    return FString();
}
FString AImperialFirstFlightGuide::GetInstruction() const
{
    switch(Guide.GetStep())
    {
    case ImperialFirstFlight::Step::Takeoff:return GuideText(TEXT("Space로 모터를 켠 뒤 ↑를 짧게 눌러 0.7m 이상 이륙하세요."),TEXT("Press Space to arm, then tap UP to rise above 0.7 m."));
    case ImperialFirstFlight::Step::Hover:return GuideText(TEXT("높이 0.8~2.3m에서 천천히 움직이며 1.5초 동안 안정을 유지하세요."),TEXT("Stay between 0.8 and 2.3 m with slow movement for 1.5 seconds."));
    case ImperialFirstFlight::Step::Move:return GuideText(TEXT("W/S 또는 A/D를 짧게 눌러 현재 위치에서 수평으로 1m 이동하세요."),TEXT("Tap W/S or A/D to move 1 m horizontally from this step's starting point."));
    case ImperialFirstFlight::Step::Land:return GuideText(TEXT("↓로 천천히 내려온 뒤 Space로 모터를 끄고 0.5초 기다리세요."),TEXT("Lower throttle with DOWN, land, then press Space to disarm and wait 0.5 seconds."));
    case ImperialFirstFlight::Step::Goal:return GuideText(TEXT("↓로 스로틀을 최저로 내린 뒤 Space로 모터를 켜고 다시 이륙해 상대 골대를 통과하세요."),TEXT("Lower throttle fully, press Space to arm explicitly, then take off again and pass through the opponent's goal."));
    case ImperialFirstFlight::Step::FinalLand:return GuideText(TEXT("득점 후 천천히 내려와 착륙하고 Space로 모터를 끄세요."),TEXT("After scoring, descend gently, land, then press Space to stop the motors."));
    case ImperialFirstFlight::Step::Complete:return GuideText(TEXT("득점과 마지막 안전 착륙까지 마쳤습니다. 수동 연습을 계속할 수 있어요."),TEXT("You completed scoring and a final safe landing. Continue manual practice when ready."));
    }
    return FString();
}
FString AImperialFirstFlightGuide::GetSecondaryHint() const
{
    switch(Guide.GetStep())
    {
    case ImperialFirstFlight::Step::Takeoff:return GuideText(TEXT("↑/↓는 상승 힘 조절 · 키를 놓아도 스로틀은 유지됩니다. 모터가 거부되면 먼저 스로틀을 내리세요."),TEXT("UP/DOWN change throttle; releasing a key retains it. Lower throttle if arming is rejected."));
    case ImperialFirstFlight::Step::Hover:return GuideText(TEXT("너무 높아지면 ↓, 내려오면 ↑를 짧게 누르세요. ANGLE은 자세만 수평으로 돌려줍니다."),TEXT("Tap DOWN if rising and UP if falling. ANGLE levels attitude; it does not hold altitude."));
    case ImperialFirstFlight::Step::Move:return GuideText(TEXT("W/S 앞뒤 기울기 · A/D 좌우 기울기 · Q/E 기수 회전. 방향은 기체를 기준으로 합니다."),TEXT("W/S pitch; A/D roll; Q/E yaw. Directions follow the aircraft, not the camera."));
    case ImperialFirstFlight::Step::Land:return GuideText(TEXT("모터가 꺼져도 스로틀 값은 남을 수 있어요. ↓로 최저까지 내리고 모든 키를 놓은 뒤 다음을 누르세요."),TEXT("Throttle may remain after disarming. Lower it fully, release all flight keys, then choose NEXT."));
    case ImperialFirstFlight::Step::Goal:return GuideText(TEXT("골대 높이에 맞춰 앞에서 뒤로 완전히 통과하세요. 새 득점 후 마지막 착륙까지 진행합니다."),TEXT("Match goal height and pass fully from front to back. A new goal is followed by the final landing task."));
    case ImperialFirstFlight::Step::FinalLand:return GuideText(TEXT("바닥에 닿은 뒤 모터를 끄세요. 스로틀을 최저로 내리고 모든 키를 놓은 뒤 완료를 확인합니다."),TEXT("Touch down before disarming. Lower throttle fully and release every flight key before confirming completion."));
    case ImperialFirstFlight::Step::Complete:return GuideText(TEXT("안내는 조종을 대신하지 않습니다. 종료 전 착륙하고 Space로 모터를 끄세요."),TEXT("The guide never flies for you. Land and disarm before exiting."));
    }
    return FString();
}
FString AImperialFirstFlightGuide::GetStatus() const
{
    if(!MessageKorean.IsEmpty()||!MessageEnglish.IsEmpty())return ImperialLocalization::IsKorean()?MessageKorean:MessageEnglish;
    if(Guide.GetStep()==ImperialFirstFlight::Step::Complete)return GuideText(TEXT("여섯 과제를 모두 마치고 안전 착륙·모터 정지를 확인했습니다."),TEXT("All six tasks and the final landed, disarmed state are confirmed."));
    if(!Guide.IsActive()||Guide.GetRejection()!=ImperialFirstFlight::Rejection::None)switch(Guide.GetRejection())
    {
    case ImperialFirstFlight::Rejection::None:return GuideText(TEXT("키보드와 파란 공격수를 선택하고, 모든 기체를 착륙·모터 정지한 뒤 안내를 시작하세요."),TEXT("Choose keyboard and the blue striker, land and disarm all aircraft, then start the guide."));
    case ImperialFirstFlight::Rejection::TrainingOnly:return GuideText(TEXT("수동 연습에서만 안내할 수 있어요. 경기·자동 모드를 끝내고 다시 시작하세요."),TEXT("Use MANUAL TRAINING. Leave the match or autonomous mode before restarting the guide."));
    case ImperialFirstFlight::Rejection::AIActive:return GuideText(TEXT("AI 연습을 종료하고 모든 기체가 착륙한 뒤 안내를 시작하세요."),TEXT("Stop AI practice and wait for all aircraft to land before starting the guide."));
    case ImperialFirstFlight::Rejection::SelectBlueStriker:return GuideText(TEXT("기체 선택이 바뀌어 안내를 중단했습니다. 파란 공격수를 선택해 다시 시작하세요."),TEXT("The aircraft changed. Select the blue striker and start the guide again."));
    case ImperialFirstFlight::Rejection::KeyboardOnly:return GuideText(TEXT("키보드 조종에서 안내할 수 있어요. 조종기 설정을 마치고 키보드와 LOS 시점을 선택하세요."),TEXT("Use keyboard control. Finish controller setup and choose keyboard with the LOS view."));
    case ImperialFirstFlight::Rejection::Autonomy:return GuideText(TEXT("자동 조종이 켜져 안내를 중단했습니다. 직접 조종하는 수동 연습에서 다시 시작하세요."),TEXT("Autonomous control changed. Restart the guide in manual training with direct control."));
    case ImperialFirstFlight::Rejection::DisarmAll:return GuideText(TEXT("모든 기체의 모터를 끈 뒤 시작하세요. 안내가 모터를 자동으로 끄지는 않습니다."),TEXT("Disarm every aircraft first. The guide never stops motors automatically."));
    case ImperialFirstFlight::Rejection::LandFirst:return GuideText(TEXT("모든 기체가 바닥에 멈추고 모터가 꺼진 뒤 시작하세요."),TEXT("Wait until all aircraft are settled on the floor with motors off."));
    case ImperialFirstFlight::Rejection::LandingRequired:return GuideText(TEXT("다시 착륙하고 모터를 끈 뒤 다음을 누르세요. 안내는 현재 단계를 유지합니다."),TEXT("Land and disarm again before continuing. This task remains selected."));
    case ImperialFirstFlight::Rejection::ThrottleNotLow:return GuideText(TEXT("↓로 스로틀을 최저까지 내린 뒤 모든 키를 놓고 다시 눌러주세요."),TEXT("Lower throttle fully with DOWN, release every flight key, then try again."));
    case ImperialFirstFlight::Rejection::InputNotNeutral:return GuideText(TEXT("방향·스로틀·Space 키를 모두 놓아 중립을 확인한 뒤 다시 눌러주세요."),TEXT("Release the direction, throttle and Space keys before trying again."));
    case ImperialFirstFlight::Rejection::StateInvalid:return GuideText(TEXT("비행 상태가 바뀌어 안내를 중단했습니다. 안전하게 착륙한 뒤 다시 시작하세요."),TEXT("Flight state changed. Land safely, then start the guide again."));
    case ImperialFirstFlight::Rejection::UserStopped:return GuideText(TEXT("안내만 닫았습니다. 비행은 계속되며 마지막 착륙은 확인하지 않았습니다. 직접 착륙하고 모터를 끄세요."),TEXT("Only the guide is closed. Flight continues and the final landing is unconfirmed. Land and disarm manually."));
    }
    if(Guide.IsStepComplete())
    {
        if(Guide.GetStep()==ImperialFirstFlight::Step::FinalLand)return GuideText(TEXT("마지막 착륙 확인! 최저 스로틀·모든 키 중립에서 완료를 눌러주세요."),TEXT("Final landing observed. Confirm completion with low throttle and all flight keys released."));
        return GuideText(TEXT("과제 완료! 준비되면 다음 단계를 누르세요."),TEXT("Task complete. Choose NEXT STEP when ready."));
    }
    switch(Guide.GetStep())
    {
    case ImperialFirstFlight::Step::Takeoff:return GuideText(TEXT("0.7m 이상 높이를 확인하고 있습니다."),TEXT("Checking takeoff above 0.7 m."));
    case ImperialFirstFlight::Step::Hover:return GuideText(TEXT("높이와 움직임이 안정되면 진행 막대가 채워집니다."),TEXT("The progress bar fills while height and movement stay stable."));
    case ImperialFirstFlight::Step::Move:return GuideText(TEXT("이 단계의 시작 위치에서 수평 이동 거리를 확인합니다."),TEXT("Measuring horizontal distance from this step's starting point."));
    case ImperialFirstFlight::Step::Land:return GuideText(TEXT("착륙과 모터 정지를 모두 확인해야 완료됩니다."),TEXT("Both a settled landing and stopped motors are required."));
    case ImperialFirstFlight::Step::Goal:return GuideText(TEXT("새 득점을 확인하면 마지막 착륙 단계로 진행할 수 있어요."),TEXT("A newly accepted goal unlocks the final landing task."));
    case ImperialFirstFlight::Step::FinalLand:return GuideText(TEXT("마지막 착륙과 모터 정지를 확인해야 첫 비행 안내가 완료됩니다."),TEXT("The guide finishes only after the final landed and disarmed state is confirmed."));
    case ImperialFirstFlight::Step::Complete:break;
    }
    return FString();
}
