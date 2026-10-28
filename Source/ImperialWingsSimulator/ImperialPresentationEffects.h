#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialPresentationEffects.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPostProcessComponent;
class ASoccerDronePawn;
class URectLightComponent;

/** Bounded cosmetic effects driven by contact/thrust telemetry; never applies flight forces. */
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialPresentationEffects : public AActor
{
    GENERATED_BODY()
public:
    AImperialPresentationEffects();
    virtual void Tick(float DeltaSeconds) override;
    bool SetVisualQuality(const FString& Name,bool bSave=true);
    FString GetQualityName() const { return QualityName; }
    int32 GetDustCapacity() const { return Capacity; }
    int32 GetGuardChipBursts() const { return GuardChipBursts; }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    struct FDustMote
    {
        FVector Position = FVector::ZeroVector;
        FVector Velocity = FVector::ZeroVector;
        float Age = 0;
        float Lifetime = 1;
        float Radius = 1;
    };
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Dust;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> GuardChips;
    struct FGuardChip {FVector Position,Velocity;float Age=0,Lifetime=.5f,Size=.2f;};
    TArray<FGuardChip> Chips;
    int32 GuardChipBursts=0;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DustMaterial;
    UPROPERTY() TObjectPtr<UPostProcessComponent> Grade;
    UPROPERTY() TArray<TObjectPtr<URectLightComponent>> FillLights;
    TArray<FDustMote> Motes;
    TArray<TWeakObjectPtr<ASoccerDronePawn>> Aircraft;
    TMap<TWeakObjectPtr<AActor>, double> LastImpact;
    FRandomStream Random{71539};
    double Elapsed = 0;
    float DiscoveryClock = 1;
    float DownwashClock = 0;
    int32 Capacity = 192;
    int32 ImpactBursts = 0;
    int32 RotorBursts = 0;
    int32 PreviousVisible = 0;
    int32 PreviousVisibleChips = 0;
    bool bEffectsEnabled = true;
    bool bVerification = false;
    bool bFixtureRun = false;
    bool bFixtureReported = false;
    int32 FixturePeak = 0;
    int32 FixtureGuardStage = 0;
    int32 FixtureGuardPeak = 0;
    int32 FixtureGuardReappearanceCount = 0;
    int32 FixtureGuardUpdateCalls = 0;
    bool bFixtureGuardInitialCleared = false;
    bool bFixtureGuardIdleZeroUpdates = false;
    bool bFixtureGuardReappeared = false;
    bool bFixtureGuardFinalCleared = false;
    bool bFixtureGuardFinalIdleZeroUpdates = false;
    bool bFixtureGuardTransformsMatch = true;
    bool bFixtureGuardQualityChecked = false;
    bool bFixtureGuardQualityPreserved = false;
    FString QualityName=TEXT("Balanced");
    void DiscoverAircraft();
    void AddMote(const FVector& Position, const FVector& Velocity, float Radius, float Lifetime);
    void EmitImpact(const FVector& Point, const FVector& Normal, float SpeedCmS);
    void EmitGuardChips(const FVector& Point,const FVector& Normal,float ImpulseNs);
    void ApplyQuality();
    UFUNCTION() void OnAircraftHit(AActor* SelfActor, AActor* OtherActor,
        FVector NormalImpulse, const FHitResult& Hit);
};
