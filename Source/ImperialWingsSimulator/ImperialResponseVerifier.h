#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialResponseVerifier.generated.h"
class FJsonValue;
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialResponseVerifier:public AActor
{
    GENERATED_BODY()
public:
    AImperialResponseVerifier();
    virtual void Tick(float Dt) override;
private:
    void Check(const TCHAR* Name,bool Passed,double Value=0);
    void Finish();
    float Age=0;
    double PeakRollRate=0;
    bool bRollRateInRange=true;
    int32 Stage=0;
    bool bOk=true;
    FString Case;
    TArray<TSharedPtr<FJsonValue>> Checks;
};
