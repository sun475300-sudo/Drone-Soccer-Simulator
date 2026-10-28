#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "SoccerDronePawn.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ImperialBuildInfo.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PrototypeArenaBuilder.h"
#include "ImperialSuspendedGoal.h"

bool AImperialMatchManager::StartAIPractice(ImperialAI::Settings Settings)
{
    if(!Settings.Valid()||!IsAutonomyAllowed()||IsAIPracticeActive())return false;
    if(LastAIResult.Exists()&&!LastAIResult.bSaved){Log(TEXT("Unsaved AI result: retry saving or explicitly discard it in LAST RESULT before starting again."));return false;}
    const auto* Pilot=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    if(Pilot&&Pilot->IsReadOnlyView()){Log(TEXT("Select a pilot view before starting AI practice."));return false;}
    for(const auto& D:Drones)if(D->IsArmed()||D->GetActorLocation().Z>35||D->GetFlightSpeed()>.35)
    {Log(TEXT("Land and disarm every aircraft before starting AI practice."));return false;}
    // Explicit session start/reset: normal practice pads, no in-flight repositioning.
    ResetMatch();SelectDrone(0);AISettings=Settings;AIPlanner.Reset(Settings);AIAttacker.Reset(Settings.Difficulty);
    for(auto& Recovery:AIRecovery)Recovery.Reset();
    AIAge=AILandingAge=0;bAILandingFault=false;Mode=EImperialMode::AIPractice;
    CurrentAIResult={};CurrentAIResult.SessionId=FGuid::NewGuid().ToString(EGuidFormats::Digits);CurrentAIResult.Settings=Settings;
    CurrentAIResult.StartedUtc=FDateTime::UtcNow().ToIso8601();CurrentAIResult.SourceHash=ImperialBuildInfo::SourceHash();CurrentAIResult.BuildVersion=ImperialBuildInfo::Version();
    CurrentAIResult.bTestSession=FParse::Param(FCommandLine::Get(),TEXT("ImperialAIMotionSmoke"))||FParse::Param(FCommandLine::Get(),TEXT("ImperialFirstFlightSmoke"))||FParse::Param(FCommandLine::Get(),TEXT("ImperialAIPracticeSmoke"))||FParse::Param(FCommandLine::Get(),TEXT("ImperialAIResultsSmoke"))||FParse::Param(FCommandLine::Get(),TEXT("ImperialAIRecoverySmoke"))||FParse::Param(FCommandLine::Get(),TEXT("ImperialAIDuelSmoke"));
    for(int32 I=0;I<Drones.Num();++I)Rules.SetActive(I,I==0||(I>=Settings.FirstId()&&I<Settings.FirstId()+Settings.Count));
    for(int32 I=0;I<Settings.Count;++I)
    {
        auto* D=GetDrone(AISettings.FirstId()+I);D->SetEquipmentInspection(false);D->SetAutonomousEnabled(true);
        const auto P=Settings.Duel&&I==0?ImperialAI::Point{6.5,0,1.2}:AIPlanner.GetTargets()[I-(Settings.Duel?1:0)];D->SetAutonomousTarget(FVector(P.X,P.Y,P.Z)*100.);
    }
    Log(Settings.Duel?TEXT("AI duel started: red striker + defenders launching. Blue striker stays manual. 3 minutes; RESET starts over."):TEXT("AI defenders launching. Blue striker stays DISARMED: close menu, then arm manually at low throttle."));return true;
}
void AImperialMatchManager::StopAIPractice()
{
    if(Mode!=EImperialMode::AIPractice)return;
    SnapshotAIResult(TEXT("manual_stop"));
    Mode=EImperialMode::AILanding;AILandingAge=0;AILandingTargets.Reset();
    for(int32 I=0;I<AISettings.Count;++I)
    {
        auto* D=GetDrone(AISettings.FirstId()+I);
        D->ResetAutonomousHorizontalLimits();
        AILandingTargets.Add(D->GetActorLocation());
    }
    Log(TEXT("AI opponents landing vertically. Land your blue striker manually; RESET is an immediate session reset."));
}
void AImperialMatchManager::TickAIPractice(float Dt)
{
    if(!IsAIPracticeActive())return;
    if(!IsAutonomyAllowed()){FinishAIResult(TEXT("mode_changed"));for(int32 I=0;I<AISettings.Count;++I)GetDrone(AISettings.FirstId()+I)->SetAutonomousEnabled(false);Mode=EImperialMode::Manual;return;}
    AIAge+=Dt;
    if(Mode==EImperialMode::AIPractice)
    {
        const FVector P=GetDrone(0)->GetActorLocation()/100.,V=GetDrone(0)->GetBodyMesh()->GetPhysicsLinearVelocity()/100.;
        // Bounded sub-intervals advance only the tactical target; the aircraft remains
        // controlled by the existing force/torque flight loop, not this planner clock.
        float Remaining=FMath::Clamp(Dt,0.f,.25f);
        while(Remaining>0){const float Step=FMath::Min(Remaining,.05f);AIPlanner.Step(Step,{P.X,P.Y,P.Z},{V.X,V.Y,V.Z});Remaining-=Step;}
        for(int32 I=0;I<AISettings.Count;++I)
        {
            auto* D=GetDrone(AISettings.FirstId()+I);const FVector Position=D->GetActorLocation()/100.;auto T=AIPlanner.GetTargets()[AISettings.Duel?FMath::Max(0,I-1):I];
            if(AISettings.Duel&&I==0)
            {
                FVector Goal(-650,0,300);if(auto* Arena=Cast<APrototypeArenaBuilder>(UGameplayStatics::GetActorOfClass(GetWorld(),APrototypeArenaBuilder::StaticClass())))if(const auto* Ring=Arena->GetGoal(0))Goal=Ring->GetCentre();
                Goal/=100.;T=AIAttacker.Step(FMath::Clamp(Dt,0.f,.25f),{Position.X,Position.Y,Position.Z},D->GetFlightSpeed(),D->GetBodyMesh()->GetUpVector().Z,{Goal.X,Goal.Y,Goal.Z},Rules.IsOffside(1),AIRecovery[I].IsEscaping());
            }
            const int Before=AIRecovery[I].GetAttempts();
            const auto Action=AIRecovery[I].Step(FMath::Clamp(Dt,0.f,.25f),{Position.X,Position.Y,Position.Z},T,D->GetBodyMesh()->GetUpVector().Z,D->IsArmed(),D->IsAutonomousEnabled(),I,AISettings.Duel&&I==0);
            if(Action==ImperialAI::RecoveryAction::Grounded||Action==ImperialAI::RecoveryAction::Unavailable||Action==ImperialAI::RecoveryAction::Failed)
            {
                const TCHAR* Reason=Action==ImperialAI::RecoveryAction::Grounded?TEXT("defender_grounded"):(Action==ImperialAI::RecoveryAction::Unavailable?TEXT("defender_unavailable"):TEXT("recovery_failed"));
                SnapshotAIResult(Reason);
                // Stop the inverted motor only at floor contact. No automatic re-arm,
                // teleport, or in-air force boost is used to recover a disabled body.
                if(Action==ImperialAI::RecoveryAction::Grounded)D->SetAutonomousEnabled(false);
                StopAIPractice();Log(FString::Printf(TEXT("AI aircraft %d cannot continue. Landing the team; see LAST RESULT for the reason."),I+1));break;
            }
            // Only the duel striker's transit phases use the tier ceilings. Precision
            // approach alignment/crossing, recovery escapes and every defender retain defaults.
            if(AISettings.Duel&&I==0&&Action!=ImperialAI::RecoveryAction::Escape)
                D->SetAutonomousHorizontalLimits(
                    static_cast<float>(AIAttacker.GetHorizontalSpeedLimitMps()),
                    static_cast<float>(AIAttacker.GetHorizontalAccelerationLimitMps2()));
            else D->ResetAutonomousHorizontalLimits();
            if(Action==ImperialAI::RecoveryAction::Escape)T=AIRecovery[I].GetTarget();
            if(AIRecovery[I].GetAttempts()>Before)Log(FString::Printf(TEXT("AI aircraft %d is blocked: trying a short escape route."),I+1));
            D->SetAutonomousTarget(FVector(T.X,T.Y,T.Z)*100.);
        }
        // A finite practice session avoids unattended battery depletion.
        if(AIAge>=180){SnapshotAIResult(TEXT("time_limit"));StopAIPractice();}
    }
    else
    {
        AILandingAge+=Dt;bool Landed=true;
        for(int32 I=0;I<AISettings.Count;++I)
        {
            auto* D=GetDrone(AISettings.FirstId()+I);
            if(D->IsAutonomousEnabled())
            {
                AILandingTargets[I].Z=FMath::Max(12.,AILandingTargets[I].Z-40.*FMath::Clamp(Dt,0.f,.25f));
                D->SetAutonomousTarget(AILandingTargets[I]);
            }
            if(D->GetActorLocation().Z<32&&D->GetFlightSpeed()<.35){D->SetAutonomousEnabled(false);D->SetArmed(false);}
            else Landed=false;
        }
        if(Landed){Mode=EImperialMode::Manual;for(int32 I=0;I<Drones.Num();++I)Rules.SetActive(I,true);FinishAIResult(TEXT("landed"));}
        else if(AILandingAge>30&&!bAILandingFault){bAILandingFault=true;Log(TEXT("AI landing delayed by contact. Inspect the arena; RESET explicitly resets all aircraft."));}
    }
}
FString AImperialMatchManager::GetAIStatus() const
{
    const TCHAR* Level=AISettings.Difficulty==0?TEXT("BEGINNER"):(AISettings.Difficulty==1?TEXT("INTERMEDIATE"):TEXT("ADVANCED"));
    const TCHAR* Pattern=AIPlanner.GetPattern()==0?TEXT("STAGGERED"):(AIPlanner.GetPattern()==1?TEXT("PATROL"):TEXT("TRACKING"));
    return FString::Printf(TEXT("%s / %d AI / SEED %u / %s / RECOVERING %d / %.0f s"),Level,AISettings.Count,AISettings.Seed,IsAILanding()?TEXT("LANDING"):Pattern,GetAIRecoveringCount(),AIAge);
}
int32 AImperialMatchManager::GetAIRecoveryAttempts() const
{int32 Count=0;for(const auto& R:AIRecovery)Count+=R.GetAttempts();return Count;}
int32 AImperialMatchManager::GetAIRecoveringCount() const
{int32 Count=0;if(Mode==EImperialMode::AIPractice)for(const auto& R:AIRecovery)if(R.IsEscaping())++Count;return Count;}
