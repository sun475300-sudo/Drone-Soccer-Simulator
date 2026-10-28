#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialAIMotionVerifier.generated.h"

class AImperialMatchManager;
class ASoccerDronePawn;
class FJsonValue;

/** Opt-in native diagnostic. No device input or in-flight position writes. */
UCLASS()
class AImperialAIMotionVerifier : public AActor
{
    GENERATED_BODY()
public:
    AImperialAIMotionVerifier();
    virtual void Tick(float Dt) override;
private:
    void Check(const FString& Name,bool Passed,double Value=0);
    void Finish();
    void VerifySetter(AImperialMatchManager& Manager);
    void ObserveDuel(AImperialMatchManager& Manager);
    bool IsDefault(const ASoccerDronePawn& Drone) const;
    bool AllDefault(const AImperialMatchManager& Manager,int32 Except=INDEX_NONE) const;
    bool AllQuiet(const AImperialMatchManager& Manager) const;

    int32 Stage=0;
    double Age=0,Total=0,MinDt=1e9,MaxDt=0,NextSample=0,FirstGoal=-1;
    bool bReady=false,bOk=true,bFinished=false,bPlayerSafe=true,bOtherDefault=true;
    bool bCapsCorrect=true,bPrecisionDefault=true,bFinite=true,bLandingDefault=true;
    bool SawPhase[6]={false,false,false,false,false,false};
    int32 PhaseFrames[6]={},TransitFrames=0,PrecisionFrames=0,RecoverySkippedFrames=0;
    int32 GoalsObserved=0,CyclesObserved=0;
    FString ResultPath,DataDirectory;
    TArray<TSharedPtr<FJsonValue>> Checks,Samples;
};
