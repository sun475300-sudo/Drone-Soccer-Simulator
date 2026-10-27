#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialFlightTrace.generated.h"

class ASoccerDronePawn;
class FArchive;

struct FImperialFlightTraceIntegral
{
    TWeakObjectPtr<ASoccerDronePawn> Drone;
    uint64 Epoch = 0;
    uint64 PhysicsStep = 0;
    uint64 FirstEpoch = 0;
    uint64 ResetCount = 0;
    double FlightSeconds = 0;
    double SensorSeconds = 0;
    double ConsumedAh = 0;
    double IntegratedCurrentAh = 0;
    double ObservedConsumedAh = 0;
    double IntegratedDeliveredJ = 0;
    double IntegratedMotorAppliedJ = 0;
    double MaxChargeIntervalErrorAh = 0;
    double CapacityAh = 0;
    bool bBatteryEnabled = false;
    bool bInitialized = false;
};

// Game-thread, per-physics-frame diagnostics. Does not apply input or forces.
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialFlightTrace : public AActor
{
    GENERATED_BODY()
public:
    AImperialFlightTrace();
    virtual ~AImperialFlightTrace() override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void RequestFinish(double SmokeElapsedSeconds, bool SmokeValid);
private:
    bool WriteText(const FString& Text);
    bool SampleFrame(float Dt);
    void Finish(const FString& Error = FString());
    bool CloseWriter();
    void RestoreTiming();

    FArchive* Writer = nullptr;
    FString CsvFile;
    FString ResultFile;
    FString Failure;
    FImperialFlightTraceIntegral Bodies[10];
    uint64 Frames = 0;
    uint64 Rows = 0;
    int32 RequestedFPS = 60;
    double WindowStart = 0;
    double WindowEnd = 0;
    double MinimumGameDt = TNumericLimits<double>::Max();
    double MaximumGameDt = 0;
    double CompletionSignalWorld = 0;
    double SmokeCompletionElapsed = 0;
    double PreviousFixedGameDt = 0;
    float PreviousMaxFPS = 0;
    bool bFinished = false;
    bool bFinishRequested = false;
    bool bSmokeValid = false;
    bool bFiniteBodies = true;
    bool bObserved = false;
    bool bTimingApplied = false;
    bool bPreviousFixedGameTime = false;
};
