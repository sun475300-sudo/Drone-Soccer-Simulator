#include "ImperialAIComparisonVerifier.h"
#include "AIResultComparison.h"
#include "ImperialBuildInfo.h"
#include "ImperialMatchManager.h"
#include "ImperialRadioSubsystem.h"
#include "ImperialUserData.h"
#include "SoccerDronePawn.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool RecordMatches(const FImperialAIComparisonRecord& Record,const TSharedPtr<FJsonObject>& Expected)
{
    if(!Record.bValid||!Expected.IsValid())return false;
    const auto& R=Record.Result;
    const auto Conditions=Expected->GetObjectField(TEXT("conditions"));
    const auto Metrics=Expected->GetObjectField(TEXT("metrics"));
    bool Ok=Conditions.IsValid()&&Metrics.IsValid()&&
        R.SessionId.ToLower()==Expected->GetStringField(TEXT("session_id"))&&
        R.StopReason==Expected->GetStringField(TEXT("stop_reason"))&&R.CompletionReason==Expected->GetStringField(TEXT("completion_reason"));
    if(!Ok)return false;
    Ok&=Record.Schema==Conditions->GetIntegerField(TEXT("schema"))&&R.SourceHash.ToLower()==Conditions->GetStringField(TEXT("source_sha256"))&&
        R.BuildVersion==Conditions->GetStringField(TEXT("build_version"))&&R.Settings.Duel==(Conditions->GetStringField(TEXT("session_mode"))==TEXT("duel"))&&
        R.Settings.Count==Conditions->GetIntegerField(TEXT("opponents"))&&R.Settings.Difficulty==Conditions->GetIntegerField(TEXT("difficulty"))&&
        static_cast<double>(R.Settings.Seed)==Conditions->GetNumberField(TEXT("seed"))&&R.bTestSession==Conditions->GetBoolField(TEXT("test_session"));
    const auto Near=[](double A,double B){return FMath::Abs(A-B)<=1e-6;};
    Ok&=R.Goals==Metrics->GetIntegerField(TEXT("goals"))&&Near(R.PracticeSeconds,Metrics->GetNumberField(TEXT("practice_seconds")))&&
        Near(R.LandingSeconds,Metrics->GetNumberField(TEXT("landing_seconds")))&&Record.bHasRecovery==Metrics->HasField(TEXT("recovery_attempts"));
    if(Record.Schema==2)Ok&=R.OpponentGoals==Metrics->GetIntegerField(TEXT("opponent_goals"));
    if(Record.bHasRecovery)Ok&=R.RecoveryAttempts==Metrics->GetIntegerField(TEXT("recovery_attempts"));
    bool PlayerFound=false;
    for(const auto& A:R.Aircraft)if(A.Id==0){PlayerFound=true;Ok&=A.GuardImpacts==Metrics->GetIntegerField(TEXT("player_guard_impacts"))&&Near(A.GuardRemaining,Metrics->GetNumberField(TEXT("player_guard_remaining_percent")));}
    return Ok&&PlayerFound;
}
}
AImperialAIComparisonVerifier::AImperialAIComparisonVerifier()
{PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AImperialAIComparisonVerifier::Check(const FString& Name,bool Passed)
{
    auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("check"),Name);R->SetBoolField(TEXT("ok"),Passed);
    Checks.Add(MakeShared<FJsonValueObject>(R));bOk&=Passed;
}
void AImperialAIComparisonVerifier::BeginPlay()
{
    Super::BeginPlay();StartWall=FPlatformTime::Seconds();
    FString RequestedData,RequestedUser,Marker;
    FParse::Value(FCommandLine::Get(),TEXT("ImperialDataDir="),RequestedData);
    FParse::Value(FCommandLine::Get(),TEXT("UserDir="),RequestedUser);
    FParse::Value(FCommandLine::Get(),TEXT("ImperialAIComparisonManifest="),ManifestPath);
    FParse::Value(FCommandLine::Get(),TEXT("ImperialAIComparisonResult="),ResultFile);
    DataDir=ImperialUserData::Directory();CorpusDir=FPaths::GetPath(ManifestPath);
    const FString Parent=FPaths::GetPath(DataDir);
    const bool Paths=FParse::Param(FCommandLine::Get(),TEXT("ImperialAIComparisonSmoke"))&&
        !RequestedData.IsEmpty()&&!RequestedUser.IsEmpty()&&!ManifestPath.IsEmpty()&&!ResultFile.IsEmpty()&&
        !FPaths::IsRelative(RequestedData)&&!FPaths::IsRelative(RequestedUser)&&!FPaths::IsRelative(ManifestPath)&&!FPaths::IsRelative(ResultFile)&&
        FPaths::IsSamePath(RequestedData,DataDir)&&FPaths::GetCleanFilename(DataDir)==TEXT("user")&&
        FPaths::IsSamePath(RequestedUser,FPaths::Combine(Parent,TEXT("engine-user")))&&
        FPaths::IsSamePath(ResultFile,FPaths::Combine(Parent,TEXT("result.json")))&&
        FPaths::IsSamePath(ManifestPath,FPaths::Combine(Parent,TEXT("corpus/manifest.json")));
    const bool Fresh=Paths&&FFileHelper::LoadFileToString(Marker,*FPaths::Combine(DataDir,TEXT(".ai-comparison-qa")))&&
        Marker==TEXT("imperial-ai-comparison-qa-v1\n")&&!IFileManager::Get().FileExists(*ResultFile)&&
        !IFileManager::Get().FileExists(*FPaths::Combine(DataDir,TEXT("Config/RadioProfile.json")))&&
        !IFileManager::Get().DirectoryExists(*FPaths::Combine(DataDir,TEXT("TrainingResults")));
    bGuarded=Paths&&Fresh;Check(TEXT("explicit fresh absolute synthetic QA paths and marker required"),bGuarded);
    if(!bGuarded){Finish();return;}
    FString Text;TSharedPtr<FJsonObject> Manifest;
    const bool Loaded=IFileManager::Get().FileSize(*ManifestPath)<=1024*1024&&
        FFileHelper::LoadFileToString(Text,*ManifestPath)&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Manifest)&&Manifest.IsValid();
    bool Synthetic=false;const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
    const bool Valid=Loaded&&Manifest->TryGetBoolField(TEXT("records_are_synthetic"),Synthetic)&&Synthetic&&
        Manifest->GetIntegerField(TEXT("schema"))==1&&Manifest->TryGetArrayField(TEXT("cases"),Rows)&&Rows->Num()>0&&Rows->Num()<=200;
    Check(TEXT("bounded manifest explicitly identifies synthetic records"),Valid);
    if(!Valid){Finish();return;}Cases=*Rows;
    int32 FPS=60;FParse::Value(FCommandLine::Get(),TEXT("FPS="),FPS);
    Check(TEXT("explicit supported fixed timestep"),FPS==20||FPS==60||FPS==240);
    if(FPS!=20&&FPS!=60&&FPS!=240){Finish();return;}
    FApp::SetFixedDeltaTime(1./FPS);FApp::SetUseFixedTimeStep(true);if(GEngine)GEngine->SetMaxFPS(60);
}
bool AImperialAIComparisonVerifier::RunCase(const TSharedPtr<FJsonObject>& Case,TSharedPtr<FJsonObject>& Report)
{
    Report=MakeShared<FJsonObject>();if(!Case.IsValid())return false;
    FString Id,AName,BName;bool ExpectedA=false,ExpectedB=false;
    if(!Case->TryGetStringField(TEXT("id"),Id)||!Case->TryGetStringField(TEXT("reference"),AName)||!Case->TryGetStringField(TEXT("candidate"),BName)||
       !Case->TryGetBoolField(TEXT("expected_valid_reference"),ExpectedA)||!Case->TryGetBoolField(TEXT("expected_valid_candidate"),ExpectedB)||
       AName.IsEmpty()||BName.IsEmpty()||FPaths::GetCleanFilename(AName)!=AName||FPaths::GetCleanFilename(BName)!=BName)return false;
    Report->SetStringField(TEXT("id"),Id);
    FImperialAIComparisonRecord A,B;
    const bool ValidA=ImperialAIComparison::Load(FPaths::Combine(CorpusDir,AName),A);
    const bool ValidB=ImperialAIComparison::Load(FPaths::Combine(CorpusDir,BName),B);
    const auto Outcome=ImperialAIComparison::Compare(A,B);
    bool Ok=ValidA==ExpectedA&&ValidB==ExpectedB;
    Report->SetBoolField(TEXT("valid_reference"),ValidA);Report->SetBoolField(TEXT("valid_candidate"),ValidB);
    Report->SetStringField(TEXT("reference_error"),A.Error);Report->SetStringField(TEXT("candidate_error"),B.Error);
    if(!ExpectedA||!ExpectedB)
    {
        Ok&=Case->GetIntegerField(TEXT("expected_exit"))==1&&!Outcome.bEligible&&Outcome.Deltas.IsEmpty();
    }
    else
    {
        const auto E=Case->GetObjectField(TEXT("expected"));
        Ok&=E.IsValid()&&RecordMatches(A,E->GetObjectField(TEXT("reference")))&&RecordMatches(B,E->GetObjectField(TEXT("candidate")))&&
            Outcome.bEligible==E->GetBoolField(TEXT("eligible_for_deltas"))&&
            Outcome.bRecordedConditionsMatch==E->GetBoolField(TEXT("recorded_conditions_match"))&&
            Case->GetIntegerField(TEXT("expected_exit"))==(Outcome.bEligible?0:2);
        TArray<FString> ExpectedReasons,ActualReasons=Outcome.Reasons;
        for(const auto& Value:E->GetArrayField(TEXT("reasons")))ExpectedReasons.Add(Value->AsString());
        ExpectedReasons.Sort();ActualReasons.Sort();Ok&=ExpectedReasons==ActualReasons;
        const auto D=E->GetObjectField(TEXT("deltas"));Ok&=D.IsValid()&&D->Values.Num()==Outcome.Deltas.Num();
        TSet<FString> Seen;
        for(const auto& Delta:Outcome.Deltas)
        {
            double ExpectedValue=0;
            Ok&=!Seen.Contains(Delta.Key)&&D->TryGetNumberField(Delta.Key,ExpectedValue)&&FMath::IsFinite(Delta.Value)&&FMath::Abs(Delta.Value-ExpectedValue)<=1e-6;
            Seen.Add(Delta.Key);
        }
        if(!Outcome.bEligible)Ok&=Outcome.Deltas.IsEmpty()&&!Outcome.Reasons.IsEmpty();
    }
    Report->SetBoolField(TEXT("eligible"),Outcome.bEligible);Report->SetBoolField(TEXT("recorded_conditions_match"),Outcome.bRecordedConditionsMatch);
    TArray<TSharedPtr<FJsonValue>> Reasons;for(const auto& Reason:Outcome.Reasons)Reasons.Add(MakeShared<FJsonValueString>(Reason));Report->SetArrayField(TEXT("reasons"),Reasons);
    auto Deltas=MakeShared<FJsonObject>();for(const auto& D:Outcome.Deltas)Deltas->SetNumberField(D.Key,D.Value);Report->SetObjectField(TEXT("deltas"),Deltas);
    Report->SetBoolField(TEXT("ok"),Ok);return Ok;
}
void AImperialAIComparisonVerifier::Tick(float Dt)
{
    Super::Tick(Dt);if(bFinished||!bGuarded)return;
    MinDt=FMath::Min(MinDt,static_cast<double>(Dt));MaxDt=FMath::Max(MaxDt,static_cast<double>(Dt));
    WaitSimulationAge+=Dt;++WaitTicks;
    if(FPlatformTime::Seconds()-StartWall>10){Check(TEXT("bounded native fixture runtime"),false);Finish();return;}
    auto* M=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));
    if(!M||M->GetDrones().Num()!=10)
    {
        auto State=MakeShared<FJsonObject>();State->SetStringField(TEXT("phase"),TEXT("initial_state"));
        State->SetBoolField(TEXT("manager_exists"),M!=nullptr);State->SetNumberField(TEXT("aircraft_count"),M?M->GetDrones().Num():0);
        State->SetNumberField(TEXT("tick_dt_s"),Dt);State->SetNumberField(TEXT("wait_simulation_age_s"),WaitSimulationAge);
        SafetyStates.Add(MakeShared<FJsonValueObject>(State));Check(TEXT("settling wait requires ten initialized aircraft"),false);Finish();return;
    }
    const auto WaitInvariant=[&](){
        bool Ok=M->GetSelected()==0&&!M->IsAIPracticeActive()&&M->GetScore(0)==0&&M->GetScore(1)==0&&!M->IsRecordingFlight();
        for(const auto& D:M->GetDrones())Ok&=D&&!D->IsArmed()&&!D->IsAutonomousEnabled();
        const auto* Radio=GetGameInstance()->GetSubsystem<UImperialRadioSubsystem>();return Ok&&Radio&&!Radio->IsEnabled();
    };
    const auto Safe=[&](){
        bool Ok=M->GetSelected()==0&&!M->IsAIPracticeActive()&&M->GetScore(0)==0&&M->GetScore(1)==0&&!M->IsRecordingFlight();
        for(const auto& D:M->GetDrones())Ok&=D&&!D->IsArmed()&&!D->IsAutonomousEnabled()&&D->GetAltitudeMeters()<=.35f&&D->GetFlightSpeed()<=.25f;
        const auto* Radio=GetGameInstance()->GetSubsystem<UImperialRadioSubsystem>();return Ok&&Radio&&!Radio->IsEnabled();
    };
    const auto SnapshotSafety=[&](const TCHAR* Label)
    {
        auto State=MakeShared<FJsonObject>();State->SetStringField(TEXT("phase"),Label);
        State->SetNumberField(TEXT("tick_dt_s"),Dt);State->SetNumberField(TEXT("world_age_s"),GetWorld()->GetTimeSeconds());
        State->SetNumberField(TEXT("fixture_wall_age_s"),FPlatformTime::Seconds()-StartWall);
        State->SetNumberField(TEXT("wait_simulation_age_s"),WaitSimulationAge);State->SetNumberField(TEXT("wait_ticks"),WaitTicks);
        State->SetNumberField(TEXT("consecutive_safe_ticks_after_minimum_wait"),StableSafeTicks);State->SetBoolField(TEXT("wait_invariant"),WaitInvariant());
        State->SetNumberField(TEXT("selected"),M->GetSelected());State->SetNumberField(TEXT("blue_score"),M->GetScore(0));State->SetNumberField(TEXT("red_score"),M->GetScore(1));
        State->SetBoolField(TEXT("ai_active"),M->IsAIPracticeActive());State->SetBoolField(TEXT("flight_recording"),M->IsRecordingFlight());
        const auto* Radio=GetGameInstance()->GetSubsystem<UImperialRadioSubsystem>();
        State->SetBoolField(TEXT("radio_subsystem_exists"),Radio!=nullptr);State->SetBoolField(TEXT("radio_enabled"),Radio&&Radio->IsEnabled());
        State->SetNumberField(TEXT("aircraft_count"),M->GetDrones().Num());TArray<TSharedPtr<FJsonValue>> Aircraft;
        int32 Id=0;for(const auto& D:M->GetDrones())
        {
            auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("id"),Id++);Row->SetBoolField(TEXT("exists"),D!=nullptr);
            if(D)
            {
                const double Altitude=D->GetAltitudeMeters(),Speed=D->GetFlightSpeed();
                Row->SetNumberField(TEXT("altitude_m"),Altitude);Row->SetNumberField(TEXT("speed_mps"),Speed);
                Row->SetBoolField(TEXT("armed"),D->IsArmed());Row->SetBoolField(TEXT("autonomous"),D->IsAutonomousEnabled());
                Row->SetNumberField(TEXT("throttle"),D->GetThrottle());
                Row->SetBoolField(TEXT("existing_altitude_condition"),Altitude<=.35f);Row->SetBoolField(TEXT("existing_speed_condition"),Speed<=.25f);
                if(const auto* Body=D->GetBodyMesh())
                {
                    const FVector V=Body->GetPhysicsLinearVelocity()*.01;
                    Row->SetNumberField(TEXT("vx_mps"),V.X);Row->SetNumberField(TEXT("vy_mps"),V.Y);Row->SetNumberField(TEXT("vz_mps"),V.Z);
                    Row->SetNumberField(TEXT("body_z_m"),Body->GetComponentLocation().Z*.01);
                }
            }
            Aircraft.Add(MakeShared<FJsonValueObject>(Row));
        }
        State->SetArrayField(TEXT("aircraft"),Aircraft);State->SetBoolField(TEXT("existing_safe_condition"),Safe());
        SafetyStates.Add(MakeShared<FJsonValueObject>(State));
    };
    // Only the fixture waits for the existing spawn clearance to settle under gravity.
    // During the wait, any ARM/autonomy/input-context/score violation fails immediately.
    if(WaitTicks==1)SnapshotSafety(TEXT("initial_state"));
    if(!WaitInvariant())
    {
        SnapshotSafety(TEXT("wait_invariant_failed"));Check(TEXT("settling wait preserves disarmed manual no-input no-score invariants"),false);Finish();return;
    }
    if(WaitSimulationAge>2.)
    {
        SnapshotSafety(TEXT("settling_timeout"));Check(TEXT("existing safety thresholds settle within two simulation seconds"),false);Finish();return;
    }
    StableSafeTicks=WaitSimulationAge>=.5&&Safe()?StableSafeTicks+1:0;
    if(StableSafeTicks<2)return;
    ReadySimulationAge=WaitSimulationAge;SnapshotSafety(TEXT("before_comparison"));
    Check(TEXT("model verification starts with ten grounded disarmed manual aircraft and no score"),Safe());
    if(!Safe()){Finish();return;}
    for(const auto& Row:Cases)
    {
        TSharedPtr<FJsonObject> Report;const auto Object=Row.IsValid()&&Row->Type==EJson::Object?Row->AsObject():TSharedPtr<FJsonObject>();
        const bool Ok=RunCase(Object,Report);if(!Report.IsValid())Report=MakeShared<FJsonObject>();
        if(!Report->HasField(TEXT("ok")))Report->SetBoolField(TEXT("ok"),false);
        Reports.Add(MakeShared<FJsonValueObject>(Report));Check(Object.IsValid()?Object->GetStringField(TEXT("id")):TEXT("invalid manifest case"),Ok);
    }
    FString Sentinel;
    SnapshotSafety(TEXT("after_comparison"));
    Check(TEXT("comparison reads never arm, fly, score or enable radio"),Safe());
    Check(TEXT("unrelated sentinel and absent calibration/training paths preserved"),
        FFileHelper::LoadFileToString(Sentinel,*FPaths::Combine(DataDir,TEXT("keep.txt")))&&Sentinel==TEXT("synthetic comparison QA sentinel\n")&&
        !IFileManager::Get().FileExists(*FPaths::Combine(DataDir,TEXT("Config/RadioProfile.json")))&&
        !IFileManager::Get().DirectoryExists(*FPaths::Combine(DataDir,TEXT("TrainingResults"))));
    Finish();
}
void AImperialAIComparisonVerifier::Finish()
{
    if(bFinished)return;bFinished=true;SetActorTickEnabled(false);
    auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("ok"),bOk&&bGuarded&&Reports.Num()==Cases.Num()&&!Cases.IsEmpty());
    R->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());R->SetArrayField(TEXT("checks"),Checks);R->SetArrayField(TEXT("cases"),Reports);
    R->SetStringField(TEXT("data_directory"),DataDir);R->SetNumberField(TEXT("min_game_dt_s"),MinDt);R->SetNumberField(TEXT("max_game_dt_s"),MaxDt);
    R->SetArrayField(TEXT("safety_states"),SafetyStates);
    R->SetNumberField(TEXT("settle_min_simulation_s"),.5);R->SetNumberField(TEXT("settle_max_simulation_s"),2.);
    R->SetNumberField(TEXT("comparison_ready_simulation_s"),ReadySimulationAge);R->SetNumberField(TEXT("consecutive_safe_ticks"),StableSafeTicks);
    R->SetNumberField(TEXT("wait_ticks"),WaitTicks);
    R->SetStringField(TEXT("safety_diagnostic_scope"),TEXT("Read-only diagnostics around fixture-only settling: at least 0.5 simulation seconds then two consecutive ticks meeting the unchanged 0.35m/0.25m/s limits, at most 2 simulation seconds and 10 wall seconds. Manual/disarmed/no-input/no-score invariants checked every waiting tick. No threshold, force, pose, velocity, ARM or gameplay changes."));
    R->SetStringField(TEXT("scope"),TEXT("Fresh synthetic files read through product Load/Compare and compared with CLI expected guards, metadata, reasons and deltas. test_session=false fixtures are not human records. No OS input, controller, UI interaction, flight or online evidence."));
    FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));
    const bool Saved=bGuarded&&FFileHelper::SaveStringToFile(Text,*ResultFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FPlatformMisc::RequestExitWithStatus(false,bOk&&Saved&&Reports.Num()==Cases.Num()&&!Cases.IsEmpty()?0:1);
}
