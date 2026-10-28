#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PhysicsModels.h"
#include "ContactResponse.h"
#include "SoccerDronePawn.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UImperialRadioSubsystem;
class UInstancedStaticMeshComponent;
class UTextRenderComponent;
class UPhysicalMaterial;
class UPrimitiveComponent;
class USceneComponent;
class UMaterialInstanceDynamic;
class FPhysScene_Chaos;

/** Experimental force-driven quadcopter. +X forward, +Y right, +Z up.
 *  All motor/controller constants are prototype assumptions, not measured aircraft data.
 *  N -> kg cm/s^2 uses 100; N m -> kg cm^2/s^2 uses 10000.
 */
UCLASS()
class IMPERIALWINGSSIMULATOR_API ASoccerDronePawn : public APawn
{
    GENERATED_BODY()

public:
    ASoccerDronePawn();
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
    virtual void UnPossessed() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    bool VerifyDetailedAirframe() const;
    bool HasPhotorealEquipment() const;
    void SetEquipmentInspection(bool bEnabled);
    bool IsEquipmentInspection() const { return bEquipmentInspection; }
    void ClearManualPilotInput(const FString& Reason);

    UFUNCTION(BlueprintCallable, Category="Drone|Autonomy")
    void SetAutonomousTarget(const FVector& WorldPosition);

    /** Explicit autonomous launch command. Disabling autonomy disarms the aircraft. */
    UFUNCTION(BlueprintCallable, Category="Drone|Autonomy")
    void SetAutonomousEnabled(bool bEnabled);

    /** Session-scoped horizontal controller ceilings; no motor/vertical/manual tuning.
     *  Finite values in 0.1-3.0 m/s and 0.1-4.0 m/s^2 only; rejection retains prior limits.
     *  AI duel transit is the only production caller. Reset/disable/end restore defaults.
     */
    bool SetAutonomousHorizontalLimits(float SpeedMps,float AccelerationMps2);
    void ResetAutonomousHorizontalLimits();
    float GetAutonomousHorizontalSpeedLimitMps() const { return AutonomousHorizontalSpeedLimitMps; }
    float GetAutonomousHorizontalAccelerationLimitMps2() const { return AutonomousHorizontalAccelerationLimitMps2; }

    UFUNCTION(BlueprintCallable, Category="Drone|Safety")
    bool SetArmed(bool bShouldArm);

    UFUNCTION(BlueprintCallable, Category="Drone|Safety")
    void ResetDrone();

    UFUNCTION(BlueprintCallable, Category="Drone|Camera")
    void SetChaseView(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category="Drone|Camera")
    void ToggleCamera();

    UFUNCTION(BlueprintCallable, Category="Drone|Appearance")
    void SetTeamColor(FLinearColor Color);

    /** FIDA Class 40: 0 Striker, 1 Guide, 2 Libero, 3 Sweeper, 4 Keeper. */
    UFUNCTION(BlueprintCallable, Category="Drone|Appearance")
    void SetPlayerRole(int32 PlayerRoleValue);

    UFUNCTION(BlueprintCallable, Category="Drone|Appearance")
    void SetRoleColor(FLinearColor Color);

    UFUNCTION(BlueprintPure, Category="Drone|Appearance")
    int32 GetPlayerRole() const { return PlayerRole; }

    UFUNCTION(BlueprintPure, Category="Drone|Appearance")
    FLinearColor GetRoleColor() const { return RoleColor; }

    UFUNCTION(BlueprintPure, Category="Drone|Telemetry")
    bool IsArmed() const { return bArmed; }

    UFUNCTION(BlueprintPure, Category="Drone|Telemetry")
    bool IsAutonomousEnabled() const { return bAutonomousEnabled; }

    UFUNCTION(BlueprintPure, Category="Drone|Telemetry")
    bool IsChaseView() const { return bChaseView; }

    UFUNCTION(BlueprintPure, Category="Drone|Telemetry")
    float GetThrottle() const;

    /** Flight speed in metres/second, including vertical velocity. */
    UFUNCTION(BlueprintPure, Category="Drone|Telemetry")
    float GetFlightSpeed() const;

    /** World Z in metres; not terrain-relative altitude. */
    UFUNCTION(BlueprintPure, Category="Drone|Telemetry")
    float GetAltitudeMeters() const;

    UFUNCTION(BlueprintPure, Category="Drone|Telemetry")
    FVector GetAutonomousTarget() const { return AutonomousTarget; }

    UFUNCTION(BlueprintPure, Category="Drone|Telemetry")
    float GetTotalThrustNewtons() const;

    UFUNCTION(BlueprintPure, Category="Drone|Telemetry")
    FString GetPilotStatus() const;

