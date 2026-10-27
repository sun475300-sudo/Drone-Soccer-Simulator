#include "ImperialSolverBridgeVerifier.h"
#include "ImperialBuildInfo.h"
#include "ImperialMatchManager.h"
#include "SoccerDronePawn.h"
#include "Chaos/SimCallbackObject.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PBDRigidsSolver.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Components/SphereComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include <limits>

namespace
{
constexpr uint32 BoundGeneration = 1;
constexpr double CommandTTL = .1;
constexpr double PhaseDuration = .5;
constexpr double Duration = 9 * PhaseDuration;
constexpr double MassKg = 2;
constexpr double RadiusM = .25;
const FVector CommandForce(2, -1, .5);
const FVector CommandOffset(0, .05, 0); // World-space displacement FROM the centre of mass.

bool Finite(const FVector& V) { return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z); }
TArray<TSharedPtr<FJsonValue>> VectorJson(const FVector& V)
{
    return { MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z) };
}
bool DisarmedArena(UWorld* World)
{
    const auto* Manager = Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(World, AImperialMatchManager::StaticClass()));
    if (!Manager || Manager->GetDrones().Num() != 10) return false;
    for (const auto& Drone : Manager->GetDrones()) if (!Drone || Drone->IsArmed() || Drone->IsAutonomousEnabled()) return false;
    return true;
}

// Snapshot contains values only. No UObject, component or input-device access in physics.
struct FBridgeCommand
{
    uint64 PublishIndex = 0;
    uint64 Sequence = 0;
    uint32 Generation = BoundGeneration;
    int32 Phase = 0;
    uint8 ReplayKind = 0;
    double Issued = 0;
    bool bArmed = false;
    FVector ForceN = FVector::ZeroVector;
    FVector OffsetM = FVector::ZeroVector;
};
struct FBridgeInput : Chaos::FSimCallbackInput
{
    FBridgeCommand Command;
    void Reset() { Command = FBridgeCommand(); }
};
struct FBridgeOutput : Chaos::FSimCallbackOutput
{
    FBridgeCommand Command;
    uint64 Sequence = 0;
    double Time = 0;
    double Dt = 0;
    double Mass = 0;
    FVector Inertia = FVector::ZeroVector;
    FVector PreV = FVector::ZeroVector;
    FVector PostV = FVector::ZeroVector;
    FVector PreW = FVector::ZeroVector;
    FVector PostW = FVector::ZeroVector;
    FVector PreOrigin = FVector::ZeroVector;
    FVector PreCOM = FVector::ZeroVector;
    FVector Point = FVector::ZeroVector;
    FQuat PreQ = FQuat::Identity;
    FVector Force = FVector::ZeroVector;
    FVector Torque = FVector::ZeroVector;
    uint32 PreSimulates = 0, PreIntegrates = 0, PostIntegrates = 0;
    uint32 ForceCalls = 0, TorqueCalls = 0, Errors = 0;
    uint8 Stage = 0;
    const TCHAR* Decision = TEXT("missing");
    bool bApplied = false;
    bool bNumbersFinite = true;
    void Reset() { *this = FBridgeOutput(); }
};

void CommandJson(TSharedPtr<FJsonObject> Object, const FBridgeCommand& C)
{
    Object->SetNumberField(TEXT("publish_index"), C.PublishIndex);
    Object->SetNumberField(TEXT("command_seq"), C.Sequence);
    Object->SetNumberField(TEXT("input_generation"), C.Generation);
    Object->SetNumberField(TEXT("phase"), C.Phase);
    Object->SetStringField(TEXT("replay_kind"), C.ReplayKind == 1 ? TEXT("same_sequence_new_snapshot") : C.ReplayKind == 2 ? TEXT("older_sequence") : TEXT("none"));
    Object->SetNumberField(TEXT("issued_s"), C.Issued);
    Object->SetBoolField(TEXT("armed"), C.bArmed);
    const bool bFinite = FMath::IsFinite(C.Issued) && Finite(C.ForceN) && Finite(C.OffsetM);
    Object->SetBoolField(TEXT("input_numbers_finite"), bFinite);
    // JSON cannot represent NaN; the explicit flag records deliberate invalid input.
    Object->SetArrayField(TEXT("command_force_n"), VectorJson(Finite(C.ForceN) ? C.ForceN : FVector::ZeroVector));
    Object->SetArrayField(TEXT("offset_m"), VectorJson(C.OffsetM));
}
}

