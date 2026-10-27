#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialSolverBridgeVerifier.generated.h"

namespace Chaos { class FPBDRigidsSolver; }
class USphereComponent;
class FImperialSolverBridgeCallback;
class FJsonValue;
struct FImperialBridgeLifetime;

// Opt-in isolated rigid-body fixture. It never takes ownership of a drone.
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialSolverBridgeVerifier : public AActor
{
    GENERATED_BODY()
public:
    AImperialSolverBridgeVerifier();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void PublishCommand(double SolverNow);
    void DrainOutputs();
    void ReleaseCallback();
    void RestoreTiming();
    void Finish(const FString& Error = FString());
    UPROPERTY() TObjectPtr<USphereComponent> Fixture;
    Chaos::FPBDRigidsSolver* Solver = nullptr;
    FImperialSolverBridgeCallback* Callback = nullptr;
    TSharedPtr<FImperialBridgeLifetime, ESPMode::ThreadSafe> Lifetime;
    TArray<TSharedPtr<FJsonValue>> Samples;
    TArray<TSharedPtr<FJsonValue>> Commands;
    FString ResultFile;
    FString Failure;
    uint64 PublishIndex = 0;
    uint64 Sequence = 0;
    uint64 StepCount = 0;
    uint64 GameFrames = 0;
    uint64 ForceCalls = 0;
    uint64 TorqueCalls = 0;
    uint64 OrderErrors = 0;
    double Warmup = 0;
    double StartTime = 0;
    double LastSolverEnd = 0;
    double SolverElapsed = 0;
    double GameElapsed = 0;
    double GameMinDt = TNumericLimits<double>::Max();
    double GameMaxDt = 0;
    double SolverMinDt = TNumericLimits<double>::Max();
    double SolverMaxDt = 0;
    double IntegrationMinDt = 0;
    double IntegrationMaxDt = 0;
    double MaxLinearError = 0;
    double MaxAngularError = 0;
    double QuiesceElapsed = 0;
    double CoastError = 0;
    double CoastAngularError = 0;
    double PreviousGameDt = 0;
    float PreviousMaxFPS = 0;
    int32 RequestedFPS = 60;
    int32 LastPublishedPhase = -1;
    bool bPreviousFixed = false;
    bool bTimingApplied = false;
    bool bStarted = false;
    bool bFinished = false;
    bool bQuiescing = false;
    bool bUnregistered = false;
    bool bNormalDronesDisarmed = true;
    FVector LastVelocity = FVector::ZeroVector;
    FVector LastAngularVelocity = FVector::ZeroVector;
    uint32 PhaseRows[9] = {};
};