    FString GetInputSource() const;
    void SetWakeSources(const TArray<FVector>& PositionsM,const TArray<float>& ThrustsN);
    float GetBatteryPercent() const { return static_cast<float>(BatteryState.Soc*100.); }
    float GetBatteryVoltage() const { return static_cast<float>(BatteryState.VoltageV); }
    double GetBatterySoc() const { return BatteryState.Soc; }
    double GetBatteryVoltageV() const { return BatteryState.VoltageV; }
    double GetBatteryCapacityAh() const { return PhysicalParams.Battery.CapacityAh; }
    bool IsBatteryModelEnabled() const { return bBatteryModel; }
    uint64 GetBatteryResetEpoch() const { return BatteryResetEpoch; }
    uint64 GetPhysicsStepSequence() const { return PhysicsStepSequence; }
    double GetFlightModelSeconds() const { return FlightModelSeconds; }
    double GetSensorModelSeconds() const { return SensorElapsedSeconds; }
    double GetBatteryConsumedAh() const { return BatteryState.ConsumedAh; }
    double GetBatteryCurrentA() const { return BatteryState.CurrentA; }
    double GetBatteryDeliveredPowerW() const { return BatteryState.DeliveredPowerW; }
    bool IsBatteryPowerLimited() const { return BatteryState.PowerLimited; }
    double GetMotorElectricalPowerW() const;
    float GetThrottleCommand() const;
    bool UsesPrecisionThrottle() const;
    double GetRequestedMotorPowerW() const { return PowerBudget.RequestedMotorPowerW; }
    double GetAvailableMotorPowerW() const { return PowerBudget.AvailableMotorPowerW; }
    double GetMotorThrustScale() const { return PowerBudget.ThrustScale; }
    /** Explicit power-budget fixture only; never loads or changes a USB profile. */
    void SetPowerVerificationConditions(double MaximumCurrentA,double Soc);
    float GetNetDeflection() const;
    float GetGuardIntegrityPercent() const { return static_cast<float>(GuardState.Integrity*100); }
    int32 GetGuardImpactCount() const { return static_cast<int32>(GuardState.ImpactCount); }
    float GetGuardImpactWorkJ() const { return static_cast<float>(GuardState.TotalDissipatedJ); }
    FString GetFlightModeName() const { return TEXT("ANGLE"); }
    float GetTiltDegrees() const;
    float GetAngleLimitDegrees() const { return MaximumTiltDegrees; }
    FVector GetManualTargetRateDegrees() const { return LastManualTargetRateDegrees; }
    /** Explicit in-engine verification fixture; unavailable without its test flag. */
    void SetAngleVerificationInput(FVector Stick, float Throttle);

