#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialAIResultsVerifier.generated.h"
class FJsonValue;
class AImperialHUD;
class AImperialMatchManager;
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialAIResultsVerifier:public AActor
{
    GENERATED_BODY()
public:
    AImperialAIResultsVerifier();
    virtual void Tick(float Dt) override;
private:
    void Check(const TCHAR* Name,bool Passed);
    void Finish();
    void CheckRunningSettings(AImperialHUD& Hud,AImperialMatchManager& Manager);
    double Age=0,Total=0;
    int32 Stage=0,ExpectedImpacts=0;
    bool bOk=true,bUISetupCaptured=false;
    FString Case,Capture,FirstId;
    TArray<TSharedPtr<FJsonValue>> Checks;
};
