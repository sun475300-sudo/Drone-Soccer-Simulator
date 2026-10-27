#include "ImperialSolverProbe.h"
#include "ImperialBuildInfo.h"
#include "ImperialMatchManager.h"
#include "SoccerDronePawn.h"
#include "Chaos/SimCallbackObject.h"
#include "PBDRigidsSolver.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
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
#include "HAL/PlatformTime.h"
#include "HAL/PlatformTLS.h"

namespace
{
constexpr int32 MaximumSamples = 128;
constexpr uint64 MaximumOutputs = 16384;
constexpr double PassthroughTimeoutSeconds = 45;

bool IsDisarmedArena(UWorld* World)
{
    const auto* Manager = Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(World, AImperialMatchManager::StaticClass()));
    if (!Manager || Manager->GetDrones().Num() != 10) return false;
    for (const auto& Drone : Manager->GetDrones())
        if (!Drone || Drone->IsArmed() || Drone->IsAutonomousEnabled()) return false;
    return true;
}

struct FImperialSolverProbeOutput : Chaos::FSimCallbackOutput
{
    uint64 Sequence = 0;
    double Dt = 0;
    uint32 PreSimulates = 0;
    uint32 PreIntegrates = 0;
    uint32 PostIntegrates = 0;
    uint32 Errors = 0;
    uint32 Threads[3] = {};
    uint8 Events[8] = {};
    uint8 EventCount = 0;
    uint8 Stage = 0;
    bool bEventOverflow = false;
    void Reset() { *this = FImperialSolverProbeOutput(); }
    void Event(uint8 Phase)
    {
        if (EventCount < UE_ARRAY_COUNT(Events)) Events[EventCount++] = Phase;
        else bEventOverflow = true;
    }
};
}

// POD-only instrumentation: no actor/component/proxy reads, forces, PID or I/O.
class FImperialSolverProbeCallback final : public Chaos::TSimCallbackObject<
    Chaos::FSimCallbackNoInput, FImperialSolverProbeOutput,
    Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::PreIntegrate | Chaos::ESimCallbackOptions::PostIntegrate>
{
public:
    virtual FName GetFNameForStatId() const override
    {
        static const FName Name(TEXT("ImperialSolverProbe"));
        return Name;
    }
private:
    uint64 Sequence = 0;
    virtual void OnPreSimulate_Internal() override
    {
        auto& Out = GetProducerOutputData_Internal();
        if (Out.Stage != 0) ++Out.Errors;
        Out.Sequence = ++Sequence;
        Out.Dt = static_cast<double>(GetDeltaTime_Internal());
        ++Out.PreSimulates;
        Out.Threads[0] = FPlatformTLS::GetCurrentThreadId();
        Out.Stage = 1;
        Out.Event(1);
    }
    virtual void OnPreIntegrate_Internal() override
    {
        auto& Out = GetProducerOutputData_Internal();
        if (Out.Stage != 1 && Out.Stage != 3) ++Out.Errors;
        ++Out.PreIntegrates;
        Out.Threads[1] = FPlatformTLS::GetCurrentThreadId();
        Out.Stage = 2;
        Out.Event(2);
    }
    virtual void OnPostIntegrate_Internal() override
    {
        auto& Out = GetProducerOutputData_Internal();
        if (Out.Stage != 2) ++Out.Errors;
        ++Out.PostIntegrates;
        Out.Threads[2] = FPlatformTLS::GetCurrentThreadId();
        Out.Stage = 3;
        Out.Event(3);
    }
};

AImperialSolverProbe::AImperialSolverProbe()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
}