    UStaticMeshComponent* GetBodyMesh() const { return Body; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone")
    TObjectPtr<UStaticMeshComponent> Body;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone")
    TObjectPtr<UCameraComponent> FpvCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone")
    TObjectPtr<USpringArmComponent> ChaseArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone")
    TObjectPtr<UCameraComponent> ChaseCamera;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Physics", meta=(ClampMin="0.1"))
    float MassKg = 0.9f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Physics", meta=(ClampMin="0.01"))
    float ArmLengthM = 0.095f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Motors", meta=(ClampMin="0.1"))
    float MaxMotorThrustN = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Motors", meta=(ClampMin="0.0"))
    float ArmedIdleThrustN = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Motors", meta=(ClampMin="0.001"))
    float MotorResponseSeconds = 0.035f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Motors", meta=(ClampMin="0.001"))
    float YawReactionArmM = 0.022f;

    // X = roll, Y = pitch, Z = yaw, in degrees per second.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Rates")
    FVector MaxRateDegrees = FVector(600.0, 600.0, 420.0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Angle", meta=(ClampMin="5",ClampMax="60"))
    float MaximumTiltDegrees = 35.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Angle", meta=(ClampMin="0.1",ClampMax="15"))
    float AngleGainPerSecond = 4.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Angle", meta=(ClampMin="30",ClampMax="360"))
    float MaximumAngleRateDegrees = 180.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|PID")
    FVector RateKp = FVector(0.14, 0.14, 0.09);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|PID")
    FVector RateKi = FVector(0.012, 0.012, 0.01);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|PID")
    FVector RateKd = FVector(0.002, 0.002, 0.0015);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Physics")
    float LinearDragNsPerM = 0.035f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Physics")
    float QuadraticDragNPerSpeedSquared = 0.015f;

    // Safety: keyboard mode until a human verifies the radio's four axes.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Input")
    bool bUseBoxer = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Input")
    bool bBoxerCalibrated = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Input")
    bool bUsbAxisZeroToOne = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Input")
    bool bInvertRoll = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Input")
    bool bInvertPitch = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Input")
    bool bInvertYaw = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Input")
    bool bInvertThrottle = false;

private:
    FVector LastManualTargetRateDegrees=FVector::ZeroVector;
    bool bAngleVerificationInput=false;
    FVector AngleVerificationStick=FVector::ZeroVector;
    float AngleVerificationThrottle=0;
    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> RotorVisuals;
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> RotorPivots;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> TeamHighlights;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> FrameVisuals;

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> CageRods;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> CageJoints;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> MetalHardware;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> MotorVents;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> PowerWires;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> GroundWires;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> CageClips;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> GuardScuffs;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GuardMaterial;
    ImperialContact::GuardParameters GuardParameters;
    ImperialContact::GuardState GuardState;
    TWeakObjectPtr<AActor> LastWearOther;
    double LastWearContactTime=-100;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> EquipmentAssembly;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> DetailedPropellers;
    bool bPhotorealEquipment=false,bEquipmentInspection=false;

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> TeamLedElements;

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> RoleLedElements;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> EquatorFrame;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> FrontLedElements;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> RearLedElements;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> PropellerBlades;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> RoleTagPlate;

    UPROPERTY()
    TObjectPtr<UTextRenderComponent> RoleTagLetter;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> DarkVisuals;

    bool bArmed = false;
    bool bLastArmed = false;
    bool bAutonomousEnabled = false;
    bool bChaseView = true;
    bool bRadioFlight = false;
    float KeyboardThrottle = 0.0f;
    float KeyboardThrottleStep = 0.0f;
    float KeyRoll = 0.0f;
    float KeyPitch = 0.0f;
    float KeyYaw = 0.0f;
    float UsbRoll = 0.0f;
    float UsbPitch = 0.0f;
    float UsbYaw = 0.0f;
    float UsbThrottle = 0.0f;
    float AutonomousHorizontalSpeedLimitMps = 2.2f;
    float AutonomousHorizontalAccelerationLimitMps2 = 3.2f;
    float AutonomousThrottle = 0.0f;
    float AutonomousHeading = 0.0f;
    float RotorAngle = 0.0f;
    double FlightModelSeconds = 0;
    uint64 BatteryResetEpoch = 0;
    uint64 PhysicsStepSequence = 0;
    double SensorElapsedSeconds = 0;
    float PreviousPhysicsSeconds = 0;
    void PhysicsStep(FPhysScene_Chaos* Scene,float Dt);
    float MotorThrustN[4] = {0, 0, 0, 0};
    FVector IntegralError = FVector::ZeroVector;
    FVector AutonomousVelocityIntegral = FVector::ZeroVector;
    FVector PreviousBodyRate = FVector::ZeroVector;
    FVector FilteredBodyAcceleration = FVector::ZeroVector;
    FVector SpawnPosition = FVector::ZeroVector;
    FVector AutonomousTarget = FVector::ZeroVector;
    FQuat SpawnRotation = FQuat::Identity;
    FLinearColor TeamColor = FLinearColor(0.015f, 0.05f, 1.0f);
    FLinearColor RoleColor = FLinearColor(0.015f, 1.0f, 0.035f);
    int32 PlayerRole = 0;
    FString LastSafetyMessage;
    UPROPERTY() TObjectPtr<UPhysicalMaterial> CagePhysicalMaterial;
    ImperialPhysics::Parameters PhysicalParams;
    ImperialPhysics::BatteryState BatteryState;
    ImperialPhysics::PowerBudgetResult PowerBudget;
    std::array<ImperialPhysics::MotorState,4> MotorStates;
    std::array<ImperialPhysics::NetState,5> NetStates;
    ImperialPhysics::SensorPipeline Sensors;
    ImperialPhysics::SensorReading SensorSample;
    FVector PreviousVelocityMS=FVector::ZeroVector;
    FVector CenterOfMassLocalM=FVector::ZeroVector;
    TArray<FVector> WakePositionsM;
    TArray<float> WakeThrustsN;
    double PhysicalTime=0;
    float CageCompression=0;
    int32 CageCompressionAxis=0;
    bool bBatteryModel=true,bWakeModel=true,bSurfaceModel=true,bSensorModel=true,bNonlinearMotors=true;
    UFUNCTION() void OnBodyHit(UPrimitiveComponent* HitComponent,AActor* OtherActor,UPrimitiveComponent* OtherComponent,FVector NormalImpulse,const FHitResult& Hit);
    void InitializePhysicalModel();
    void ApplyNetForces(float Dt);
    FVector AirVelocityAt(const FVector& WorldPointM) const;

    void ReadKeyboardRoll(float V) { KeyRoll = V; }
    void ReadKeyboardPitch(float V) { KeyPitch = V; }
    void ReadKeyboardYaw(float V) { KeyYaw = V; }
    void ReadKeyboardThrottle(float V) { KeyboardThrottleStep = V; }
    void ReadBoxerRoll(float V) { UsbRoll = V; }
    void ReadBoxerPitch(float V) { UsbPitch = V; }
    void ReadBoxerYaw(float V) { UsbYaw = V; }
    void ReadBoxerThrottle(float V) { UsbThrottle = V; }

    void ToggleArm();
    float NormalizeUsb(float Value, bool bInvert) const;
    void ClearController();
    void ApplyFlightForces(float Dt);
    void ApplyAerodynamicDrag();
    void ApplyVisualMaterials();
    void BuildCageVisuals();
    void BuildDetailedEquipment();
    void UpdateGuardMaterial();
    void ResetFromPilotInput();
    void UpdateRoleTag();
    FVector CalculateAutonomousRate(float& OutCollectiveThrustN,float Step);
    UImperialRadioSubsystem* GetRadio() const;
};