// Shared POD acknowledgement outlives the callback and contains no UObject references.
struct FImperialBridgeLifetime
{
    TAtomic<bool> Shutdown { false };
    TAtomic<bool> Deleted { false };
    TAtomic<uint64> CallsAfterShutdown { 0 };
};

class FImperialSolverBridgeCallback final : public Chaos::TSimCallbackObject<FBridgeInput, FBridgeOutput,
    Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::PreIntegrate | Chaos::ESimCallbackOptions::PostIntegrate>
{
public:
    FImperialSolverBridgeCallback(Chaos::FSingleParticlePhysicsProxy* InProxy, TSharedPtr<FImperialBridgeLifetime, ESPMode::ThreadSafe> InLifetime)
        : Proxy(InProxy), Lifetime(MoveTemp(InLifetime)) {}
    virtual ~FImperialSolverBridgeCallback() override { Lifetime->Deleted.Store(true); }
    virtual FName GetFNameForStatId() const override { static const FName Name(TEXT("ImperialSolverBridge")); return Name; }
private:
    Chaos::FSingleParticlePhysicsProxy* const Proxy; // Bound once; actor owns its body until deletion acknowledgement.
    TSharedPtr<FImperialBridgeLifetime, ESPMode::ThreadSafe> Lifetime;
    FBridgeCommand Held;
    uint64 HighestCommand = 0;
    uint64 Sequence = 0;
    bool bCurrentSequenceValid = false;
    virtual void OnPreSimulate_Internal() override
    {
        if (Lifetime->Shutdown.Load()) ++Lifetime->CallsAfterShutdown;
        auto& Out = GetProducerOutputData_Internal();
        if (Out.Stage != 0) ++Out.Errors;
        Out.Sequence = ++Sequence;
        Out.Time = GetSimTime_Internal();
        Out.Dt = GetDeltaTime_Internal();
        ++Out.PreSimulates;
        Out.Stage = 1;
        if (Lifetime->Shutdown.Load()) { Out.Decision = TEXT("shutdown"); ++Out.Errors; return; }
        const auto* Input = GetConsumerInput_Internal();
        if (Input && Input->Command.PublishIndex != Held.PublishIndex)
        {
            // Reusing one immutable snapshot over substeps is valid. A NEW publication
            // must advance the command sequence; equal numbers cannot refresh a lease
            // or replace a rejected packet with an armed payload.
            bCurrentSequenceValid = Input->Command.PublishIndex > Held.PublishIndex && Input->Command.Sequence > HighestCommand;
            Held = Input->Command;
            HighestCommand = FMath::Max(HighestCommand, Held.Sequence);
        }
        Out.Command = Held;
        Out.bNumbersFinite = FMath::IsFinite(Held.Issued) && Finite(Held.ForceN) && Finite(Held.OffsetM);
        if (Held.PublishIndex == 0) Out.Decision = TEXT("missing");
        else if (!bCurrentSequenceValid) Out.Decision = TEXT("sequence");
        else
        {
            // A rejected new packet replaces the old ARM command, never refreshes it.
            if (Held.Generation != BoundGeneration) Out.Decision = TEXT("generation");
            else if (!Out.bNumbersFinite) Out.Decision = TEXT("nonfinite");
            else if (Held.Issued > Out.Time + 1e-8) Out.Decision = TEXT("future");
            else if (Out.Time - Held.Issued > CommandTTL) Out.Decision = TEXT("stale");
            else if (!Held.bArmed) Out.Decision = TEXT("disarmed");
            else Out.Decision = TEXT("applied");
        }
    }
    virtual void OnPreIntegrate_Internal() override
    {
        auto& Out = GetProducerOutputData_Internal();
        ++Out.PreIntegrates;
        if (Out.Stage != 1) { ++Out.Errors; return; } // Never apply twice within one solver advance.
        Out.Stage = 2;
        if (Lifetime->Shutdown.Load()) { Out.Decision = TEXT("shutdown"); ++Out.Errors; return; }
        auto* Handle = Proxy ? Proxy->GetPhysicsThreadAPI() : nullptr;
        if (!Handle || Handle->ObjectState() != Chaos::EObjectStateType::Dynamic || !FMath::IsFinite(Out.Dt) || Out.Dt <= 0)
        { Out.Decision = TEXT("body_invalid"); ++Out.Errors; return; }
        Out.PreV = FVector(Handle->GetV()) / 100.;
        Out.PreW = FVector(Handle->GetW());
        Out.PreQ = FQuat(Handle->GetR());
        Out.PreOrigin = FVector(Handle->GetX()) / 100.;
        Out.PreCOM = FVector(Handle->GetX() + Handle->GetR().RotateVector(Handle->CenterOfMass())) / 100.;
        Out.Point = Out.PreCOM + Held.OffsetM;
        Out.Mass = Handle->M();
        Out.Inertia = FVector(Handle->I()) / 10000.;
        if (FCString::Strcmp(Out.Decision, TEXT("applied")) != 0) return;
        // Single conversion boundary: N -> kg*cm/s², Nm -> kg*cm²/s².
        // COM torque matches Epic's vehicle DeferredForces adapter. No external AddForce path exists for this fixture.
        Out.Force = Held.ForceN;
        Out.Torque = FVector::CrossProduct(Out.Point - Out.PreCOM, Out.Force);
        Handle->AddForce(Chaos::FVec3(Out.Force * 100.), false);
        ++Out.ForceCalls;
        Handle->AddTorque(Chaos::FVec3(Out.Torque * 10000.), false);
        ++Out.TorqueCalls;
        Out.bApplied = true;
    }
    virtual void OnPostIntegrate_Internal() override
    {
        auto& Out = GetProducerOutputData_Internal();
        ++Out.PostIntegrates;
        if (Out.Stage != 2) ++Out.Errors;
        Out.Stage = 3;
        if (Lifetime->Shutdown.Load()) { ++Out.Errors; return; }
        if (auto* Handle = Proxy ? Proxy->GetPhysicsThreadAPI() : nullptr)
        {
            // V/W are integrated now. X/R are not final transforms here, so do not report them as post-step positions.
            Out.PostV = FVector(Handle->GetV()) / 100.;
            Out.PostW = FVector(Handle->GetW());
        }
        else ++Out.Errors;
    }
};