void AImperialSolverProbe::BeginPlay()
{
    Super::BeginPlay();
    if (!FParse::Param(FCommandLine::Get(), TEXT("ImperialSolverProbe")))
    {
        bFinished = true;
        SetActorTickEnabled(false);
        return;
    }
    bFixedRequested = FParse::Param(FCommandLine::Get(), TEXT("ImperialSolverProbeFixed"));
    bPassthrough = FParse::Param(FCommandLine::Get(), TEXT("ImperialSolverProbePassthrough"));
    FParse::Value(FCommandLine::Get(), TEXT("ImperialSolverProbeResult="), ResultFile);
    FParse::Value(FCommandLine::Get(), TEXT("ImperialSolverProbeSeconds="), Duration);
    FParse::Value(FCommandLine::Get(), TEXT("FPS="), RequestedFPS);
    GameThreadId = FPlatformTLS::GetCurrentThreadId();
    WallStart = FPlatformTime::Seconds();
    if (ResultFile.IsEmpty()) ResultFile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Verification/solver-probe.json"));
    if ((RequestedFPS != 20 && RequestedFPS != 30 && RequestedFPS != 45 && RequestedFPS != 60 && RequestedFPS != 120 && RequestedFPS != 240) ||
        !FMath::IsFinite(Duration) || Duration < 5 || Duration > 10)
    {
        Finish(TEXT("Use FPS 20/30/45/60/120/240 and a finite probe duration between 5 and 10 seconds."));
        return;
    }
    if (bFixedRequested && bPassthrough)
    {
        Finish(TEXT("Passthrough is allowed only for the unchanged synchronous solver mode."));
        return;
    }
    if (bPassthrough && !FParse::Param(FCommandLine::Get(), TEXT("ImperialPhysicsSmoke")))
    {
        Finish(TEXT("Passthrough requires ImperialPhysicsSmoke and its explicit completion signal."));
        return;
    }
    bPreviousFixedGameTime = FApp::UseFixedTimeStep();
    PreviousFixedGameDt = FApp::GetFixedDeltaTime();
    PreviousMaxFPS = GEngine ? GEngine->GetMaxFPS() : 0;
    FApp::SetFixedDeltaTime(1.0 / RequestedFPS);
    FApp::SetUseFixedTimeStep(true);
    if (GEngine) GEngine->SetMaxFPS(RequestedFPS);
    bTimingApplied = true;
}

void AImperialSolverProbe::Tick(float Dt)
{
    Super::Tick(Dt);
    if (bFinished) return;
    if (!FMath::IsFinite(Dt) || Dt <= 0) { Finish(TEXT("Game Tick dt was not finite and strictly positive.")); return; }
    if (!bStarted)
    {
        Warmup += Dt;
        if (Warmup < .5) return;
        auto* Scene = GetWorld()->GetPhysicsScene();
        Solver = Scene ? Scene->GetSolver() : nullptr;
        if (!Solver) { Finish(TEXT("No Chaos rigid solver is available.")); return; }
        Solver->WaitOnPendingTasks_External();
        PreviousAsyncDt = Solver->GetAsyncDeltaTime();
        if (bFixedRequested)
        {
            if (!IsDisarmedArena(GetWorld())) { Finish(TEXT("Fixed counter experiment requires ten disarmed, non-autonomous aircraft before mode selection.")); return; }
            Solver->EnableAsyncMode(Chaos::FReal(1.0 / 120.0));
            bChangedAsyncDt = true;
        }
        bActualFixedDt = Solver->IsUsingFixedDt();
        if (bActualFixedDt != bFixedRequested)
        {
            Finish(TEXT("The solver fixed-dt flag does not match the explicitly requested mode; global gates were not overridden."));
            return;
        }
        SolverStart = Solver->GetSolverTime();
        SolverInitialRemainder = Solver->GetAccumulatedTime();
        IntegrationMinDt = Solver->GetMinDeltaTime_External();
        IntegrationMaxDt = Solver->GetMaxDeltaTime_External();
        Callback = Solver->CreateAndRegisterSimCallbackObject_External<FImperialSolverProbeCallback>();
        GameStart = GameEnd = GetWorld()->GetTimeSeconds();
        bStarted = true;
        WallStart = FPlatformTime::Seconds();
        return;
    }
    if (bFixedRequested && !IsDisarmedArena(GetWorld()))
    {
        Finish(TEXT("Fixed counter experiment cannot run with armed or autonomous aircraft."));
        return;
    }
    ++GameFrames;
    GameEnd = GetWorld()->GetTimeSeconds();
    GameElapsed = GameEnd - GameStart;
    GameMinDt = FMath::Min(GameMinDt, static_cast<double>(Dt));
    GameMaxDt = FMath::Max(GameMaxDt, static_cast<double>(Dt));
    bFixedFlagStable &= Solver->IsUsingFixedDt() == bActualFixedDt;
    bIntegrationSettingsStable &= Solver->GetMinDeltaTime_External() == IntegrationMinDt && Solver->GetMaxDeltaTime_External() == IntegrationMaxDt;
    DrainOutputs();
    if (SolverSteps > MaximumOutputs) { Finish(TEXT("Probe output safety bound exceeded.")); return; }
    if (bPassthrough)
    {
        if (bCompletionRequested) { bCompletedBySmoke = true; Finish(); }
        else if (GameElapsed >= PassthroughTimeoutSeconds) Finish(TEXT("Passthrough timed out before the PhysicsSmoke completion signal."));
    }
    else if (GameElapsed >= Duration) Finish();
}

