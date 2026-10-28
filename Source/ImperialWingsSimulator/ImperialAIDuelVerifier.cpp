#include "ImperialAIDuelVerifier.h"
#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "ImperialHUD.h"
#include "ImperialBuildInfo.h"
#include "SoccerDronePawn.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"
namespace
{
TArray<TSharedPtr<FJsonValue>> PerformanceNumbers(const TArray<double>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    for(double Value:Values)Result.Add(MakeShared<FJsonValueNumber>(Value));
    return Result;
}
double PerformanceMedian(TArray<double> Values)
{
    if(Values.IsEmpty())return -1;
    Values.Sort();const int32 N=Values.Num();
    return N%2?Values[N/2]:(Values[N/2-1]+Values[N/2])*.5;
}
TArray<TSharedPtr<FJsonValue>> PerformanceVector(const FVector& Value)
{
    return {MakeShared<FJsonValueNumber>(Value.X),MakeShared<FJsonValueNumber>(Value.Y),MakeShared<FJsonValueNumber>(Value.Z)};
}
}
void AImperialAIDuelVerifier::ResetPerformance()
{
    ObservedSpeeds.Reset();GoalTimes.Reset();CycleTimes.Reset();
    for(double& Seconds:PhaseSeconds)Seconds=0;
    PerformanceClock=NextPerformanceRow=MaximumStrikerTilt=0;MinimumRosterSeparation=1e9;
    PerformanceLastScore=PerformanceLastCycles=OutOfBoundsFrames=0;
}
TSharedRef<FJsonObject> AImperialAIDuelVerifier::SpeedStatistics(int32 Phase,bool Horizontal,bool TravelOnly) const
{
    TArray<TPair<double,double>> Distribution;double Seconds=0,WeightedSpeed=0,Peak=0;
    for(const auto& Sample:ObservedSpeeds)
    {
        if((Phase>=0&&Sample.Phase!=Phase)||(TravelOnly&&Sample.Phase!=1&&Sample.Phase!=5))continue;
        const double Value=Horizontal?Sample.Horizontal:Sample.Speed;
        Distribution.Emplace(Value,Sample.Seconds);Seconds+=Sample.Seconds;WeightedSpeed+=Value*Sample.Seconds;Peak=FMath::Max(Peak,Value);
    }
    Distribution.Sort([](const auto& A,const auto& B){return A.Key<B.Key;});
    const auto Quantile=[&](double Fraction)
    {
        if(Distribution.IsEmpty())return 0.;
        double Accumulated=0;for(const auto& Item:Distribution){Accumulated+=Item.Value;if(Accumulated>=Seconds*Fraction)return Item.Key;}
        return Distribution.Last().Key;
    };
    auto Stats=MakeShared<FJsonObject>();Stats->SetNumberField(TEXT("count"),Distribution.Num());Stats->SetNumberField(TEXT("duration_s"),Seconds);
    Stats->SetNumberField(TEXT("mean_mps"),Seconds>0?WeightedSpeed/Seconds:0);Stats->SetNumberField(TEXT("peak_mps"),Peak);
    Stats->SetNumberField(TEXT("p50_mps"),Quantile(.5));Stats->SetNumberField(TEXT("p95_mps"),Quantile(.95));return Stats;
}
void AImperialAIDuelVerifier::RecordPerformance(AImperialMatchManager& M,ASoccerDronePawn& Striker,float Dt)
{
    // GetElapsed is the duel's remaining 180-second clock. It freezes while
    // landing, so active distributions exclude landing and post-finish probes.
    const double CurrentClock=FMath::Clamp(180.-M.GetElapsed(),0.,180.);
    const double ActiveDt=FMath::Max(0.,CurrentClock-PerformanceClock);
    PerformanceClock=CurrentClock;
    const int32 Phase=FMath::Clamp(M.GetAIAttackPhase(),0,5);
    const FVector Velocity=Striker.GetBodyMesh()->GetPhysicsLinearVelocity()/100.;
    const double Speed=Striker.GetFlightSpeed(),Horizontal=Velocity.Size2D();
    const int32 Score=M.GetScore(1),Cycles=M.GetAIAttackCycles();
    for(int32 I=PerformanceLastScore;I<Score;++I)GoalTimes.Add(CurrentClock);
    for(int32 I=PerformanceLastCycles;I<Cycles;++I)CycleTimes.Add(CurrentClock);
    PerformanceLastScore=Score;PerformanceLastCycles=Cycles;
    if(ActiveDt>0)
    {
        FObservedSpeed Sample;Sample.Speed=Speed;Sample.Horizontal=Horizontal;Sample.Seconds=ActiveDt;Sample.Phase=Phase;ObservedSpeeds.Add(Sample);
        PhaseSeconds[Phase]+=ActiveDt;MaximumStrikerTilt=FMath::Max(MaximumStrikerTilt,static_cast<double>(Striker.GetTiltDegrees()));
        const FVector Position=Striker.GetActorLocation()/100.;
        if(Position.ContainsNaN()||FMath::Abs(Position.X)>8.05||FMath::Abs(Position.Y)>3.55||Position.Z<.1||Position.Z>4.55)++OutOfBoundsFrames;
        const int32 Count=M.GetAISettings().Count;
        for(int32 I=0;I<Count+1;++I)for(int32 J=I+1;J<Count+1;++J)
        {
            const int32 A=I==0?0:I+4,B=J==0?0:J+4;
            if(auto* First=M.GetDrone(A))if(auto* Second=M.GetDrone(B))
                MinimumRosterSeparation=FMath::Min(MinimumRosterSeparation,FVector::Distance(First->GetActorLocation(),Second->GetActorLocation())/100.);
        }
    }
    // Sample current real body state at the first actual frame at/after each
    // 0.1 game-second boundary. No wall-time gating or interpolated velocities.
    if(Age+1e-6>=NextPerformanceRow)
    {
        auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("case"),Case);Row->SetNumberField(TEXT("time"),Age);Row->SetNumberField(TEXT("active_time"),CurrentClock);
        Row->SetNumberField(TEXT("phase"),Phase);Row->SetBoolField(TEXT("landing"),M.IsAILanding());Row->SetBoolField(TEXT("active"),ActiveDt>0);
        Row->SetArrayField(TEXT("position_m"),PerformanceVector(Striker.GetActorLocation()/100.));Row->SetArrayField(TEXT("velocity_mps"),PerformanceVector(Velocity));
        Row->SetNumberField(TEXT("speed_mps"),Speed);Row->SetArrayField(TEXT("target_m"),PerformanceVector(Striker.GetAutonomousTarget()/100.));
        Row->SetNumberField(TEXT("red"),Score);Row->SetNumberField(TEXT("cycles"),Cycles);Row->SetNumberField(TEXT("guard_impacts"),Striker.GetGuardImpactCount());
        Row->SetNumberField(TEXT("guard_remaining_pct"),Striker.GetGuardIntegrityPercent());Row->SetNumberField(TEXT("recovery_attempts"),M.GetAIRecoveryAttempts());
        PerformanceRows.Add(MakeShared<FJsonValueObject>(Row));
        do{NextPerformanceRow+=.1;}while(NextPerformanceRow<=Age+1e-6);
    }
    (void)Dt;
}
void AImperialAIDuelVerifier::FinishPerformance(AImperialMatchManager& M,ASoccerDronePawn& Striker)
{
    const auto& Result=M.GetAIResult();const auto Settings=M.GetAISettings();auto Metric=MakeShared<FJsonObject>();
    Metric->SetNumberField(TEXT("case"),Case);Metric->SetNumberField(TEXT("opponents"),Settings.Count);Metric->SetNumberField(TEXT("difficulty"),Settings.Difficulty);Metric->SetNumberField(TEXT("seed"),Settings.Seed);
    Metric->SetNumberField(TEXT("active_seconds"),PerformanceClock);Metric->SetNumberField(TEXT("first_goal_s"),GoalTimes.IsEmpty()?-1:GoalTimes[0]);
    Metric->SetArrayField(TEXT("goal_times_s"),PerformanceNumbers(GoalTimes));Metric->SetArrayField(TEXT("cycle_times_s"),PerformanceNumbers(CycleTimes));
    TArray<double> GoalIntervals,RetreatIntervals;
    for(int32 I=1;I<GoalTimes.Num();++I)GoalIntervals.Add(GoalTimes[I]-GoalTimes[I-1]);
    for(double Goal:GoalTimes)for(double Cycle:CycleTimes)if(Cycle>Goal){RetreatIntervals.Add(Cycle-Goal);break;}
    Metric->SetNumberField(TEXT("goal_interval_median_s"),PerformanceMedian(GoalIntervals));Metric->SetNumberField(TEXT("goal_to_retreat_median_s"),PerformanceMedian(RetreatIntervals));
    Metric->SetNumberField(TEXT("red_goals"),Result.OpponentGoals);Metric->SetNumberField(TEXT("attack_cycles"),M.GetAIAttackCycles());Metric->SetNumberField(TEXT("landing_seconds"),Result.LandingSeconds);
    Metric->SetNumberField(TEXT("recovery_attempts"),Result.RecoveryAttempts);Metric->SetStringField(TEXT("stop_reason"),Result.StopReason);Metric->SetStringField(TEXT("completion_reason"),Result.CompletionReason);
    int32 RosterImpacts=0,StrikerImpacts=0;double StrikerGuard=100;
    for(const auto& Aircraft:Result.Aircraft){RosterImpacts+=Aircraft.GuardImpacts;if(Aircraft.Id==5){StrikerImpacts=Aircraft.GuardImpacts;StrikerGuard=Aircraft.GuardRemaining;}}
    Metric->SetNumberField(TEXT("striker_guard_impacts_at_stop"),StrikerImpacts);Metric->SetNumberField(TEXT("striker_guard_remaining_pct_at_stop"),StrikerGuard);
    Metric->SetNumberField(TEXT("roster_guard_impacts_at_stop"),RosterImpacts);Metric->SetNumberField(TEXT("striker_guard_impacts_after_landing"),Striker.GetGuardImpactCount());
    Metric->SetNumberField(TEXT("minimum_roster_pair_separation_m"),MinimumRosterSeparation);Metric->SetNumberField(TEXT("maximum_striker_tilt_deg"),MaximumStrikerTilt);
    Metric->SetNumberField(TEXT("out_of_bounds_frame_count"),OutOfBoundsFrames);Metric->SetObjectField(TEXT("active_speed"),SpeedStatistics(-1,false,false));Metric->SetObjectField(TEXT("active_horizontal_speed"),SpeedStatistics(-1,true,false));
    Metric->SetObjectField(TEXT("travel_speed"),SpeedStatistics(-1,false,true));Metric->SetObjectField(TEXT("travel_horizontal_speed"),SpeedStatistics(-1,true,true));
    static const TCHAR* Names[]={TEXT("lift"),TEXT("approach"),TEXT("align"),TEXT("cross"),TEXT("clear"),TEXT("return")};TArray<TSharedPtr<FJsonValue>> Phases;
    for(int32 Phase=0;Phase<6;++Phase)
    {
        auto Entry=MakeShared<FJsonObject>();Entry->SetNumberField(TEXT("phase"),Phase);Entry->SetStringField(TEXT("name"),Names[Phase]);Entry->SetNumberField(TEXT("duration_s"),PhaseSeconds[Phase]);
        Entry->SetObjectField(TEXT("speed"),SpeedStatistics(Phase,false,false));Entry->SetObjectField(TEXT("horizontal_speed"),SpeedStatistics(Phase,true,false));Phases.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Metric->SetArrayField(TEXT("phase_metrics"),Phases);Metric->SetStringField(TEXT("scope"),TEXT("tick-observed real Chaos speed; time-weighted active distributions; travel=approach+return; raw states at 0.1 game-second cadence; guard contacts include floor/goal, not pair-specific collision classification; no USB input"));
    PerformanceMetrics.Add(MakeShared<FJsonValueObject>(Metric));
}
AImperialAIDuelVerifier::AImperialAIDuelVerifier(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;}
void AImperialAIDuelVerifier::Check(const TCHAR* Name,bool Passed,double Value)
{auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("case"),Case);R->SetStringField(TEXT("check"),Name);R->SetBoolField(TEXT("ok"),Passed);R->SetNumberField(TEXT("value"),Value);Checks.Add(MakeShared<FJsonValueObject>(R));bOk&=Passed;}
void AImperialAIDuelVerifier::Finish()
{
    auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("ok"),bOk);R->SetArrayField(TEXT("checks"),Checks);R->SetArrayField(TEXT("samples"),Samples);R->SetArrayField(TEXT("performance_rows"),PerformanceRows);R->SetArrayField(TEXT("performance_metrics"),PerformanceMetrics);R->SetNumberField(TEXT("performance_schema"),1);R->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());R->SetNumberField(TEXT("min_game_dt_s"),MinDt);R->SetNumberField(TEXT("max_game_dt_s"),MaxDt);
    FString File,Json;FParse::Value(FCommandLine::Get(),TEXT("ImperialAIDuelResult="),File);FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Json));const bool Saved=FFileHelper::SaveStringToFile(Json,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);SetActorTickEnabled(false);FPlatformMisc::RequestExitWithStatus(false,bOk&&Saved?0:1);
}
void AImperialAIDuelVerifier::Tick(float Dt)
{
    Super::Tick(Dt);Age+=Dt;Total+=Dt;MinDt=FMath::Min(MinDt,static_cast<double>(Dt));MaxDt=FMath::Max(MaxDt,static_cast<double>(Dt));
    if(Total>1500){Check(TEXT("bounded duel fixture"),false);Finish();return;}
    auto* M=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));auto* PC=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));auto* H=PC?Cast<AImperialHUD>(PC->GetHUD()):nullptr;if(M&&H&&Stage==1)if(auto* Observed=M->GetDrone(5))RecordPerformance(*M,*Observed,Dt);if(!M||!H||Age<.25)return;
    const bool Interrupted=FParse::Param(FCommandLine::Get(),TEXT("ImperialAIDuelInterrupted"));
    auto* Player=M->GetDrone(0);auto* Striker=M->GetDrone(5);const int32 Count=Case<3?3:4;
    if(Stage==0)
    {
        if(FParse::Param(FCommandLine::Get(),TEXT("ImperialAIDuelReload")))
        {
            const auto& R=M->GetAIResult();Check(TEXT("saved duel restores scores roster and winner"),R.Exists()&&R.bSaved&&R.Settings.Duel&&R.OpponentGoals>=2&&R.Goals==0&&R.Aircraft.Num()==R.Settings.Count+1&&FString(R.Outcome())==TEXT("RED WINS"));
            bool Safe=!M->IsAIPracticeActive();for(const auto& D:M->GetDrones())Safe&=!D->IsArmed();Check(TEXT("loading a duel record never launches aircraft"),Safe);Finish();return;
        }
        FImperialAIResult Result;Result.Settings.Duel=true;Result.StopReason=TEXT("time_limit");Result.Goals=2;Result.OpponentGoals=1;
        Check(TEXT("winner comparison uses both teams"),FString(Result.Outcome())==TEXT("BLUE WINS"));Result.OpponentGoals=2;Check(TEXT("equal score is a draw"),FString(Result.Outcome())==TEXT("DRAW"));Result.StopReason=TEXT("manual_stop");Check(TEXT("early stop cannot award a win"),FString(Result.Outcome())==TEXT("INTERRUPTED"));
        FParse::Value(FCommandLine::Get(),TEXT("ImperialAIDuelCapture="),Capture);FParse::Value(FCommandLine::Get(),TEXT("ImperialAIDuelCases="),CaseCount);CaseCount=FMath::Clamp(CaseCount,1,6);if(GEngine)GEngine->SetMaxFPS(Capture.IsEmpty()?0:60);
        M->ResetMatch();M->ChangeView(1);if(H->IsRadioPanelOpen())H->ToggleRadioPanel();if(!H->IsPilotMenuOpen())H->ToggleAIPracticePanel();
        H->NotifyHitBoxClick(TEXT("AI_Duel"));H->NotifyHitBoxClick(Count==3?TEXT("AI_Count3"):TEXT("AI_Count4"));H->NotifyHitBoxClick(Case%3==0?TEXT("AI_Easy"):(Case%3==1?TEXT("AI_Medium"):TEXT("AI_Hard")));H->NotifyHitBoxClick(TEXT("AI_Start"));
        Check(TEXT("native menu selects duel and launches only AI roster"),M->IsAIPracticeActive()&&M->GetAISettings().Duel&&M->GetAISettings().Count==Count&&M->GetAISettings().Difficulty==Case%3&&!Player->IsArmed());
        M->SelectDrone(5);Check(TEXT("AI striker possession rejected"),M->GetSelected()==0);M->StartMatch();Check(TEXT("official match cannot replace active AI duel"),M->IsAIPracticeActive()&&M->IsAutonomyAllowed());
        if(!M->IsAIPracticeActive()){Finish();return;}
        if(Case==0&&!M->IsRecordingFlight())M->ToggleFlightRecording();
        ResetPerformance();bPlayerSafe=bRosterSafe=bFinite=true;bSawRetreat=false;LastSample=-1;Stage=1;Age=0;return;
    }
    if(Stage==1)
    {
        bPlayerSafe&=!Player->IsArmed()&&!Player->IsAutonomousEnabled()&&M->GetSelected()==0;
        for(int32 I=0;I<10;++I){auto* D=M->GetDrone(I);bFinite&=!D->GetActorLocation().ContainsNaN()&&D->GetFlightSpeed()<12;if(I<5||I>=5+Count)bRosterSafe&=!D->IsArmed()&&!D->IsAutonomousEnabled();}
        bSawRetreat|=M->IsOffside(1);if(Age-LastSample>=1)
        {
            LastSample=Age;auto S=MakeShared<FJsonObject>();const FVector P=Striker->GetActorLocation()/100.,T=Striker->GetAutonomousTarget()/100.;S->SetNumberField(TEXT("case"),Case);S->SetNumberField(TEXT("time"),Age);S->SetNumberField(TEXT("phase"),M->GetAIAttackPhase());S->SetNumberField(TEXT("cycles"),M->GetAIAttackCycles());S->SetNumberField(TEXT("red"),M->GetScore(1));S->SetNumberField(TEXT("attempts"),M->GetAIRecoveryAttempts());S->SetNumberField(TEXT("tilt"),Striker->GetTiltDegrees());S->SetArrayField(TEXT("position"),{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)});S->SetArrayField(TEXT("target"),{MakeShared<FJsonValueNumber>(T.X),MakeShared<FJsonValueNumber>(T.Y),MakeShared<FJsonValueNumber>(T.Z)});Samples.Add(MakeShared<FJsonValueObject>(S));
        }
        if(Interrupted&&Age>=8&&M->IsAIPracticeActive()&&!M->IsAILanding())M->StopAIPractice();
        if(M->IsAIPracticeActive()&&Age<215)return;
        Check(TEXT("bounded session stop lands all AI"),!M->IsAIPracticeActive()&&(Interrupted?(Age>=8&&Age<35):(Age>=180&&Age<215)),Age);
        const auto& R=M->GetAIResult();if(!Interrupted)Check(TEXT("AI striker physically scores twice with own-half retreat"),M->GetScore(1)>=2&&M->GetAIAttackCycles()>=1&&bSawRetreat,M->GetScore(1));
        Check(TEXT("player ownership and inactive roster preserved"),bPlayerSafe&&bRosterSafe&&bFinite);
        bool Landed=true;for(int32 I=5;I<5+Count;++I)Landed&=!M->GetDrone(I)->IsArmed()&&M->GetDrone(I)->GetAltitudeMeters()<.32;
        Check(TEXT("all AI physically on floor with motors stopped"),Landed);
        Check(TEXT("duel result has real score winner time limit and landing"),R.bSaved&&R.bTestSession&&R.Settings.Duel&&R.Settings.Count==Count&&R.Goals==0&&R.OpponentGoals==M->GetScore(1)&&R.StopReason==(Interrupted?TEXT("manual_stop"):TEXT("time_limit"))&&R.CompletionReason==TEXT("landed")&&FString(R.Outcome())==(Interrupted?TEXT("INTERRUPTED"):TEXT("RED WINS")));
        FinishPerformance(*M,*Striker);
        if(M->IsRecordingFlight())M->ToggleFlightRecording();
        ScoreAtStop=M->GetScore(1);Stage=2;Age=0;
        // Explicit post-finish diagnostic flight: any later physical crossing must not score.
        Player->ResetDrone();Player->GetBodyMesh()->SetWorldLocationAndRotation(FVector(530,0,300),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);Player->SetAutonomousEnabled(true);Player->SetAutonomousTarget(FVector(730,0,300));return;
    }
    if(Stage==2&&Age>12)
    {
        Check(TEXT("score remains frozen during a real post-finish crossing"),M->IsAIDuelFinished()&&Player->GetActorLocation().X>678&&M->GetScore(0)==0&&M->GetScore(1)==ScoreAtStop);
        Player->SetAutonomousEnabled(false);M->ResetMatch();Check(TEXT("explicit reset reopens normal training and clears scores"),!M->IsAIDuelFinished()&&M->GetScore(0)==0&&M->GetScore(1)==0);
        if(!Capture.IsEmpty()){H->ToggleAIPracticePanel();H->NotifyHitBoxClick(TEXT("AI_Last"));FScreenshotRequest::RequestScreenshot(Capture,true,false);Stage=9;Age=0;return;}
        if(++Case>=CaseCount){Finish();return;}Stage=0;Age=0;return;
    }
    if(Stage==9&&Age>1)Finish();
}
