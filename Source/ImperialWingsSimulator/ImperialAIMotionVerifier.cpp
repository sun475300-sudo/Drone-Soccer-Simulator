#include "ImperialAIMotionVerifier.h"
#include "ImperialMatchManager.h"
#include "ImperialBuildInfo.h"
#include "SoccerDronePawn.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Engine/Engine.h"
#include <limits>

AImperialAIMotionVerifier::AImperialAIMotionVerifier()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.TickGroup=TG_PostPhysics;
}
void AImperialAIMotionVerifier::Check(const FString& Name,bool Passed,double Value)
{
    auto Row=MakeShared<FJsonObject>();
    Row->SetNumberField(TEXT("stage"),Stage);Row->SetStringField(TEXT("check"),Name);
    Row->SetBoolField(TEXT("ok"),Passed);Row->SetNumberField(TEXT("value"),FMath::IsFinite(Value)?Value:0.);
    Checks.Add(MakeShared<FJsonValueObject>(Row));bOk&=Passed;
}
bool AImperialAIMotionVerifier::IsDefault(const ASoccerDronePawn& Drone) const
{
    return Drone.GetAutonomousHorizontalSpeedLimitMps()==2.2f&&
        Drone.GetAutonomousHorizontalAccelerationLimitMps2()==3.2f;
}
bool AImperialAIMotionVerifier::AllDefault(const AImperialMatchManager& Manager,int32 Except) const
{
    if(Manager.GetDrones().Num()!=10)return false;
    for(int32 I=0;I<10;++I)
        if(I!=Except){const auto* Drone=Manager.GetDrone(I);if(!Drone||!IsDefault(*Drone))return false;}
    return true;
}
bool AImperialAIMotionVerifier::AllQuiet(const AImperialMatchManager& Manager) const
{
    if(Manager.GetDrones().Num()!=10)return false;
    for(int32 I=0;I<10;++I)
    {const auto* Drone=Manager.GetDrone(I);if(!Drone||Drone->IsArmed()||Drone->IsAutonomousEnabled())return false;}
    return true;
}
void AImperialAIMotionVerifier::VerifySetter(AImperialMatchManager& Manager)
{
    auto* Target=Manager.GetDrone(5);
    Check(TEXT("all ten bodies start with exact default limits"),AllDefault(Manager));
    Check(TEXT("diagnostic starts with all ten motors disarmed"),AllQuiet(Manager));
    const float Speeds[]={2.35f,2.55f,2.75f},Accelerations[]={3.35f,3.6f,3.9f};
    for(int32 Tier=0;Tier<3;++Tier)
    {
        const bool Accepted=Target->SetAutonomousHorizontalLimits(Speeds[Tier],Accelerations[Tier]);
        Check(FString::Printf(TEXT("valid tier %d changes only one explicit body and never arms"),Tier),
            Accepted&&Target->GetAutonomousHorizontalSpeedLimitMps()==Speeds[Tier]&&
            Target->GetAutonomousHorizontalAccelerationLimitMps2()==Accelerations[Tier]&&
            AllDefault(Manager,5)&&AllQuiet(Manager));
    }
    const float PreviousSpeed=Target->GetAutonomousHorizontalSpeedLimitMps();
    const float PreviousAcceleration=Target->GetAutonomousHorizontalAccelerationLimitMps2();
    const float NaN=std::numeric_limits<float>::quiet_NaN(),Infinity=std::numeric_limits<float>::infinity();
    struct FInvalid { const TCHAR* Name;float Speed,Acceleration; };
    const FInvalid Invalid[]={
        {TEXT("NaN speed"),NaN,3.6f},{TEXT("NaN acceleration"),2.55f,NaN},
        {TEXT("infinite speed"),Infinity,3.6f},{TEXT("infinite acceleration"),2.55f,Infinity},
        {TEXT("zero speed"),0,3.6f},{TEXT("zero acceleration"),2.55f,0},
        {TEXT("negative speed"),-.1f,3.6f},{TEXT("negative acceleration"),2.55f,-.1f},
        {TEXT("above speed bound"),3.01f,3.6f},{TEXT("above acceleration bound"),2.55f,4.01f},
        {TEXT("below positive speed bound"),.099f,3.6f},{TEXT("below positive acceleration bound"),2.55f,.099f}
    };
    for(const auto& Entry:Invalid)
    {
        const bool Rejected=!Target->SetAutonomousHorizontalLimits(Entry.Speed,Entry.Acceleration);
        Check(FString(TEXT("rejects "))+Entry.Name+TEXT(" without changing either prior limit"),
            Rejected&&Target->GetAutonomousHorizontalSpeedLimitMps()==PreviousSpeed&&
            Target->GetAutonomousHorizontalAccelerationLimitMps2()==PreviousAcceleration&&AllDefault(Manager,5)&&AllQuiet(Manager));
    }
    Check(TEXT("inclusive lower bounds accepted without arming"),
        Target->SetAutonomousHorizontalLimits(.1f,.1f)&&
        Target->GetAutonomousHorizontalSpeedLimitMps()==.1f&&Target->GetAutonomousHorizontalAccelerationLimitMps2()==.1f&&AllQuiet(Manager));
    Check(TEXT("inclusive upper bounds accepted without arming"),
        Target->SetAutonomousHorizontalLimits(3.f,4.f)&&
        Target->GetAutonomousHorizontalSpeedLimitMps()==3.f&&Target->GetAutonomousHorizontalAccelerationLimitMps2()==4.f&&AllQuiet(Manager));
    Target->SetAutonomousEnabled(false);
    Check(TEXT("idempotent autonomy false restores defaults before its early return"),AllDefault(Manager)&&AllQuiet(Manager));
    const bool Changed=Target->SetAutonomousHorizontalLimits(2.75f,3.9f);
    Target->ResetDrone(); // Explicit reset is the only diagnostic repositioning.
    Check(TEXT("explicit reset restores defaults and disarmed state"),Changed&&AllDefault(Manager)&&AllQuiet(Manager));
    Check(TEXT("setter checks leave selected manual player ownership intact"),Manager.GetSelected()==0);
}
void AImperialAIMotionVerifier::ObserveDuel(AImperialMatchManager& Manager)
{
    auto* Striker=Manager.GetDrone(5);auto* Player=Manager.GetDrone(0);
    const int32 Phase=Manager.GetAIAttackPhase();
    if(Phase<0||Phase>=6){bCapsCorrect=false;return;}
    SawPhase[Phase]=true;++PhaseFrames[Phase];
    bPlayerSafe&=!Player->IsArmed()&&!Player->IsAutonomousEnabled()&&Manager.GetSelected()==0;
    bOtherDefault&=AllDefault(Manager,5);
    for(int32 I=0;I<10;++I)
    {
        const auto* Drone=Manager.GetDrone(I);
        const FVector Position=Drone->GetActorLocation(),Velocity=Drone->GetBodyMesh()->GetPhysicsLinearVelocity();
        bFinite&=!Position.ContainsNaN()&&!Velocity.ContainsNaN()&&
            FMath::IsFinite(Drone->GetFlightSpeed())&&Drone->GetFlightSpeed()<12;
    }
    const bool Transit=Phase==1||Phase==4||Phase==5;
    const bool Recovering=Manager.GetAIRecoveringCount()>0;
    if(Recovering)++RecoverySkippedFrames; // Cannot infer which aircraft is escaping from the aggregate counter.
    else
    {
        const float ExpectedSpeed=Transit?2.75f:2.2f,ExpectedAcceleration=Transit?3.9f:3.2f;
        const bool Correct=Striker->GetAutonomousHorizontalSpeedLimitMps()==ExpectedSpeed&&
            Striker->GetAutonomousHorizontalAccelerationLimitMps2()==ExpectedAcceleration;
        bCapsCorrect&=Correct;
        if(Transit)++TransitFrames;
        else{++PrecisionFrames;bPrecisionDefault&=Correct;}
    }
    GoalsObserved=Manager.GetScore(1);CyclesObserved=Manager.GetAIAttackCycles();
    if(GoalsObserved>0&&FirstGoal<0)FirstGoal=Age;
    if(Age+1e-6<NextSample)return;
    auto Row=MakeShared<FJsonObject>();
    Row->SetNumberField(TEXT("time_s"),Age);Row->SetNumberField(TEXT("phase"),Phase);
    Row->SetNumberField(TEXT("striker_cap_mps"),Striker->GetAutonomousHorizontalSpeedLimitMps());
    Row->SetNumberField(TEXT("striker_acceleration_cap_mps2"),Striker->GetAutonomousHorizontalAccelerationLimitMps2());
    const double HorizontalSpeed=Striker->GetBodyMesh()->GetPhysicsLinearVelocity().Size2D()/100.;
    Row->SetNumberField(TEXT("actual_horizontal_speed_mps"),FMath::IsFinite(HorizontalSpeed)?HorizontalSpeed:-1.);
    Row->SetNumberField(TEXT("red_goals"),GoalsObserved);Row->SetNumberField(TEXT("cycles"),CyclesObserved);
    Row->SetBoolField(TEXT("other_nine_limits_default"),AllDefault(Manager,5));
    Row->SetNumberField(TEXT("recovering_aircraft"),Manager.GetAIRecoveringCount());
    Samples.Add(MakeShared<FJsonValueObject>(Row));
    do{NextSample+=.1;}while(NextSample<=Age+1e-6);
}
void AImperialAIMotionVerifier::Finish()
{
    if(bFinished)return;bFinished=true;
    auto Result=MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("schema"),1);Result->SetBoolField(TEXT("ok"),bOk);
    Result->SetArrayField(TEXT("checks"),Checks);Result->SetArrayField(TEXT("samples"),Samples);
    Result->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());
    Result->SetStringField(TEXT("data_directory"),DataDirectory);
    Result->SetNumberField(TEXT("min_game_dt_s"),MinDt);Result->SetNumberField(TEXT("max_game_dt_s"),MaxDt);
    Result->SetNumberField(TEXT("simulated_seconds"),Total);Result->SetNumberField(TEXT("first_goal_s"),FirstGoal);
    Result->SetNumberField(TEXT("goals_observed"),GoalsObserved);Result->SetNumberField(TEXT("cycles_observed"),CyclesObserved);
    Result->SetNumberField(TEXT("transit_frames"),TransitFrames);Result->SetNumberField(TEXT("precision_frames"),PrecisionFrames);
    Result->SetNumberField(TEXT("recovery_frames_excluded_from_phase_cap_check"),RecoverySkippedFrames);
    TArray<TSharedPtr<FJsonValue>> Phases;
    for(int32 I=0;I<6;++I)
    {
        auto Phase=MakeShared<FJsonObject>();Phase->SetNumberField(TEXT("phase"),I);
        Phase->SetBoolField(TEXT("seen"),SawPhase[I]);Phase->SetNumberField(TEXT("frames"),PhaseFrames[I]);
        Phases.Add(MakeShared<FJsonValueObject>(Phase));
    }
    Result->SetArrayField(TEXT("phases"),Phases);
    Result->SetStringField(TEXT("scope"),TEXT("Explicit isolated native fixture: limit API rejection/preservation; one actual hard AI duel goal and own-half return; stop/landing/reset and manual-match gates. No OS input, USB, external SDACS flight, human difficulty rating or non-reset teleport."));
    FString Json;const bool Serialized=FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Json));
    const bool Saved=Serialized&&!ResultPath.IsEmpty()&&
        FFileHelper::SaveStringToFile(Json,*ResultPath,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    SetActorTickEnabled(false);FPlatformMisc::RequestExitWithStatus(false,bOk&&Saved?0:1);
}
void AImperialAIMotionVerifier::Tick(float Dt)
{
    Super::Tick(Dt);if(bFinished)return;
    if(!bReady)
    {
        const bool Explicit=FParse::Param(FCommandLine::Get(),TEXT("ImperialAIMotionSmoke"))&&
            FParse::Value(FCommandLine::Get(),TEXT("ImperialDataDir="),DataDirectory)&&!DataDirectory.IsEmpty()&&!FPaths::IsRelative(DataDirectory)&&
            FParse::Value(FCommandLine::Get(),TEXT("ImperialAIMotionResult="),ResultPath)&&!ResultPath.IsEmpty()&&!FPaths::IsRelative(ResultPath);
        if(!Explicit)
        {
            UE_LOG(LogTemp,Error,TEXT("AIMotion fixture requires explicit absolute ImperialDataDir and ImperialAIMotionResult; no fixture actions performed."));
            bFinished=true;SetActorTickEnabled(false);FPlatformMisc::RequestExitWithStatus(false,2);return;
        }
        DataDirectory=FPaths::ConvertRelativePathToFull(DataDirectory);
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(ResultPath),true);
        if(GEngine)GEngine->SetMaxFPS(0);bReady=true;
    }
    if(!FMath::IsFinite(Dt)||Dt<=0){Check(TEXT("positive finite game timestep"),false);Finish();return;}
    Age+=Dt;Total+=Dt;MinDt=FMath::Min(MinDt,static_cast<double>(Dt));MaxDt=FMath::Max(MaxDt,static_cast<double>(Dt));
    if(Total>120){Check(TEXT("bounded motion fixture runtime"),false,Total);Finish();return;}
    auto* Manager=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));
    if(!Manager||Age<.25)return;
    if(Manager->GetDrones().Num()!=10){Check(TEXT("ten actual drone bodies available"),false);Finish();return;}
    auto* Player=Manager->GetDrone(0);auto* Striker=Manager->GetDrone(5);
    if(Stage==0)
    {
        AddTickPrerequisiteActor(Manager);
        Manager->ResetMatch();Manager->ChangeView(1);
        VerifySetter(*Manager);Manager->ResetMatch();
        const bool Started=Manager->StartAIPractice({3,2,1,true});
        Check(TEXT("hard duel starts only its explicit AI roster"),Started&&Manager->GetAISettings().Duel&&
            Manager->GetAISettings().Difficulty==2&&Manager->GetSelected()==0&&!Player->IsArmed()&&!Player->IsAutonomousEnabled());
        if(!Started){Finish();return;}
        Stage=1;Age=0;return;
    }
    if(Stage==1)
    {
        ObserveDuel(*Manager);
        if(Manager->IsAIPracticeActive()&&!Manager->IsAILanding()&&Age<60&&
            !(GoalsObserved>=1&&CyclesObserved>=1))return;
        Check(TEXT("actual hard duel observes Lift and higher-cap Approach"),SawPhase[0]&&SawPhase[1]&&TransitFrames>0);
        Check(TEXT("actual Align and Cross retain default precision limits"),SawPhase[2]&&SawPhase[3]&&bPrecisionDefault&&PrecisionFrames>0);
        Check(TEXT("actual Clear and Return receive transit limits"),SawPhase[4]&&SawPhase[5]);
        Check(TEXT("all observed non-recovery striker caps match the current phase"),bCapsCorrect);
        Check(TEXT("all other nine bodies retain default limits throughout duel"),bOtherDefault);
        Check(TEXT("manual player is never automatically armed or possessed"),bPlayerSafe);
        Check(TEXT("all actual body states stay finite and bounded in speed"),bFinite);
        Check(TEXT("actual physical goal observed within sixty game seconds"),GoalsObserved>=1&&FirstGoal<=60,FirstGoal);
        Check(TEXT("actual own-half return cycle completed within sixty game seconds"),CyclesObserved>=1&&Age<=60,Age);
        const bool Active=Manager->IsAIPracticeActive()&&!Manager->IsAILanding();
        TArray<FVector> Before;for(int32 I=0;I<10;++I)Before.Add(Manager->GetDrone(I)->GetActorLocation());
        Manager->StopAIPractice();bool SamePositions=true;
        for(int32 I=0;I<10;++I)SamePositions&=Before[I]==Manager->GetDrone(I)->GetActorLocation();
        Check(TEXT("STOP restores every limit immediately and begins physical landing"),Active&&Manager->IsAILanding()&&AllDefault(*Manager));
        Check(TEXT("STOP does not reposition bodies or arm the manual player"),SamePositions&&!Player->IsArmed()&&!Player->IsAutonomousEnabled());
        Stage=2;Age=0;return;
    }
    if(Stage==2)
    {
        bLandingDefault&=AllDefault(*Manager);
        bPlayerSafe&=!Player->IsArmed()&&!Player->IsAutonomousEnabled();
        if(Manager->IsAIPracticeActive()&&Age<35)return;
        bool Landed=!Manager->IsAIPracticeActive();
        for(int32 I=5;I<8;++I)
            Landed&=!Manager->GetDrone(I)->IsArmed()&&!Manager->GetDrone(I)->IsAutonomousEnabled()&&Manager->GetDrone(I)->GetAltitudeMeters()<.32;
        Check(TEXT("all three AI physically land and disarm with default limits"),Landed&&bLandingDefault,Age);
        Check(TEXT("manual player stays disarmed during AI landing"),bPlayerSafe);
        Manager->ResetMatch();
        Check(TEXT("session reset restores all ten quiet default bodies"),AllDefault(*Manager)&&AllQuiet(*Manager));
        const bool Practice=Manager->StartAIPractice({3,1,7,false});
        Check(TEXT("defensive practice launches only AI with unchanged default limits"),Practice&&AllDefault(*Manager)&&
            !Player->IsArmed()&&!Player->IsAutonomousEnabled());
        Manager->ResetMatch();
        Manager->StartMatch();
        const bool AIRejected=!Manager->StartAIPractice({3,2,1,true});
        Striker->SetAutonomousEnabled(true);Manager->HoldFleet();Manager->ReturnFleet();
        Check(TEXT("official manual match rejects AI and autonomous commands without arming"),
            AIRejected&&!Manager->IsAutonomyAllowed()&&!Manager->IsAIPracticeActive()&&AllQuiet(*Manager)&&AllDefault(*Manager));
        Manager->ResetMatch();
        Check(TEXT("final explicit reset leaves all ten motors off and limits default"),AllQuiet(*Manager)&&AllDefault(*Manager));
        Finish();return;
    }
}