AImperialSolverBridgeVerifier::AImperialSolverBridgeVerifier()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    Fixture = CreateDefaultSubobject<USphereComponent>(TEXT("IsolatedBridgeFixture"));
    SetRootComponent(Fixture);
    Fixture->InitSphereRadius(RadiusM * 100.);
    Fixture->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
    Fixture->SetCollisionResponseToAllChannels(ECR_Ignore);
    Fixture->SetSimulatePhysics(true);
    Fixture->SetEnableGravity(false);
    Fixture->SetLinearDamping(0);
    Fixture->SetAngularDamping(0);
    Fixture->BodyInstance.SleepFamily = ESleepFamily::Custom;
    Fixture->BodyInstance.CustomSleepThresholdMultiplier = 0;
    Fixture->BodyInstance.bStartAwake = true;
    Fixture->SetVisibility(false);
}

void AImperialSolverBridgeVerifier::BeginPlay()
{
    Super::BeginPlay();
    FParse::Value(FCommandLine::Get(), TEXT("ImperialSolverBridgeResult="), ResultFile);
    if (ResultFile.IsEmpty()) ResultFile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Verification/solver-bridge.json"));
    if (!FParse::Param(FCommandLine::Get(), TEXT("ImperialSolverBridgeSmoke"))) { bFinished = true; SetActorTickEnabled(false); return; }
    FParse::Value(FCommandLine::Get(), TEXT("FPS="), RequestedFPS);
    if (RequestedFPS != 20 && RequestedFPS != 30 && RequestedFPS != 45 && RequestedFPS != 60 && RequestedFPS != 120 && RequestedFPS != 240)
    { Finish(TEXT("Use FPS 20/30/45/60/120/240.")); return; }
    for (const TCHAR* Flag : { TEXT("ImperialPhysicsSmoke"), TEXT("ImperialSolverProbe"), TEXT("ImperialSolverProbeFixed"), TEXT("ImperialFlightTrace"), TEXT("ImperialInputSmoke"), TEXT("ImperialTimeSmoke"), TEXT("ImperialRadioModeSmoke"), TEXT("ImperialPowerSmoke"), TEXT("ImperialThrottleSmoke") })
        if (FParse::Param(FCommandLine::Get(), Flag)) { Finish(TEXT("Solver bridge conflict: cannot run with another smoke, trace or solver probe.")); return; }
    bPreviousFixed = FApp::UseFixedTimeStep();
    PreviousGameDt = FApp::GetFixedDeltaTime();
    PreviousMaxFPS = GEngine ? GEngine->GetMaxFPS() : 0;
    FApp::SetFixedDeltaTime(1. / RequestedFPS);
    FApp::SetUseFixedTimeStep(true);
    if (GEngine) GEngine->SetMaxFPS(RequestedFPS);
    bTimingApplied = true;
    // Updating mass resolves a physical material through GEngine. Cook loads
    // native class defaults before GEngine exists, so do this only on the live fixture.
    Fixture->SetMassOverrideInKg(NAME_None, MassKg);
    // Explicit test reset only; subsequent movement is produced entirely by Chaos forces.
    SetActorLocationAndRotation(FVector(10000, 10000, 10000), FRotator(23, -17, 31), false, nullptr, ETeleportType::TeleportPhysics);
    Fixture->SetPhysicsLinearVelocity(FVector::ZeroVector);
    Fixture->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    Fixture->WakeAllRigidBodies();
}

