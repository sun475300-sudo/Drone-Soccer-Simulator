#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FirstFlightGuide.h"
#include "ImperialFirstFlightGuide.generated.h"
class AImperialMatchManager;

/** Session-only, observation-only first-flight guide. It never arms or flies an aircraft. */
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialFirstFlightGuide : public AActor
{
    GENERATED_BODY()
public:
    AImperialFirstFlightGuide();
    virtual void Tick(float DeltaSeconds) override;
    bool BeginGuide();
    bool AdvanceStep();
    void RetryStep();
    void StopGuide();
    /** Explicit user reset only: resets drone 0, never the match or other aircraft. */
    void ResetAircraftAndRestart();
    bool IsActive() const { return Guide.IsActive(); }
    bool IsStepComplete() const { return Guide.IsStepComplete(); }
    int32 GetStepIndex() const { return static_cast<int32>(Guide.GetStep()); }
    int32 GetStepCount() const { return 6; }
    float GetProgress() const { return static_cast<float>(Guide.GetProgress()); }
    FString GetStatus() const;
    FString GetTitle() const;
    FString GetInstruction() const;
    FString GetSecondaryHint() const;
private:
    ImperialFirstFlight::Guide Guide;
    UPROPERTY() TWeakObjectPtr<AImperialMatchManager> Match;
    FString MessageKorean,MessageEnglish;
    AImperialMatchManager* GetMatch();
    bool ObserveWorld(ImperialFirstFlight::Observation& Observation,bool* bAllGrounded=nullptr);
    void ClearMessage();
    void SetMessage(const TCHAR* Korean,const TCHAR* English);
};
