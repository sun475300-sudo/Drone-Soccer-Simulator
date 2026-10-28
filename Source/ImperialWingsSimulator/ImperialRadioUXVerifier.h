#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialRadioUXVerifier.generated.h"
class FJsonValue;

/** Isolated UI/language and file-failure checks. No selected device or injected USB samples. */
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialRadioUXVerifier : public AActor
{
    GENERATED_BODY()
public:
    AImperialRadioUXVerifier();
    virtual void Tick(float Dt) override;
private:
    int32 Stage=0;
    double Age=0;
    bool bOk=true;
    TArray<TSharedPtr<FJsonValue>> Checks;
    void Check(const TCHAR* Name,bool Passed);
    void CheckStorage();
    void CheckResponse(class AImperialHUD& Hud, class UImperialRadioSubsystem& Radio, class AImperialMatchManager& Match);
    void Finish();
};