void AImperialSolverBridgeVerifier::PublishCommand(double SolverNow)
{
    FBridgeCommand C;
    C.PublishIndex = ++PublishIndex;
    C.Sequence = ++Sequence;
    C.Phase = FMath::Clamp(FMath::FloorToInt((SolverNow - StartTime + 1e-8) / PhaseDuration), 0, 8);
    C.Issued = SolverNow;
    C.ForceN = CommandForce;
    C.OffsetM = CommandOffset;
    C.bArmed = C.Phase != 0 && C.Phase != 2 && C.Phase != 8;
    if (C.Phase == 3) C.Issued -= 1.;
    if (C.Phase == 4) C.Generation = BoundGeneration + 1;
    if (C.Phase == 5) C.ForceN.X = std::numeric_limits<double>::quiet_NaN();
    if (C.Phase == 6)
    {
        C.ReplayKind = LastPublishedPhase != 6 ? 1 : 2;
        C.Sequence = C.ReplayKind == 1 ? PublishIndex - 1 : 1;
    }
    LastPublishedPhase = C.Phase;
    Callback->GetProducerInputData_External()->Command = C;
    auto Json = MakeShared<FJsonObject>();
    CommandJson(Json, C);
    Json->SetNumberField(TEXT("publish_solver_s"), SolverNow);
    Commands.Add(MakeShared<FJsonValueObject>(Json));
}

void AImperialSolverBridgeVerifier::Tick(float Dt)
{
    Super::Tick(Dt);
    if (bFinished) return;
    if (!FMath::IsFinite(Dt) || Dt <= 0) { Finish(TEXT("Invalid game delta.")); return; }
    if (!bStarted)
    {
        Warmup += Dt;
        if (Warmup < .5) return;
        auto* Scene = GetWorld()->GetPhysicsScene();
        Solver = Scene ? Scene->GetSolver() : nullptr;
        if (!Solver || !Fixture->GetBodyInstance() || !Fixture->GetBodyInstance()->GetPhysicsActor())
        { Finish(TEXT("No solver or fixture proxy.")); return; }
        Solver->WaitOnPendingTasks_External();
        if (Solver->IsUsingFixedDt()) { Finish(TEXT("This fixture requires the existing synchronous variable-step solver.")); return; }
        bNormalDronesDisarmed &= DisarmedArena(GetWorld());
        if (!bNormalDronesDisarmed) { Finish(TEXT("Fixture requires ten disarmed non-autonomous aircraft.")); return; }
        StartTime = LastSolverEnd = Solver->GetSolverTime();
        IntegrationMinDt = Solver->GetMinDeltaTime_External();
        IntegrationMaxDt = Solver->GetMaxDeltaTime_External();
        Lifetime = MakeShared<FImperialBridgeLifetime, ESPMode::ThreadSafe>();
        Callback = Solver->CreateAndRegisterSimCallbackObject_External<FImperialSolverBridgeCallback>(Fixture->GetBodyInstance()->GetPhysicsActor(), Lifetime);
        bStarted = true;
        PublishCommand(StartTime);
        return;
    }
    Solver->WaitOnPendingTasks_External();
    bNormalDronesDisarmed &= DisarmedArena(GetWorld());
    if (bQuiescing)
    {
        QuiesceElapsed += Dt;
        CoastError = FMath::Max(CoastError, FVector::Distance(Fixture->GetPhysicsLinearVelocity() / 100., LastVelocity));
        CoastAngularError = FMath::Max(CoastAngularError, FVector::Distance(Fixture->GetPhysicsAngularVelocityInRadians(), LastAngularVelocity));
        if (Lifetime->Deleted.Load()) { Finish(); return; }
        if (QuiesceElapsed > .5) Finish(TEXT("Callback deletion acknowledgement timed out while fixture was still alive."));
        return;
    }
    GameElapsed += Dt;
    ++GameFrames;
    GameMinDt = FMath::Min(GameMinDt, static_cast<double>(Dt));
    GameMaxDt = FMath::Max(GameMaxDt, static_cast<double>(Dt));
    if (Solver->IsUsingFixedDt() || Solver->GetMinDeltaTime_External() != IntegrationMinDt || Solver->GetMaxDeltaTime_External() != IntegrationMaxDt)
    { Finish(TEXT("Solver mode or integration limits changed.")); return; }
    DrainOutputs();
    if (!Failure.IsEmpty()) { Finish(); return; }
    if (!bNormalDronesDisarmed) { Finish(TEXT("Normal aircraft became armed or autonomous.")); return; }
    if (StepCount > 4096 || GameElapsed > 6) { Finish(TEXT("Bounded bridge fixture exceeded its sample or time budget.")); return; }
    if (SolverElapsed + 1e-7 >= Duration)
    {
        ReleaseCallback();
        bQuiescing = true;
        return;
    }
    PublishCommand(Solver->GetSolverTime());
}

