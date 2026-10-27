#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialSolverProbe.generated.h"

namespace Chaos { class FPBDRigidsSolver; }
class FImperialSolverProbeCallback;
class FJsonValue;

// Explicit diagnostic only. The physics callback never accesses this UObject.
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialSolverProbe : public AActor
{
    GENERATED_BODY()
public:
    AImperialSolverProbe();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    void CompletePassthrough(double SmokeElapsedSeconds, TFunction<void(bool)> OnComplete);
private:
    void DrainOutputs();
    void ReleaseCallback();
    void Finish(const FString& Error = FString());
    void RestoreTiming();

    Chaos::FPBDRigidsSolver* Solver = nullptr;
    FImperialSolverProbeCallback* Callback = nullptr;
    FString ResultFile;
    FString Failure;
    int32 RequestedFPS = 60;
    double Duration = 8;
    double Warmup = 0;
    double GameElapsed = 0;
    double GameMinDt = TNumericLimits<double>::Max();
    double GameMaxDt = 0;
    double SolverMinDt = TNumericLimits<double>::Max();
    double SolverMaxDt = 0;
    double SolverElapsed = 0;
    double SolverStart = 0;
    double SolverEnd = 0;
    double GameStart = 0;
    double GameEnd = 0;
    double SolverInitialRemainder = 0;
    double IntegrationMinDt = 0;
    double IntegrationMaxDt = 0;
    double CompletionSignalWorld = 0;
    double SmokeCompletionElapsed = 0;
    double WallStart = 0;
    double PreviousAsyncDt = -1;
    double PreviousFixedGameDt = 0;
    float PreviousMaxFPS = 0;
    uint64 SolverSteps = 0;
    uint64 PreSimulates = 0;
    uint64 PreIntegrations = 0;
    uint64 PostIntegrations = 0;
    uint64 OrderErrors = 0;
    uint64 InvalidDtSteps = 0;
    uint64 NonSingleIntegrationSteps = 0;
    uint64 IntegrationDtViolations = 0;
    uint64 LastSequence = 0;
    uint64 GameFrames = 0;
    uint32 GameThreadId = 0;
    bool bFixedRequested = false;
    bool bPassthrough = false;
    bool bStarted = false;
    bool bFinished = false;
    bool bChangedAsyncDt = false;
    bool bTimingApplied = false;
    bool bPreviousFixedGameTime = false;
    bool bActualFixedDt = false;
    bool bFixedFlagStable = true;
    bool bIntegrationSettingsStable = true;
    bool bCompletionRequested = false;
    bool bCompletedBySmoke = false;
    bool bLastResultOk = false;
    TFunction<void(bool)> PassthroughCompletion;
    TSet<uint32> CallbackThreadIds;
    TArray<TSharedPtr<FJsonValue>> Samples;
};