void AImperialSolverProbe::CompletePassthrough(double SmokeElapsedSeconds, TFunction<void(bool)> OnComplete)
{
    // PhysicsSmoke freezes its final metrics in PrePhysics. Completion is
    // delivered only after this same frame's physics and our PostPhysics tick.
    if (!bPassthrough || bFinished)
    {
        if (OnComplete) OnComplete(bPassthrough && bLastResultOk);
        return;
    }
    CompletionSignalWorld = GetWorld()->GetTimeSeconds();
    SmokeCompletionElapsed = SmokeElapsedSeconds;
    PassthroughCompletion = MoveTemp(OnComplete);
    bCompletionRequested = true;
}

void AImperialSolverProbe::DrainOutputs()
{
    if (!Callback) return;
    // These are finalized/completed outputs, not unfinished producer data. The
    // future pop deliberately bypasses presentation interpolation delay.
    uint64 Drained = 0;
    while (auto Output = Callback->PopFutureOutputData_External())
    {
        const auto& Out = *Output;
        ++SolverSteps;
        PreSimulates += Out.PreSimulates;
        PreIntegrations += Out.PreIntegrates;
        PostIntegrations += Out.PostIntegrates;
        OrderErrors += Out.Errors;
        if (Out.Sequence != LastSequence + 1 || Out.PreSimulates != 1 || Out.Stage != 3 || Out.bEventOverflow) ++OrderErrors;
        LastSequence = Out.Sequence;
        if (Out.PreIntegrates != 1 || Out.PostIntegrates != 1) ++NonSingleIntegrationSteps;
        if (!FMath::IsFinite(Out.Dt) || Out.Dt <= 0) ++InvalidDtSteps;
        else
        {
            SolverElapsed += Out.Dt;
            SolverMinDt = FMath::Min(SolverMinDt, Out.Dt);
            SolverMaxDt = FMath::Max(SolverMaxDt, Out.Dt);
            // Hook dt is the outer solver dt. A single Integrate can still be
            // truncated by the evolution min/max limits, so check those too.
            if (!FMath::IsFinite(IntegrationMinDt) || !FMath::IsFinite(IntegrationMaxDt) ||
                Out.Dt <= IntegrationMinDt || (IntegrationMaxDt > 0 && Out.Dt > IntegrationMaxDt + 1e-8)) ++IntegrationDtViolations;
        }
        for (uint32 Id : Out.Threads)
        {
            if (Id != 0 && CallbackThreadIds.Num() < 32) CallbackThreadIds.Add(Id);
            else if (Id == 0 || !CallbackThreadIds.Contains(Id)) ++OrderErrors;
        }
        if (Samples.Num() < MaximumSamples)
        {
            auto Row = MakeShared<FJsonObject>();
            Row->SetNumberField(TEXT("sequence"), static_cast<double>(Out.Sequence));
            Row->SetNumberField(TEXT("sim_start_s"), Out.InternalTime);
            Row->SetNumberField(TEXT("solver_dt_s"), Out.Dt);
            Row->SetNumberField(TEXT("pre_integrations"), Out.PreIntegrates);
            Row->SetNumberField(TEXT("post_integrations"), Out.PostIntegrates);
            TArray<TSharedPtr<FJsonValue>> Events;
            for (uint8 I = 0; I < Out.EventCount; ++I)
                Events.Add(MakeShared<FJsonValueString>(Out.Events[I] == 1 ? TEXT("pre_simulate") : Out.Events[I] == 2 ? TEXT("pre_integrate") : TEXT("post_integrate")));
            Row->SetArrayField(TEXT("order"), Events);
            Row->SetNumberField(TEXT("pre_simulate_thread_id"), Out.Threads[0]);
            Row->SetNumberField(TEXT("pre_integrate_thread_id"), Out.Threads[1]);
            Row->SetNumberField(TEXT("post_integrate_thread_id"), Out.Threads[2]);
            Samples.Add(MakeShared<FJsonValueObject>(Row));
        }
        if (++Drained >= MaximumOutputs) { Failure = TEXT("One drain exceeded its bounded output budget."); break; }
    }
}

