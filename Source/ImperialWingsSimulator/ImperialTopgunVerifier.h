#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialTopgunVerifier.generated.h"
class FJsonValue;
UCLASS()
class AImperialTopgunVerifier : public AActor
{
    GENERATED_BODY()
public:
    AImperialTopgunVerifier();
    virtual void Tick(float Dt) override;
private:
    void Check(const TCHAR* Name,bool Passed,double Value=0);
    void Finish();
    int32 Case=0,Stage=0,Leg=0,CaseCount=6;
    double Age=0,Total=0,LastSample=-1,MinDt=1e9,MaxDt=0,CompletedTime=0;
    bool bOk=true,bSafe=true,bChangedClock=false;
    FString Capture;
    TArray<TSharedPtr<FJsonValue>> Checks,Samples;
};
