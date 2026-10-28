#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialAIDuelVerifier.generated.h"
class FJsonValue;
class FJsonObject;
class AImperialMatchManager;
class ASoccerDronePawn;
UCLASS()
class AImperialAIDuelVerifier : public AActor
{
    GENERATED_BODY()
public:
    AImperialAIDuelVerifier();
    virtual void Tick(float Dt) override;
private:
    void Check(const TCHAR* Name,bool Passed,double Value=0);
    void Finish();
    // Diagnostic observation only: no targets, controls, device state or physics writes.
    void ResetPerformance();
    void RecordPerformance(AImperialMatchManager& Manager,ASoccerDronePawn& Striker,float Dt);
    void FinishPerformance(AImperialMatchManager& Manager,ASoccerDronePawn& Striker);
    TSharedRef<FJsonObject> SpeedStatistics(int32 Phase,bool Horizontal,bool TravelOnly) const;
    struct FObservedSpeed { double Speed=0,Horizontal=0,Seconds=0;int32 Phase=0; };
    TArray<FObservedSpeed> ObservedSpeeds;
    TArray<TSharedPtr<FJsonValue>> PerformanceRows,PerformanceMetrics;
    TArray<double> GoalTimes,CycleTimes;
    double PhaseSeconds[6]={},PerformanceClock=0,NextPerformanceRow=0;
    double MaximumStrikerTilt=0,MinimumRosterSeparation=1e9;
    int32 PerformanceLastScore=0,PerformanceLastCycles=0,OutOfBoundsFrames=0;

    int32 Case=0,Stage=0,CaseCount=6,ScoreAtStop=0;
    double Age=0,Total=0,LastSample=-1,MinDt=1e9,MaxDt=0;
    bool bOk=true,bPlayerSafe=true,bRosterSafe=true,bFinite=true,bSawRetreat=false;
    FString Capture;
    TArray<TSharedPtr<FJsonValue>> Checks,Samples;
};