void AImperialSolverBridgeVerifier::DrainOutputs()
{
    if (!Callback) return;
    while (auto Output = Callback->PopFutureOutputData_External())
    {
        const auto& O = *Output;
        ++StepCount;
        ForceCalls += O.ForceCalls;
        TorqueCalls += O.TorqueCalls;
        OrderErrors += O.Errors;
        if (O.Sequence != StepCount || O.PreSimulates != 1 || O.PreIntegrates != 1 || O.PostIntegrates != 1 || O.Stage != 3) ++OrderErrors;
        const bool bDtValid = FMath::IsFinite(O.Dt) && O.Dt > IntegrationMinDt && (IntegrationMaxDt <= 0 || O.Dt <= IntegrationMaxDt + 1e-8);
        if (!bDtValid || FMath::Abs(O.Time - LastSolverEnd) > 1e-7) Failure = TEXT("Solver time, delta or single-integrate contract failed.");
        if (O.Command.Phase >= 0 && O.Command.Phase < 9) ++PhaseRows[O.Command.Phase];
        else Failure = TEXT("Invalid phase.");
        SolverElapsed += O.Dt;
        LastSolverEnd = O.Time + O.Dt;
        SolverMinDt = FMath::Min(SolverMinDt, O.Dt);
        SolverMaxDt = FMath::Max(SolverMaxDt, O.Dt);
        if (!Finite(O.PreV) || !Finite(O.PostV) || !Finite(O.PreW) || !Finite(O.PostW) || !Finite(O.Force) || !Finite(O.Torque) ||
            !Finite(O.Inertia) || !FMath::IsFinite(O.Mass) || FMath::Abs(O.Mass - MassKg) > 1e-6 || O.Inertia.GetMin() <= 0)
            Failure = TEXT("Invalid actual body state or configured mass.");
        if (O.Mass > 0 && O.Inertia.GetMin() > 0)
        {
            const FVector LinearExpected = O.Force * (O.Dt / O.Mass);
            const FVector LocalTorque = O.PreQ.UnrotateVector(O.Torque);
            const FVector AngularExpected = O.PreQ.RotateVector(LocalTorque / O.Inertia) * O.Dt;
            MaxLinearError = FMath::Max(MaxLinearError, (O.PostV - O.PreV - LinearExpected).Size());
            MaxAngularError = FMath::Max(MaxAngularError, (O.PostW - O.PreW - AngularExpected).Size());
        }
        LastVelocity = O.PostV;
        LastAngularVelocity = O.PostW;
        auto Row = MakeShared<FJsonObject>();
        CommandJson(Row, O.Command);
        Row->SetNumberField(TEXT("sequence"), O.Sequence);
        Row->SetNumberField(TEXT("sim_s"), O.Time);
        Row->SetNumberField(TEXT("dt_s"), O.Dt);
        Row->SetStringField(TEXT("decision"), O.Decision);
        Row->SetBoolField(TEXT("applied"), O.bApplied);
        Row->SetNumberField(TEXT("mass_kg"), O.Mass);
        Row->SetArrayField(TEXT("inertia_kgm2"), VectorJson(O.Inertia));
        Row->SetArrayField(TEXT("pre_v_mps"), VectorJson(O.PreV));
        Row->SetArrayField(TEXT("post_v_mps"), VectorJson(O.PostV));
        Row->SetArrayField(TEXT("pre_w_radps"), VectorJson(O.PreW));
        Row->SetArrayField(TEXT("post_w_radps"), VectorJson(O.PostW));
        Row->SetArrayField(TEXT("pre_origin_m"), VectorJson(O.PreOrigin));
        Row->SetArrayField(TEXT("pre_com_m"), VectorJson(O.PreCOM));
        Row->SetArrayField(TEXT("application_point_m"), VectorJson(O.Point));
        Row->SetArrayField(TEXT("pre_q_xyzw"), { MakeShared<FJsonValueNumber>(O.PreQ.X), MakeShared<FJsonValueNumber>(O.PreQ.Y), MakeShared<FJsonValueNumber>(O.PreQ.Z), MakeShared<FJsonValueNumber>(O.PreQ.W) });
        Row->SetArrayField(TEXT("force_n"), VectorJson(O.Force));
        Row->SetArrayField(TEXT("torque_nm"), VectorJson(O.Torque));
        Row->SetNumberField(TEXT("pre_simulates"), O.PreSimulates);
        Row->SetNumberField(TEXT("pre_integrates"), O.PreIntegrates);
        Row->SetNumberField(TEXT("post_integrates"), O.PostIntegrates);
        Row->SetNumberField(TEXT("force_calls"), O.ForceCalls);
        Row->SetNumberField(TEXT("torque_calls"), O.TorqueCalls);
        Samples.Add(MakeShared<FJsonValueObject>(Row));
    }
}

