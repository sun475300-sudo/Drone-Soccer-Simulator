#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "DroneSoccerRules.h"
#include "GuidanceRequestWindow.h"
#include "SimulationTime.h"
#include "AITraining.h"
#include "AIPracticeResult.h"
#include "AIResultComparison.h"
#include "AIRecovery.h"
#include "AIAttacker.h"
#include "ImperialMatchManager.generated.h"
class ASoccerDronePawn;
class FJsonValue;
class FJsonObject;
class FPhysScene_Chaos;
enum class EImperialMode { Manual, Mission, Hold, Returning, AIPractice, AILanding };
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialMatchManager : public AActor
{
    GENERATED_BODY()
public:
    AImperialMatchManager();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;
    const TArray<TObjectPtr<ASoccerDronePawn>>& GetDrones() const { return Drones; }
    ASoccerDronePawn* GetDrone(int32 Id) const;
    ASoccerDronePawn* GetSelectedDrone() const;
    int32 GetSelected() const { return Selected; }
    void SelectDrone(int32 Id);
    void SelectNext();
    void LaunchSwarm();
    void HoldFleet();
    void ReturnFleet();
    void ManualSelected();
    void ResetMatch();
    bool PrepareToExit();
    void StartMatch();
    bool StartAIPractice(ImperialAI::Settings Settings);
    void StopAIPractice();
    bool IsAIPracticeActive() const { return Mode==EImperialMode::AIPractice||Mode==EImperialMode::AILanding; }
    bool IsAILanding() const { return Mode==EImperialMode::AILanding; }
    ImperialAI::Settings GetAISettings() const { return AISettings; }
    int32 GetAIDecisions() const { return AIPlanner.DecisionCount(); }
    int32 GetAIRecoveryAttempts() const;
    int32 GetAIRecoveringCount() const;
    FString GetAIStatus() const;
    int32 GetAIAttackCycles() const { return AIAttacker.GetCycles(); }
    int32 GetAIAttackPhase() const { return static_cast<int32>(AIAttacker.GetPhase()); }
    bool IsAIDuelFinished() const { return bAIScoringClosed; }
    const FImperialAIResult& GetAIResult() const { return LastAIResult; }
    bool SaveAIResult();
    void DiscardUnsavedAIResult();
    void OpenAIComparison();
    void CloseAIComparison();
    bool RefreshAIComparison();
    void MoveAIComparisonPage(int32 Delta);
    void SelectAIComparisonRecord(int32 Slot,int32 Index);
    void ShowAIComparisonOutcome(bool bShow);
    void MoveAIComparisonDetailsPage(int32 Delta);
    const FImperialAIComparisonView& GetAIComparisonView() const { return AIComparison; }
    bool IsArmingAllowed() const { return Rules.IsArmingAllowed(); }
    bool IsAutonomyAllowed() const { return Rules.GetPhase()==drone_soccer::Phase::Practice; }
    FString GetMatchStatus() const;
    int32 GetSetWins(int32 Team) const { return Rules.SetWins(Team); }
    bool IsOffside(int32 Team) const { return Rules.IsOffside(Team); }
    void ToggleArmSelected();
    void ChangeView(int32 View);
    bool ExportTelemetry();
    void RecordGuardImpact(ASoccerDronePawn* Drone,float DissipatedJ);
    void ToggleFlightRecording();
    bool IsRecordingFlight() const { return !FlightLogPath.IsEmpty(); }
    bool HasFlightRecordingFailure() const { return bFlightRecordingFailure; }
    bool IsConnected() const { return bConnected; }
    FString GetSourceCommit() const { return SourceCommit; }
    uint64 GetGuidanceCommand() const { return GuidanceWindow.Command(); }
    FString GetModeName() const;
    FString GetNotice() const { return Notice; }
    int32 GetScore(int32 Team) const { return Rules.Score(Team); }
    double GetElapsed() const { return AISettings.Duel&&(IsAIPracticeActive()||bAIScoringClosed)?FMath::Max(0.,180.-AIAge):(Rules.GetPhase()==drone_soccer::Phase::Practice?Elapsed:Rules.Remaining()); }
    float GetFPS() const { return SmoothFPS; }
    const FImperialSimulationTime& GetSimulationTime() const { return SimulationTime; }
    float GetClosestSeparation() const { return ClosestSeparation; }
    int32 GetConflicts() const { return Conflicts; }
    int32 GetView() const { return CameraView; }
    FLinearColor GetTeamColor(int32 Id) const;
    const TArray<FString>& GetEvents() const { return Events; }
private:
    ImperialAI::Settings AISettings;
    ImperialAI::Planner AIPlanner;
    ImperialAI::Attacker AIAttacker;
    bool bAIScoringClosed=false;
    std::array<ImperialAI::Recovery,4> AIRecovery;
    TArray<FVector> AILandingTargets;
    double AIAge=0,AILandingAge=0;
    bool bAILandingFault=false;
    void TickAIPractice(float Dt);
    FImperialAIResult CurrentAIResult,LastAIResult;
    FImperialAIComparisonView AIComparison;
    void UpdateAIComparisonOutcome();
    void SnapshotAIResult(const FString& Reason);
    void FinishAIResult(const FString& Reason);
    void LoadAIResult();
    FImperialSimulationTime SimulationTime;
    void OnPhysicsTime(FPhysScene_Chaos* Scene,float Dt);
    UPROPERTY() TArray<TObjectPtr<ASoccerDronePawn>> Drones;
    TArray<FVector> Homes;
    TArray<bool> GoalReady;
    TArray<FVector> PreviousPositions;
    TArray<FString> Events;
    drone_soccer::MatchRules Rules;
    EImperialMode Mode=EImperialMode::Manual;
    int32 Selected=0;
    int32 CameraView=1;
    int32 BlueScore=0,OrangeScore=0,Conflicts=0;
    double Elapsed=0,LastResponse=-100;
    float ClosestSeparation=0,SmoothFPS=60;
    bool bConnected=false,bRequestPending=false,bSmoke=false,bSmokeDone=false,bDemo=false,bDemoStarted=false;
    bool bContactSmoke=false,bViewSmoke=false,bSwarmSmoke=false;
    float FixtureAge=0,FixtureGuardFirst=100,FixtureGuardSecond=100,FixtureGoalPeak=0;
    int32 FixtureGoalHits=0;
    bool bFixtureOk=true;
    TArray<TSharedPtr<FJsonValue>> FixtureViewRows;
    TSharedPtr<FJsonObject> ContactVerification;
    float FixtureStableGoalPeak=0;
    TArray<FVector> SwarmHoldPositions;
    TArray<TSharedPtr<FJsonValue>> SwarmCheckpoints;
    double SwarmStageStart=0,SwarmLastSample=-1;
    float SwarmMinSeparation=100,SwarmMaxTilt=0,SwarmHoldDrift=0,SwarmMaxAltitude=0;
    bool bSwarmFinite=true,bSwarmAirborne=false,bSwarmTimeoutHeld=false,bSwarmReconnectHeld=false;
    int32 SmokePhase=0;
    float SmokeMaxHeight=0,SmokeMinHeight=10000;
    float SmokeMinDt=1000,SmokeMaxDt=0;
    float SmokeHoverError=0,SmokeNetPeak=0,SmokeNetVisualPeak=0,SmokeNetRebound=0,SmokeNetMinY=0;
    bool bSmokeFloorWall=false,bSmokeNet=false,bSmokeGoal=false;
    bool bAngleSmoke=false,bAngleInitialLevel=false,bAngleReleasedLevel=false;
    double AnglePhaseStart=0;
    float AnglePeak=0,AngleInputPeak=0,AngleReleaseWorst=0;
    FString SourceCommit;
    FString GuidanceSession;
    FString GuidanceUrl=TEXT("http://127.0.0.1:8766");
    ImperialGuidance::RequestWindow GuidanceWindow;
    FHttpRequestPtr PendingGuidance;
    double LastRequestWallTime=0;
    FString CaptureFile;
    FString SmokeResultFile;
    double CaptureAt=10;
    bool bCaptureRequested=false;
    TArray<double> FrameTimes;
    bool bProfileWritten=false;
    FString Notice=TEXT("Keyboard ready. Select AUTO FLEET for SDACS autonomous guidance.");
    FString FlightLogPath;
    bool bFlightRecordingFailure=false; // cleared only by a successful explicit new recording
    double LastFlightRecord=0;
    bool RecordFlightSample(const FString& Event);
    void RequestGuidance();
    void OnGuidance(FHttpRequestPtr Request,FHttpResponsePtr Response,bool bSuccess,ImperialGuidance::Ticket Ticket);
    void AdvanceGuidanceCommand();
    void CancelPendingGuidance();
    FString GuidanceMode() const;
    void Log(const FString& Message);
    void UpdateScoring();
    void HandleMatchPhaseTransition(drone_soccer::Phase Previous);
    bool VerifyMatchAutonomyGates();
    void SmokeTick();
    void AngleSmokeTick();
    void ContactSmokeTick(float Dt);
    void ViewSmokeTick(float Dt);
    void SwarmSmokeTick();
    void SwarmCheckpoint(const FString& Event);
};
