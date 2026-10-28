#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialFirstFlightVerifier.generated.h"
class FJsonValue;

/** Explicit isolated guide fixture; simulated engine keys never reach the OS or USB. */
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialFirstFlightVerifier : public AActor
{
    GENERATED_BODY()
public:
    AImperialFirstFlightVerifier();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
private:
    void Check(const TCHAR* Name,bool Passed,double Value=0);
    void Finish();
    void Next(int32 NewStage);
    int32 Stage=0;
    double Age=0,Total=0,MinDt=1e9,MaxDt=0,MaximumTakeoffAltitude=0;
    bool bOk=true,bUpHeld=false,bTakeoffObserved=false,bKeyboardStayed=true,bCaptureRequested=false;
    double ProbeStableAge=0,ProbeLaunchAge=-1,ProbeNextSampleAge=0;
    int32 ProbeGoalScore=0;
    bool bProbeHoverStable=false;
    FString Capture;
    TArray<TSharedPtr<FJsonValue>> Checks,ScoreProbeSamples;
};
