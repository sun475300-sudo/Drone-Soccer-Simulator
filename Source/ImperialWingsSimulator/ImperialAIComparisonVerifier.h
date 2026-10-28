#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialAIComparisonVerifier.generated.h"
class FJsonValue;
class FJsonObject;

/** Explicit fresh-data-only read-only comparison model fixture. No pilot/OS input. */
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialAIComparisonVerifier : public AActor
{
    GENERATED_BODY()
public:
    AImperialAIComparisonVerifier();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
private:
    void Check(const FString& Name,bool Passed);
    void Finish();
    bool RunCase(const TSharedPtr<FJsonObject>& Case,TSharedPtr<FJsonObject>& Report);
    bool bGuarded=false,bFinished=false,bOk=true;
    FString DataDir,ManifestPath,ResultFile,CorpusDir;
    double StartWall=0,MinDt=1e9,MaxDt=0,WaitSimulationAge=0,ReadySimulationAge=-1;
    int32 StableSafeTicks=0,WaitTicks=0;
    TArray<TSharedPtr<FJsonValue>> Checks,Cases,Reports,SafetyStates;
};