void AImperialSolverBridgeVerifier::ReleaseCallback()
{
    if (!Solver || !Callback) return;
    auto* Scene = GetWorld() ? GetWorld()->GetPhysicsScene() : nullptr;
    if (!Scene || Scene->GetSolver() != Solver) { Callback = nullptr; Solver = nullptr; return; }
    Solver->WaitOnPendingTasks_External();
    DrainOutputs();
    Lifetime->Shutdown.Store(true);
    Solver->UnregisterAndFreeSimCallbackObject_External(Callback);
    Callback = nullptr; // Solver owns deferred destruction; fixture remains alive until acknowledgement.
    bUnregistered = true;
}

void AImperialSolverBridgeVerifier::RestoreTiming()
{
    if (!bTimingApplied) return;
    FApp::SetFixedDeltaTime(PreviousGameDt);
    FApp::SetUseFixedTimeStep(bPreviousFixed);
    if (GEngine) GEngine->SetMaxFPS(PreviousMaxFPS);
    bTimingApplied = false;
}

void AImperialSolverBridgeVerifier::Finish(const FString& Error)
{
    if (bFinished) return;
    if (!Error.IsEmpty()) Failure = Error;
    ReleaseCallback();
    bFinished = true;
    RestoreTiming();
    bool bPhases = true;
    for (uint32 Count : PhaseRows) bPhases &= Count > 0;
    const bool bDeleted = Lifetime && Lifetime->Deleted.Load();
    const uint64 AfterShutdown = Lifetime ? Lifetime->CallsAfterShutdown.Load() : 0;
    const bool bOk = Failure.IsEmpty() && bStarted && bNormalDronesDisarmed && bPhases && bUnregistered && bDeleted && AfterShutdown == 0 &&
        OrderErrors == 0 && StepCount > 0 && ForceCalls > 0 && ForceCalls == TorqueCalls &&
        MaxLinearError < 2e-6 && MaxAngularError < 2e-5 && CoastError < 2e-6 && CoastAngularError < 2e-5 && FMath::Abs(SolverElapsed - GameElapsed) < 1e-5;
    if (!bOk && Failure.IsEmpty()) Failure = TEXT("Bridge analytical, phase coverage or lifecycle validation failed.");
    auto Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("ok"), bOk);
    Result->SetStringField(TEXT("error"), Failure);
    Result->SetObjectField(TEXT("build"), ImperialBuildInfo::Json());
    Result->SetStringField(TEXT("mode"), TEXT("sync"));
    Result->SetNumberField(TEXT("requested_fps"), RequestedFPS);
    Result->SetBoolField(TEXT("fixture_only"), true);
    Result->SetBoolField(TEXT("drone_force_path_changed"), false);
    Result->SetBoolField(TEXT("normal_drones_disarmed"), bNormalDronesDisarmed);
    Result->SetBoolField(TEXT("callback_unregistered_before_body_release"), bUnregistered);
    Result->SetBoolField(TEXT("callback_deleted_before_body_release"), bDeleted);
    Result->SetNumberField(TEXT("post_shutdown_callback_calls"), AfterShutdown);
    Result->SetNumberField(TEXT("coast_velocity_error_mps"), CoastError);
    Result->SetNumberField(TEXT("coast_angular_velocity_error_radps"), CoastAngularError);
    Result->SetNumberField(TEXT("bound_generation"), BoundGeneration);
    Result->SetNumberField(TEXT("ttl_s"), CommandTTL);
    Result->SetNumberField(TEXT("configured_mass_kg"), MassKg);
    Result->SetNumberField(TEXT("radius_m"), RadiusM);
    Result->SetArrayField(TEXT("command_force_n"), VectorJson(CommandForce));
    Result->SetArrayField(TEXT("command_offset_m"), VectorJson(CommandOffset));
    Result->SetNumberField(TEXT("phase_duration_s"), PhaseDuration);
    Result->SetNumberField(TEXT("duration_s"), Duration);
    Result->SetNumberField(TEXT("solver_start_s"), StartTime);
    Result->SetNumberField(TEXT("solver_end_s"), LastSolverEnd);
    Result->SetNumberField(TEXT("solver_elapsed_s"), SolverElapsed);
    Result->SetNumberField(TEXT("game_elapsed_s"), GameElapsed);
    Result->SetNumberField(TEXT("game_frames"), GameFrames);
    Result->SetNumberField(TEXT("game_min_dt_s"), bStarted ? GameMinDt : 0);
    Result->SetNumberField(TEXT("game_max_dt_s"), GameMaxDt);
    Result->SetNumberField(TEXT("solver_min_dt_s"), StepCount ? SolverMinDt : 0);
    Result->SetNumberField(TEXT("solver_max_dt_s"), SolverMaxDt);
    Result->SetNumberField(TEXT("integration_min_dt_s"), IntegrationMinDt);
    Result->SetNumberField(TEXT("integration_max_dt_s"), IntegrationMaxDt);
    Result->SetNumberField(TEXT("steps"), StepCount);
    Result->SetNumberField(TEXT("force_calls"), ForceCalls);
    Result->SetNumberField(TEXT("torque_calls"), TorqueCalls);
    Result->SetNumberField(TEXT("order_errors"), OrderErrors);
    Result->SetNumberField(TEXT("max_linear_increment_error_mps"), MaxLinearError);
    Result->SetNumberField(TEXT("max_angular_increment_error_radps"), MaxAngularError);
    Result->SetArrayField(TEXT("commands"), Commands);
    Result->SetArrayField(TEXT("samples"), Samples);
    Result->SetStringField(TEXT("scope"), TEXT("Isolated collision-free sphere, synchronous solver-step input snapshots and internal COM force/torque adapter. Nine software command phases; each completed integration checked analytically. No drone PID migration, contact test, fixed120 flight, real USB latency, body reuse or cross-PC claim."));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ResultFile), true);
    FString Json;
    const bool bSaved = FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Json)) && FFileHelper::SaveStringToFile(Json, *ResultFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("IMPERIAL_SOLVER_BRIDGE ok=%d steps=%llu result=%s"), bOk && bSaved, StepCount, *ResultFile);
    SetActorTickEnabled(false);
    FPlatformMisc::RequestExitWithStatus(false, bOk && bSaved ? 0 : 1);
}

void AImperialSolverBridgeVerifier::EndPlay(const EEndPlayReason::Type Reason)
{
    ReleaseCallback();
    RestoreTiming();
    Super::EndPlay(Reason);
}