void AImperialSolverProbe::ReleaseCallback()
{
    if (!Solver) return;
    auto* Scene = GetWorld() ? GetWorld()->GetPhysicsScene() : nullptr;
    if (!Scene || Scene->GetSolver() != Solver) { Callback = nullptr; Solver = nullptr; return; }
    Solver->WaitOnPendingTasks_External();
    if (!bFinished) DrainOutputs(); // Finish has already frozen final counters.
    if (Callback)
    {
        Solver->UnregisterAndFreeSimCallbackObject_External(Callback);
        Callback = nullptr; // The solver owns deferred destruction; never delete.
    }
    if (bChangedAsyncDt)
    {
        Solver->SetAsyncDeltaTime(PreviousAsyncDt);
        bChangedAsyncDt = false;
    }
    // No zero-dt advance: that would flush a fractional fixed-step remainder.
}

void AImperialSolverProbe::RestoreTiming()
{
    if (!bTimingApplied) return;
    FApp::SetFixedDeltaTime(PreviousFixedGameDt);
    FApp::SetUseFixedTimeStep(bPreviousFixedGameTime);
    if (GEngine) GEngine->SetMaxFPS(PreviousMaxFPS);
    bTimingApplied = false;
}

void AImperialSolverProbe::Finish(const FString& Error)
{
    if (bFinished) return;
    bFinished = true;
    if (!Error.IsEmpty()) Failure = Error;
    double Remainder = 0;
    double AsyncDt = -1;
    if (Solver)
    {
        Solver->WaitOnPendingTasks_External();
        DrainOutputs();
        SolverEnd = Solver->GetSolverTime();
        Remainder = Solver->GetAccumulatedTime();
        AsyncDt = Solver->GetAsyncDeltaTime();
        bFixedFlagStable &= Solver->IsUsingFixedDt() == bActualFixedDt;
        bIntegrationSettingsStable &= Solver->GetMinDeltaTime_External() == IntegrationMinDt && Solver->GetMaxDeltaTime_External() == IntegrationMaxDt;
    }
    const bool bPositive = SolverSteps > 0 && InvalidDtSteps == 0;
    const bool bSingle = NonSingleIntegrationSteps == 0 && SolverSteps == PreIntegrations && SolverSteps == PostIntegrations;
    const bool bGameDt = GameFrames > 0 && FMath::Abs(GameMinDt - 1.0 / RequestedFPS) <= 1e-5 && FMath::Abs(GameMaxDt - 1.0 / RequestedFPS) <= 1e-5;
    const bool bFixedMeasured = !bFixedRequested || (bActualFixedDt && FMath::Abs(SolverMinDt - 1.0 / 120.0) <= 1e-7 && FMath::Abs(SolverMaxDt - 1.0 / 120.0) <= 1e-7);
    const bool bClock = bStarted && FMath::Abs(SolverElapsed - (SolverEnd - SolverStart)) < 1e-5;
    const double TimeBalanceError = GameElapsed - SolverElapsed - (Remainder - SolverInitialRemainder);
    const bool bBalance = bStarted && FMath::IsFinite(TimeBalanceError) && FMath::Abs(TimeBalanceError) < 1e-5;
    const bool bIntegrationCompatible = bPositive && IntegrationDtViolations == 0 && bIntegrationSettingsStable;
    const bool bCoverage = !bPassthrough || (bCompletedBySmoke && SmokeCompletionElapsed >= 43 && GameStart < 1 &&
        GameElapsed >= 42 && GameEnd + 1e-9 >= CompletionSignalWorld);
    if (Failure.IsEmpty())
    {
        if (!bPositive) Failure = TEXT("Solver callback samples were absent, nonfinite or not strictly positive.");
        else if (!bSingle) Failure = TEXT("A solver advance did not have exactly one pre/post integration pair.");
        else if (!bGameDt) Failure = TEXT("Measured game Tick dt did not match the requested FPS.");
        else if (!bFixedMeasured || !bFixedFlagStable) Failure = TEXT("Actual fixed-dt mode or measured dt did not match the requested mode.");
        else if (!bIntegrationCompatible) Failure = TEXT("Callback dt is incompatible with evolution integration limits or those limits changed.");
        else if (!bClock) Failure = TEXT("Completed callback time did not match the actual solver clock delta.");
        else if (!bBalance) Failure = TEXT("Game time does not equal completed solver time plus the change in fixed-step remainder.");
        else if (OrderErrors != 0) Failure = TEXT("Solver callback order, sequence or thread observations were incomplete.");
        else if (!bCoverage) Failure = TEXT("Passthrough did not cover PhysicsSmoke through its completion frame.");
    }
    const bool bOk = Failure.IsEmpty();
    ReleaseCallback();
    RestoreTiming();
    auto Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("ok"), bOk);
    Result->SetStringField(TEXT("mode"), bFixedRequested ? TEXT("fixed120") : TEXT("sync"));
    Result->SetStringField(TEXT("error"), Failure);
    Result->SetNumberField(TEXT("requested_fps"), RequestedFPS);
    Result->SetNumberField(TEXT("requested_duration_s"), FMath::IsFinite(Duration) ? Duration : 0);
    Result->SetNumberField(TEXT("actual_min_game_dt_s"), GameFrames ? GameMinDt : 0);
    Result->SetNumberField(TEXT("actual_max_game_dt_s"), GameMaxDt);
    Result->SetBoolField(TEXT("solver_fixed_dt_enabled"), bActualFixedDt);
    Result->SetBoolField(TEXT("solver_fixed_flag_stable"), bFixedFlagStable);
    Result->SetNumberField(TEXT("solver_steps"), static_cast<double>(SolverSteps));
    Result->SetNumberField(TEXT("pre_simulates"), static_cast<double>(PreSimulates));
    Result->SetNumberField(TEXT("pre_integrations"), static_cast<double>(PreIntegrations));
    Result->SetNumberField(TEXT("post_integrations"), static_cast<double>(PostIntegrations));
    Result->SetNumberField(TEXT("solver_min_dt_s"), bPositive ? SolverMinDt : 0);
    Result->SetNumberField(TEXT("solver_max_dt_s"), SolverMaxDt);
    Result->SetNumberField(TEXT("solver_elapsed_s"), SolverElapsed);
    Result->SetNumberField(TEXT("solver_time_elapsed_s"), SolverEnd - SolverStart);
    Result->SetNumberField(TEXT("game_start_s"), GameStart);
    Result->SetNumberField(TEXT("game_end_s"), GameEnd);
    Result->SetNumberField(TEXT("solver_start_s"), SolverStart);
    Result->SetNumberField(TEXT("solver_end_s"), SolverEnd);
    Result->SetNumberField(TEXT("game_elapsed_s"), GameElapsed);
    Result->SetNumberField(TEXT("game_frames"), static_cast<double>(GameFrames));
    Result->SetNumberField(TEXT("wall_elapsed_s"), FPlatformTime::Seconds() - WallStart);
    Result->SetNumberField(TEXT("solver_accumulated_remainder_s"), Remainder);
    Result->SetNumberField(TEXT("solver_initial_remainder_s"), SolverInitialRemainder);
    Result->SetNumberField(TEXT("time_balance_error_s"), TimeBalanceError);
    Result->SetBoolField(TEXT("time_balance_valid"), bBalance);
    Result->SetNumberField(TEXT("solver_integration_min_dt_s"), IntegrationMinDt);
    Result->SetNumberField(TEXT("solver_integration_max_dt_s"), IntegrationMaxDt);
    Result->SetBoolField(TEXT("integration_dt_compatible"), bIntegrationCompatible);
    Result->SetNumberField(TEXT("integration_dt_violations"), static_cast<double>(IntegrationDtViolations));
    Result->SetBoolField(TEXT("solver_integration_settings_stable"), bIntegrationSettingsStable);
    Result->SetNumberField(TEXT("solver_async_dt_s"), AsyncDt);
    Result->SetNumberField(TEXT("force_disable_async_physics"), Chaos::ForceDisableAsyncPhysics);
    Result->SetNumberField(TEXT("use_async_interpolation"), Chaos::UseAsyncInterpolation);
    Result->SetNumberField(TEXT("callback_order_errors"), static_cast<double>(OrderErrors));
    Result->SetNumberField(TEXT("invalid_dt_steps"), static_cast<double>(InvalidDtSteps));
    Result->SetNumberField(TEXT("non_single_integration_steps"), static_cast<double>(NonSingleIntegrationSteps));
    Result->SetBoolField(TEXT("samples_strictly_positive"), bPositive);
    Result->SetBoolField(TEXT("one_integration_per_solver_step"), bSingle);
    Result->SetBoolField(TEXT("solver_clock_matches_completed_outputs"), bClock);
    Result->SetBoolField(TEXT("force_path_changed"), false);
    Result->SetBoolField(TEXT("rigid_state_read"), false);
    Result->SetBoolField(TEXT("passthrough"), bPassthrough);
    Result->SetBoolField(TEXT("passthrough_completed_by_smoke"), bCompletedBySmoke);
    Result->SetBoolField(TEXT("observer_covers_completion_tick"), bCoverage);
    Result->SetNumberField(TEXT("observer_begin_world_s"), GameStart);
    Result->SetNumberField(TEXT("observer_end_world_s"), GameEnd);
    Result->SetNumberField(TEXT("completion_signal_world_s"), CompletionSignalWorld);
    Result->SetNumberField(TEXT("smoke_completion_elapsed_s"), SmokeCompletionElapsed);
    Result->SetNumberField(TEXT("passthrough_timeout_s"), PassthroughTimeoutSeconds);
    Result->SetNumberField(TEXT("game_thread_id"), GameThreadId);
    TArray<TSharedPtr<FJsonValue>> Threads;
    for (uint32 Id : CallbackThreadIds) Threads.Add(MakeShared<FJsonValueNumber>(Id));
    Result->SetArrayField(TEXT("callback_thread_ids"), Threads);
    Result->SetArrayField(TEXT("samples"), Samples);
    Result->SetNumberField(TEXT("samples_limit"), MaximumSamples);
    Result->SetObjectField(TEXT("build"), ImperialBuildInfo::Json());
    Result->SetStringField(TEXT("scope"), TEXT("Read-only finalized Chaos callback counts and order; callback dt is solver-advance dt, not an independently exposed integration dt. PostIntegrate synchronizes integration workers. No forces/PID/proxy reads or 120Hz flight claim; fixed mode is disarmed counter instrumentation only."));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ResultFile), true);
    FString Json;
    const bool bSerialized = FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Json));
    const bool bSaved = bSerialized && FFileHelper::SaveStringToFile(Json, *ResultFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    bLastResultOk = bOk && bSaved;
    UE_LOG(LogTemp, Display, TEXT("IMPERIAL_SOLVER_PROBE ok=%d steps=%llu pre=%llu post=%llu result=%s"), bOk && bSaved, SolverSteps, PreIntegrations, PostIntegrations, *ResultFile);
    SetActorTickEnabled(false);
    if (bPassthrough && PassthroughCompletion)
    {
        auto Complete = MoveTemp(PassthroughCompletion);
        Complete(bLastResultOk);
    }
    else if (bPassthrough && !bLastResultOk)
    {
        // Invalid startup or a missing completion signal has no owning smoke
        // callback to terminate the process. Fail boundedly rather than hang.
        FPlatformMisc::RequestExitWithStatus(false, 1);
    }
    if (!bPassthrough) FPlatformMisc::RequestExitWithStatus(false, bLastResultOk ? 0 : 1);
}

void AImperialSolverProbe::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ReleaseCallback();
    RestoreTiming();
    Super::EndPlay(EndPlayReason);
}
