#include "ImperialFirstFlightVerifier.h"
#include "ImperialFirstFlightGuide.h"
#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "ImperialHUD.h"
#include "ImperialRadioSubsystem.h"
#include "ImperialLocalization.h"
#include "ImperialBuildInfo.h"
#include "SoccerDronePawn.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"

namespace
{
TArray<TSharedPtr<FJsonValue>> GuideVector(FVector Value)
{
    return {MakeShared<FJsonValueNumber>(Value.X),MakeShared<FJsonValueNumber>(Value.Y),MakeShared<FJsonValueNumber>(Value.Z)};
}
bool GuideContainsHangul(const FString& Text)
{
    for(const TCHAR Character:Text)if((Character>=0xAC00&&Character<=0xD7A3)||(Character>=0x1100&&Character<=0x11FF))return true;
    return false;
}
}
AImperialFirstFlightVerifier::AImperialFirstFlightVerifier()
{PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AImperialFirstFlightVerifier::BeginPlay()
{
    Super::BeginPlay();int32 FPS=60;FParse::Value(FCommandLine::Get(),TEXT("FPS="),FPS);
    if(FPS!=20&&FPS!=60&&FPS!=240)FPS=60;
    FApp::SetFixedDeltaTime(1./FPS);FApp::SetUseFixedTimeStep(true);
    FParse::Value(FCommandLine::Get(),TEXT("ImperialFirstFlightCapture="),Capture);
    if(GEngine)GEngine->SetMaxFPS(Capture.IsEmpty()?0:60);
}
void AImperialFirstFlightVerifier::Check(const TCHAR* Name,bool Passed,double Value)
{
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("check"),Name);Row->SetBoolField(TEXT("ok"),Passed);Row->SetNumberField(TEXT("value"),Value);
    Checks.Add(MakeShared<FJsonValueObject>(Row));bOk&=Passed;
}
void AImperialFirstFlightVerifier::Next(int32 NewStage)
{Stage=NewStage;Age=0;}
void AImperialFirstFlightVerifier::Finish()
{
    auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("ok"),bOk);Result->SetArrayField(TEXT("checks"),Checks);Result->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());
    Result->SetNumberField(TEXT("min_game_dt_s"),MinDt);Result->SetNumberField(TEXT("max_game_dt_s"),MaxDt);Result->SetNumberField(TEXT("maximum_takeoff_altitude_m"),MaximumTakeoffAltitude);
    Result->SetBoolField(TEXT("physical_takeoff_observed"),bTakeoffObserved);Result->SetBoolField(TEXT("full_goal_runtime_verified"),false);
    auto Probe=MakeShared<FJsonObject>();Probe->SetBoolField(TEXT("hover_stabilized"),bProbeHoverStable);Probe->SetNumberField(TEXT("launch_age_s"),ProbeLaunchAge);Probe->SetNumberField(TEXT("observed_score"),ProbeGoalScore);
    Probe->SetArrayField(TEXT("initial_placement_cm"),GuideVector(FVector(530,0,300)));Probe->SetArrayField(TEXT("hover_target_cm"),GuideVector(FVector(530,0,300)));Probe->SetArrayField(TEXT("crossing_target_cm"),GuideVector(FVector(730,0,300)));
    Probe->SetArrayField(TEXT("samples"),ScoreProbeSamples);Probe->SetStringField(TEXT("scope"),TEXT("one explicit initial pose; actual motor hover for at least 2.5 s and continuous stable position/speed/tilt/heading for 0.5 s; then target-only real goal crossing, never score injection or in-flight position correction"));Result->SetObjectField(TEXT("score_probe"),Probe);
    Result->SetStringField(TEXT("scope"),TEXT("isolated guide begin/step/retry/stop/reset and context gates; real motor-driven takeoff from explicit Unreal simulated keyboard bindings, not physical keyboard, OS events or USB; prior score seeded by a separate autonomous physical goal probe while guide inactive; hover/move/landing/goal/final-landing guide chain is covered by pure state tests, not claimed as a complete six-task runtime flight"));
    FString File,Text;FParse::Value(FCommandLine::Get(),TEXT("ImperialFirstFlightResult="),File);FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text));
    const bool Saved=!File.IsEmpty()&&FFileHelper::SaveStringToFile(Text,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    SetActorTickEnabled(false);FPlatformMisc::RequestExitWithStatus(false,bOk&&Saved?0:1);
}
void AImperialFirstFlightVerifier::Tick(float Dt)
{
    Super::Tick(Dt);Age+=Dt;Total+=Dt;MinDt=FMath::Min(MinDt,static_cast<double>(Dt));MaxDt=FMath::Max(MaxDt,static_cast<double>(Dt));
    if(Total>90){Check(TEXT("guide fixture completes within bounded game time"),false);Finish();return;}
    auto* M=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));
    auto* G=Cast<AImperialFirstFlightGuide>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialFirstFlightGuide::StaticClass()));
    auto* PC=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    auto* H=PC?Cast<AImperialHUD>(PC->GetHUD()):nullptr;auto* D=M?M->GetDrone(0):nullptr;
    auto* Radio=GetGameInstance()->GetSubsystem<UImperialRadioSubsystem>();
    if(!M||!G||!PC||!H||!D||!Radio)return;
    const auto Key=[&](FKey K,EInputEvent Event){PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Event,Event==IE_Released?0:1));};
    const auto Release=[&](){for(const FKey K:{EKeys::Up,EKeys::Down,EKeys::W,EKeys::A,EKeys::S,EKeys::D,EKeys::Q,EKeys::E,EKeys::SpaceBar})Key(K,IE_Released);bUpHeld=false;};
    const auto AllDisarmed=[&](){bool Safe=M->GetDrones().Num()==10;for(const auto& Body:M->GetDrones())Safe&=Body&&!Body->IsArmed();return Safe;};
    if(Stage==0&&Age>.8)
    {
        FString DataDir;const bool Isolated=FParse::Value(FCommandLine::Get(),TEXT("ImperialDataDir="),DataDir)&&!DataDir.IsEmpty()&&!FPaths::IsRelative(DataDir);
        Check(TEXT("fixture requires explicit absolute isolated user data"),Isolated);
        if(!Isolated){Finish();return;}
        Check(TEXT("new test user has no selected or enabled controller"),!Radio->GetSelectedDevice()&&!Radio->IsEnabled());
        G->StopGuide();M->ResetMatch();M->ChangeView(1);Release();PC->FlushPilotContextKeys();
        if(H->IsRadioPanelOpen())H->ToggleRadioPanel();if(H->IsAIPracticePanelOpen())H->ToggleAIPracticePanel();
        // Independent score fixture before the guide. One explicit initial pose,
        // then a measured motor hover before crossing; never inject the score or
        // reposition a flying body to make the goal succeed.
        D->GetBodyMesh()->SetWorldLocationAndRotation(FVector(530,0,300),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
        D->SetAutonomousEnabled(true);D->SetAutonomousTarget(FVector(530,0,300));Next(1);return;
    }
    if(Stage==1)
    {
        ProbeGoalScore=M->GetScore(0);
        const auto SampleProbe=[&]()
        {
            auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("probe_age_s"),Age);Row->SetStringField(TEXT("phase"),bProbeHoverStable?TEXT("crossing"):TEXT("hover"));
            Row->SetArrayField(TEXT("position_cm"),GuideVector(D->GetActorLocation()));Row->SetArrayField(TEXT("velocity_mps"),GuideVector(D->GetBodyMesh()->GetPhysicsLinearVelocity()*.01));
            Row->SetNumberField(TEXT("speed_mps"),D->GetFlightSpeed());Row->SetNumberField(TEXT("tilt_deg"),D->GetTiltDegrees());Row->SetNumberField(TEXT("heading_error_deg"),FMath::Abs(FMath::FindDeltaAngleDegrees(D->GetActorRotation().Yaw,0.0)));
            Row->SetNumberField(TEXT("total_thrust_n"),D->GetTotalThrustNewtons());Row->SetNumberField(TEXT("score"),ProbeGoalScore);ScoreProbeSamples.Add(MakeShared<FJsonValueObject>(Row));
        };
        if(Age+1e-8>=ProbeNextSampleAge){SampleProbe();ProbeNextSampleAge+=.1;}
        if(!bProbeHoverStable)
        {
            const bool Stable=FVector::Dist(D->GetActorLocation(),FVector(530,0,300))<=15&&D->GetFlightSpeed()<=.2f&&D->GetTiltDegrees()<3&&FMath::Abs(FMath::FindDeltaAngleDegrees(D->GetActorRotation().Yaw,0.0))<3;
            ProbeStableAge=Stable?ProbeStableAge+Dt:0;
            if(Age>=2.5&&ProbeStableAge>=.5)
            {
                Check(TEXT("independent score probe stabilizes under real motor forces before crossing"),true,Age);
                bProbeHoverStable=true;ProbeLaunchAge=Age;D->SetAutonomousTarget(FVector(730,0,300));SampleProbe();
            }
            else if(Age>8)
            {
                SampleProbe();Check(TEXT("independent score probe stabilizes under real motor forces before crossing"),false,D->GetFlightSpeed());Finish();
            }
            return;
        }
        if(ProbeGoalScore==1)
        {
            SampleProbe();Check(TEXT("independent physical goal seeds a nonzero score before guide"),true,ProbeGoalScore);
            D->SetAutonomousEnabled(false);D->ResetDrone();PC->FlushPilotContextKeys();Next(2);return;
        }
        if(Age-ProbeLaunchAge>12||ProbeGoalScore>1)
        {
            SampleProbe();Check(TEXT("independent physical goal seeds a nonzero score before guide"),false,ProbeGoalScore);Finish();
        }
        return;
    }
    if(Stage==2&&Age>.8)
    {
        const int32 Score=M->GetScore(0),Selected=M->GetSelected();const float Throttle=D->GetThrottle();const FVector Position=D->GetActorLocation();
        Check(TEXT("guide begins only from manual keyboard and grounded disarmed fleet"),G->BeginGuide());
        Check(TEXT("begin never arms, changes keyboard throttle, selection or score"),AllDisarmed()&&D->GetThrottle()==Throttle&&M->GetSelected()==Selected&&M->GetScore(0)==Score&&!Radio->IsEnabled()&&D->GetInputSource()==TEXT("KEYBOARD"));
        Check(TEXT("begin does not move the aircraft"),FVector::Dist(Position,D->GetActorLocation())<.001);
        Check(TEXT("next cannot bypass an incomplete takeoff"),!G->AdvanceStep()&&G->GetStepIndex()==0&&!G->IsStepComplete());
        G->RetryStep();Check(TEXT("retry retains current step and motor/input state"),G->GetStepIndex()==0&&G->GetProgress()==0&&AllDisarmed()&&M->GetScore(0)==Score);
        G->StopGuide();Check(TEXT("stop only stops observation"),!G->IsActive()&&AllDisarmed()&&D->GetThrottle()==Throttle&&M->GetScore(0)==Score);
        Check(TEXT("guide restarts without resetting the previous real score"),G->BeginGuide()&&M->GetScore(0)==Score);
        H->NotifyHitBoxClick(TEXT("FirstFlight_Open"));Key(EKeys::SpaceBar,IE_Pressed);Next(3);return;
    }
    if(Stage==3&&Age>.15)
    {
        Key(EKeys::SpaceBar,IE_Released);Check(TEXT("explicit simulated keyboard Space arms through native bindings"),D->IsArmed());
        Key(EKeys::Up,IE_Pressed);bUpHeld=true;Next(4);return;
    }
    if(Stage==4)
    {
        MaximumTakeoffAltitude=FMath::Max(MaximumTakeoffAltitude,static_cast<double>(D->GetAltitudeMeters()));
        bKeyboardStayed&=D->GetInputSource()==TEXT("KEYBOARD")&&!D->IsAutonomousEnabled();
        if(bUpHeld&&D->GetThrottle()>=.56f){Key(EKeys::Up,IE_Released);bUpHeld=false;}
        if(G->GetStepIndex()==0&&G->IsStepComplete())
        {
            bTakeoffObserved=true;Key(EKeys::Up,IE_Released);bUpHeld=false;
            Check(TEXT("real keyboard motor flight satisfies observed takeoff height and hold"),D->GetAltitudeMeters()>=.7f&&bKeyboardStayed&&D->IsArmed(),D->GetAltitudeMeters());
            Next(5);return;
        }
        if(Age>12){Check(TEXT("real keyboard takeoff completes within fixture limit"),false,D->GetAltitudeMeters());Release();Next(5);return;}
    }
    if(Stage==5&&Age>.25)
    {
        Check(TEXT("completed takeoff latches and never advances itself"),G->IsActive()&&G->GetStepIndex()==0&&G->IsStepComplete()&&G->GetProgress()==1);
        const bool Armed=D->IsArmed();const float Throttle=D->GetThrottle();
        Check(TEXT("explicit user next advances exactly one step without aircraft commands"),G->AdvanceStep()&&G->GetStepIndex()==1&&D->IsArmed()==Armed&&D->GetThrottle()==Throttle&&!D->IsAutonomousEnabled());
        G->RetryStep();Check(TEXT("hover retry only clears current observation progress"),G->GetStepIndex()==1&&G->GetProgress()==0&&D->IsArmed()==Armed);
        G->StopGuide();Check(TEXT("stopping an airborne guide does not disarm or change throttle"),!G->IsActive()&&D->IsArmed()==Armed&&D->GetThrottle()==Throttle);
        TArray<FVector> Positions;TArray<double> Batteries;for(int32 I=1;I<10;++I){Positions.Add(M->GetDrone(I)->GetActorLocation());Batteries.Add(M->GetDrone(I)->GetBatteryConsumedAh());}
        const int32 Score=M->GetScore(0),Selected=M->GetSelected();Release();G->ResetAircraftAndRestart();
        Check(TEXT("explicit reset restarts only the selected blue striker guide"),G->IsActive()&&G->GetStepIndex()==0&&!D->IsArmed()&&D->GetThrottle()==0);
        Check(TEXT("explicit guide reset preserves the nonzero real score and selection"),Score==1&&M->GetScore(0)==Score&&M->GetSelected()==Selected,M->GetScore(0));
        bool OthersUnchanged=true;for(int32 I=1;I<10;++I)OthersUnchanged&=FVector::Dist(Positions[I-1],M->GetDrone(I)->GetActorLocation())<.001&&Batteries[I-1]==M->GetDrone(I)->GetBatteryConsumedAh();
        Check(TEXT("explicit guide reset does not reset the other nine positions or battery clocks"),OthersUnchanged);
        M->SelectDrone(1);Next(6);return;
    }
    if(Stage==6&&Age>.15)
    {
        Check(TEXT("changing aircraft stops the guide without restoring its selection"),!G->IsActive()&&M->GetSelected()==1);
        M->SelectDrone(0);M->ResetMatch();PC->FlushPilotContextKeys();Next(7);return;
    }
    if(Stage==7&&Age>.8)
    {
        Check(TEXT("guide can start again after explicit test reset"),G->BeginGuide());
        ImperialAI::Settings Settings;Settings.Count=3;Settings.Difficulty=0;Settings.Seed=1;
        Check(TEXT("separate explicit AI context change is accepted"),M->StartAIPractice(Settings));Next(8);return;
    }
    if(Stage==8&&Age>.15)
    {
        Check(TEXT("AI mode stops the guide and guide does not undo that AI command"),!G->IsActive()&&M->IsAIPracticeActive());
        M->ResetMatch();PC->FlushPilotContextKeys();Next(9);return;
    }
    if(Stage==9&&Age>.8)
    {
        Check(TEXT("guide starts before official-mode gate check"),G->BeginGuide());M->StartMatch();Next(10);return;
    }
    if(Stage==10&&Age>.15)
    {
        Check(TEXT("official match stops guide and stays fully disarmed"),!G->IsActive()&&!M->IsAutonomyAllowed()&&AllDisarmed());
        M->ResetMatch();PC->FlushPilotContextKeys();Next(11);return;
    }
    if(Stage==11&&Age>.8)
    {
        Check(TEXT("guide starts before invalid-time observation check"),G->BeginGuide());const int32 Score=M->GetScore(0);G->Tick(0);
        Check(TEXT("invalid observation time stops guide without motor, score or input changes"),!G->IsActive()&&AllDisarmed()&&M->GetScore(0)==Score&&!Radio->IsEnabled());
        Check(TEXT("guide starts before read-only input-source change"),G->BeginGuide());M->ChangeView(4);Next(12);return;
    }
    if(Stage==12&&Age>.15)
    {
        Check(TEXT("read-only view input-source change stops guide without automatic ARM"),!G->IsActive()&&D->GetInputSource()==TEXT("VIEW ONLY")&&AllDisarmed());
        Check(TEXT("guide cannot start while spectator view blocks pilot input"),!G->BeginGuide()&&!G->IsActive()&&AllDisarmed());
        M->ChangeView(1);PC->ToggleEquipmentInspection();
        Check(TEXT("equipment-view fixture uses the real inspection state"),PC->IsEquipmentInspection());
        Check(TEXT("guide cannot start in equipment inspection even if input label says keyboard"),!G->BeginGuide()&&!G->IsActive()&&AllDisarmed());
        PC->ToggleEquipmentInspection();
        Check(TEXT("guide can be shown again in manual LOS"),G->BeginGuide());H->NotifyHitBoxClick(TEXT("FirstFlight_Open"));Next(13);return;
    }
    if(Stage==13&&Age>.5)
    {
        Check(TEXT("guide text getters remain usable after restart"),!G->GetTitle().IsEmpty()&&!G->GetStatus().IsEmpty()&&!G->GetInstruction().IsEmpty()&&!G->GetSecondaryHint().IsEmpty());
        Check(TEXT("guide exposes six tasks including the separate final landing"),G->GetStepCount()==6&&G->GetStepIndex()==0);
        const bool OriginalLanguage=ImperialLocalization::IsKorean();const int32 Score=M->GetScore(0),Selected=M->GetSelected();const bool Armed=D->IsArmed();const float Throttle=D->GetThrottle();
        Check(TEXT("guide getter language switches to Korean in isolated preference data"),ImperialLocalization::SetKorean(true));
        const FString KoreanTitle=G->GetTitle();
        Check(TEXT("all guide getters expose Korean instructions and hints"),GuideContainsHangul(KoreanTitle)&&GuideContainsHangul(G->GetInstruction())&&GuideContainsHangul(G->GetSecondaryHint())&&GuideContainsHangul(G->GetStatus()));
        Check(TEXT("guide getter language switches to English"),ImperialLocalization::SetKorean(false));
        Check(TEXT("guide English getters are distinct and contain no untranslated Hangul"),G->GetTitle()!=KoreanTitle&&!GuideContainsHangul(G->GetTitle())&&!GuideContainsHangul(G->GetInstruction())&&!GuideContainsHangul(G->GetSecondaryHint())&&!GuideContainsHangul(G->GetStatus()));
        Check(TEXT("language getter switches never change aircraft, score, throttle or stage"),M->GetScore(0)==Score&&M->GetSelected()==Selected&&D->IsArmed()==Armed&&D->GetThrottle()==Throttle&&G->GetStepIndex()==0&&G->IsActive());
        Check(TEXT("original guide display language is restored"),ImperialLocalization::SetKorean(OriginalLanguage));
        Check(TEXT("native fixture never creates a controller profile"),!IFileManager::Get().FileExists(*Radio->GetProfilePath())&&!Radio->IsEnabled()&&!Radio->GetSelectedDevice());
        if(!Capture.IsEmpty()){FScreenshotRequest::RequestScreenshot(Capture,true,false);bCaptureRequested=true;}
        Next(14);return;
    }
    if(Stage==14&&Age>.5)
    {
        if(bCaptureRequested)Check(TEXT("requested GPU guide capture was written"),IFileManager::Get().FileExists(*Capture));
        Release();G->StopGuide();Finish();
    }
}
