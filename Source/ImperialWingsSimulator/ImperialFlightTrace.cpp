#include "ImperialFlightTrace.h"
#include "ImperialBuildInfo.h"
#include "ImperialMatchManager.h"
#include "ImperialSolverProbe.h"
#include "ImperialUserData.h"
#include "SoccerDronePawn.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PBDRigidsSolver.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/Archive.h"
#include "Containers/StringConv.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"

namespace
{
constexpr uint64 MaximumRows = 110000;
constexpr double TimeoutSeconds = 45;
constexpr double ChargeToleranceAh = 1e-9;
const TCHAR* CsvHeader = TEXT("frame,world_s,game_dt_s,id,flight_model_s,physics_step_seq,sensor_model_s,reset_epoch,x_m,y_m,z_m,vx_mps,vy_mps,vz_mps,qx,qy,qz,qw,wx_radps,wy_radps,wz_radps,armed,autonomous,raw_throttle,throttle_command,current_a,consumed_ah,battery_soc,battery_voltage_v,delivered_power_w,motor_applied_power_w,motor_available_power_w\n");
FString Number(double Value) { return FString::Printf(TEXT("%.17g"), Value); }
FString Integer(uint64 Value) { return FString::Printf(TEXT("%llu"), Value); }
}

AImperialFlightTrace::AImperialFlightTrace()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
}

AImperialFlightTrace::~AImperialFlightTrace() { CloseWriter(); }

bool AImperialFlightTrace::WriteText(const FString& Text)
{
    if (!Writer) return false;
    const FTCHARToUTF8 Bytes(*Text);
    Writer->Serialize(const_cast<ANSICHAR*>(Bytes.Get()), Bytes.Length());
    return !Writer->IsError();
}

bool AImperialFlightTrace::CloseWriter()
{
    if (!Writer) return true;
    Writer->Flush();
    const bool bHealthy = !Writer->IsError();
    const bool bClosed = Writer->Close();
    delete Writer;
    Writer = nullptr;
    return bHealthy && bClosed;
}

void AImperialFlightTrace::RestoreTiming()
{
    if (!bTimingApplied) return;
    FApp::SetFixedDeltaTime(PreviousFixedGameDt);
    FApp::SetUseFixedTimeStep(bPreviousFixedGameTime);
    if (GEngine) GEngine->SetMaxFPS(PreviousMaxFPS);
    bTimingApplied = false;
}

void AImperialFlightTrace::BeginPlay()
{
    Super::BeginPlay();
    if (!FParse::Param(FCommandLine::Get(), TEXT("ImperialFlightTrace")))
    {
        bFinished = true;
        SetActorTickEnabled(false);
        return;
    }
    const bool bHasResult = FParse::Value(FCommandLine::Get(), TEXT("ImperialFlightTraceResult="), ResultFile);
    FParse::Value(FCommandLine::Get(), TEXT("ImperialFlightTraceCsv="), CsvFile);
    FParse::Value(FCommandLine::Get(), TEXT("FPS="), RequestedFPS);
    if (ResultFile.IsEmpty()) ResultFile = ImperialUserData::Path(TEXT("Diagnostics/flight-trace.json"));
    if (!FParse::Param(FCommandLine::Get(), TEXT("ImperialPhysicsSmoke")))
    {
        Finish(TEXT("ImperialFlightTrace requires the explicit ImperialPhysicsSmoke fixture."));
        return;
    }
    if (!bHasResult || CsvFile.IsEmpty()) { Finish(TEXT("Both ImperialFlightTraceCsv and ImperialFlightTraceResult are required.")); return; }
    if (RequestedFPS != 20 && RequestedFPS != 30 && RequestedFPS != 45 && RequestedFPS != 60 && RequestedFPS != 120 && RequestedFPS != 240)
    {
        Finish(TEXT("Flight trace requires FPS 20/30/45/60/120/240."));
        return;
    }
    bObserved = FParse::Param(FCommandLine::Get(), TEXT("ImperialSolverProbe"));
    if (FParse::Param(FCommandLine::Get(), TEXT("ImperialSolverProbeFixed")) ||
        (bObserved && !FParse::Param(FCommandLine::Get(), TEXT("ImperialSolverProbePassthrough"))))
    {
        Finish(TEXT("Full-flight traces allow only the unchanged synchronous probe passthrough."));
        return;
    }
    bPreviousFixedGameTime = FApp::UseFixedTimeStep();
    PreviousFixedGameDt = FApp::GetFixedDeltaTime();
    PreviousMaxFPS = GEngine ? GEngine->GetMaxFPS() : 0;
    FApp::SetFixedDeltaTime(1.0 / RequestedFPS);
    FApp::SetUseFixedTimeStep(true);
    if (GEngine) GEngine->SetMaxFPS(RequestedFPS);
    bTimingApplied = true;
    CsvFile = FPaths::ConvertRelativePathToFull(CsvFile);
    ResultFile = FPaths::ConvertRelativePathToFull(ResultFile);
    if (CsvFile.Equals(ResultFile, ESearchCase::IgnoreCase)) { Finish(TEXT("CSV and JSON output paths must differ.")); return; }
    // UE 5.8's generic writer ignores FILEWRITE_NoReplaceExisting. Check the
    // destination explicitly; our runner also allocates a fresh unique folder.
    if (IFileManager::Get().FileExists(*CsvFile) || IFileManager::Get().DirectoryExists(*CsvFile))
    {
        Finish(TEXT("Flight trace CSV already exists; choose a new output file."));
        return;
    }
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(CsvFile), true);
    Writer = IFileManager::Get().CreateFileWriter(*CsvFile, FILEWRITE_NoReplaceExisting);
    if (!Writer || !WriteText(CsvHeader)) { Finish(TEXT("Cannot create the new flight trace CSV or write its header.")); return; }
    if (bObserved)
    {
        auto* Probe = Cast<AImperialSolverProbe>(UGameplayStatics::GetActorOfClass(GetWorld(), AImperialSolverProbe::StaticClass()));
        if (!Probe) { Finish(TEXT("Requested solver observer actor is absent.")); return; }
        // Trace closes its final row before the observer consumes its completion
        // signal in this same PostPhysics group.
        Probe->AddTickPrerequisiteActor(this);
    }
}

void AImperialFlightTrace::RequestFinish(double SmokeElapsedSeconds, bool SmokeValid)
{
    if (bFinished) return;
    SmokeCompletionElapsed = SmokeElapsedSeconds;
    CompletionSignalWorld = GetWorld()->GetTimeSeconds();
    bSmokeValid = SmokeValid;
    bFinishRequested = true;
}

bool AImperialFlightTrace::SampleFrame(float Dt)
{
    auto* Manager = Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AImperialMatchManager::StaticClass()));
    if (!Manager || Manager->GetDrones().Num() != 10) { Failure = TEXT("Flight trace requires exactly ten stable aircraft."); return false; }
    if (Rows + 10 > MaximumRows) { Failure = TEXT("Flight trace row safety bound exceeded."); return false; }
    const double WorldSeconds = GetWorld()->GetTimeSeconds();
    if (!FMath::IsFinite(WorldSeconds) || !FMath::IsFinite(Dt) || Dt <= 0) { Failure = TEXT("Trace game clock is not finite and strictly advancing."); return false; }
    if (Frames == 0) WindowStart = WorldSeconds;
    else
    {
        if (WorldSeconds <= WindowEnd) { Failure = TEXT("Trace world clock failed to advance."); return false; }
        if (FMath::Abs(WorldSeconds - WindowEnd - Dt) > 1e-5) { Failure = TEXT("A game frame was omitted or duplicated in the flight trace window."); return false; }
        MinimumGameDt = FMath::Min(MinimumGameDt, static_cast<double>(Dt));
        MaximumGameDt = FMath::Max(MaximumGameDt, static_cast<double>(Dt));
        if (FMath::Abs(static_cast<double>(Dt) - 1.0 / RequestedFPS) > 1e-5) { Failure = TEXT("Actual game frame dt differs from the requested trace FPS."); return false; }
    }
    FString FrameText;
    FrameText.Reserve(8192);
    for (int32 Id = 0; Id < 10; ++Id)
    {
        auto* Drone = Manager->GetDrone(Id);
        if (!Drone || !Drone->GetBodyMesh() || !Drone->GetBodyMesh()->IsSimulatingPhysics()) { Failure = TEXT("A trace aircraft/body is absent or no longer simulating."); return false; }
        auto* Body = Drone->GetBodyMesh();
        const FVector Position = Body->GetComponentLocation() / 100.0;
        const FVector Velocity = Body->GetPhysicsLinearVelocity() / 100.0;
        const FQuat Rotation = Body->GetComponentQuat();
        const FVector Angular = Body->GetPhysicsAngularVelocityInRadians();
        const double Flight = Drone->GetFlightModelSeconds();
        const double Sensor = Drone->GetSensorModelSeconds();
        const uint64 Epoch = Drone->GetBatteryResetEpoch();
        const uint64 Step = Drone->GetPhysicsStepSequence();
        const double Current = Drone->GetBatteryCurrentA();
        const double Consumed = Drone->GetBatteryConsumedAh();
        const double Soc = Drone->GetBatterySoc();
        const double Voltage = Drone->GetBatteryVoltageV();
        const double Delivered = Drone->GetBatteryDeliveredPowerW();
        const double Motor = Drone->GetMotorElectricalPowerW();
        const double Available = Drone->GetAvailableMotorPowerW();
        const double Capacity = Drone->GetBatteryCapacityAh();
        const double Raw = Drone->GetThrottle();
        const double Command = Drone->GetThrottleCommand();
        const double Values[] = {Flight,Sensor,Position.X,Position.Y,Position.Z,Velocity.X,Velocity.Y,Velocity.Z,
            Rotation.X,Rotation.Y,Rotation.Z,Rotation.W,Angular.X,Angular.Y,Angular.Z,Raw,Command,Current,Consumed,Soc,Voltage,Delivered,Motor,Available,Capacity};
        for (double Value : Values) if (!FMath::IsFinite(Value)) { bFiniteBodies = false; Failure = TEXT("A flight trace body or electrical value is nonfinite."); return false; }
        if (Capacity <= 0 || Current < 0 || Consumed < 0 || Soc < 0 || Soc > 1) { Failure = TEXT("Battery trace values violate their physical parameter bounds."); return false; }
        auto& Integral = Bodies[Id];
        if (!Integral.bInitialized)
        {
            Integral.Drone = Drone;
            Integral.FirstEpoch = Epoch;
            Integral.CapacityAh = Capacity;
            Integral.bBatteryEnabled = Drone->IsBatteryModelEnabled();
            Integral.bInitialized = true;
        }
        else
        {
            if (Integral.Drone.Get() != Drone || Step != Integral.PhysicsStep + 1) { Failure = TEXT("Aircraft identity changed or a physics update was omitted/duplicated between samples."); return false; }
            if (Epoch < Integral.Epoch || Epoch > Integral.Epoch + 1) { Failure = TEXT("Battery reset epoch regressed or multiple resets were hidden between frames."); return false; }
            if (Capacity != Integral.CapacityAh || Drone->IsBatteryModelEnabled() != Integral.bBatteryEnabled) { Failure = TEXT("Battery parameters changed inside a comparison trace."); return false; }
            const double ModelDt = Flight - Integral.FlightSeconds;
            const double SensorDt = Sensor - Integral.SensorSeconds;
            if (!FMath::IsFinite(ModelDt) || ModelDt <= 0 || SensorDt <= 0 || FMath::Abs(ModelDt - Dt) > 1e-5 || FMath::Abs(SensorDt - ModelDt) > 1e-5)
            { Failure = TEXT("A consecutive trace lacks one positive, matching model-time interval."); return false; }
            const bool bReset = Epoch != Integral.Epoch;
            const double ChargeIncrement = bReset ? Consumed : Consumed - Integral.ConsumedAh;
            const double ChargeIntegral = Current * ModelDt / 3600.0;
            if (bReset) ++Integral.ResetCount;
            Integral.IntegratedCurrentAh += ChargeIntegral;
            Integral.ObservedConsumedAh += ChargeIncrement;
            Integral.IntegratedDeliveredJ += Delivered * ModelDt;
            Integral.IntegratedMotorAppliedJ += Motor * ModelDt;
            Integral.MaxChargeIntervalErrorAh = FMath::Max(Integral.MaxChargeIntervalErrorAh, FMath::Abs(ChargeIncrement - ChargeIntegral));
            if (ChargeIncrement < -ChargeToleranceAh || Integral.MaxChargeIntervalErrorAh > ChargeToleranceAh)
            { Failure = TEXT("Actual battery charge increment does not match the completed physics-step current integral."); return false; }
        }
        Integral.Epoch = Epoch;
        Integral.PhysicsStep = Step;
        Integral.FlightSeconds = Flight;
        Integral.SensorSeconds = Sensor;
        Integral.ConsumedAh = Consumed;
        TArray<FString> Columns = {Integer(Frames),Number(WorldSeconds),Number(Dt),Integer(Id),Number(Flight),Integer(Step),Number(Sensor),Integer(Epoch),
            Number(Position.X),Number(Position.Y),Number(Position.Z),Number(Velocity.X),Number(Velocity.Y),Number(Velocity.Z),
            Number(Rotation.X),Number(Rotation.Y),Number(Rotation.Z),Number(Rotation.W),Number(Angular.X),Number(Angular.Y),Number(Angular.Z),
            Drone->IsArmed()?TEXT("1"):TEXT("0"),Drone->IsAutonomousEnabled()?TEXT("1"):TEXT("0"),Number(Raw),Number(Command),
            Number(Current),Number(Consumed),Number(Soc),Number(Voltage),Number(Delivered),Number(Motor),Number(Available)};
        FrameText += FString::Join(Columns, TEXT(","));
        FrameText += TEXT("\n");
    }
    if (!WriteText(FrameText)) { Failure = TEXT("Streaming the flight trace CSV failed."); return false; }
    Rows += 10;
    ++Frames;
    WindowEnd = WorldSeconds;
    return true;
}

void AImperialFlightTrace::Tick(float Dt)
{
    Super::Tick(Dt);
    if (bFinished) return;
    auto* Scene = GetWorld()->GetPhysicsScene();
    if (!Scene || !Scene->GetSolver() || Scene->GetSolver()->IsUsingFixedDt()) { Finish(TEXT("Flight trace requires the original synchronous solver mode.")); return; }
    Scene->GetSolver()->WaitOnPendingTasks_External();
    if (!SampleFrame(Dt)) { Finish(Failure); return; }
    if (bFinishRequested) Finish();
    else if (WindowEnd - WindowStart >= TimeoutSeconds) Finish(TEXT("Flight trace timed out without PhysicsSmoke completion."));
}

void AImperialFlightTrace::Finish(const FString& Error)
{
    if (bFinished) return;
    bFinished = true;
    if (!Error.IsEmpty()) Failure = Error;
    if (!CloseWriter() && Failure.IsEmpty()) Failure = TEXT("The flight trace CSV could not be flushed/closed.");
    const bool bCoverage = Frames > 1 && WindowStart < 1 && bFinishRequested && SmokeCompletionElapsed >= 43 && WindowEnd + 1e-9 >= CompletionSignalWorld;
    bool bCharge = true;
    bool bResets = true;
    TArray<TSharedPtr<FJsonValue>> PerBody;
    for (int32 Id = 0; Id < 10; ++Id)
    {
        const auto& Integral = Bodies[Id];
        const double ChargeError = Integral.ObservedConsumedAh - Integral.IntegratedCurrentAh;
        bCharge &= Integral.bInitialized && Integral.MaxChargeIntervalErrorAh <= ChargeToleranceAh && FMath::Abs(ChargeError) <= ChargeToleranceAh;
        bResets &= Integral.ResetCount == (Id == 0 ? 3u : 0u);
        auto Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("id"), Id);
        Row->SetNumberField(TEXT("reset_count"), static_cast<double>(Integral.ResetCount));
        Row->SetNumberField(TEXT("first_reset_epoch"), static_cast<double>(Integral.FirstEpoch));
        Row->SetNumberField(TEXT("final_reset_epoch"), static_cast<double>(Integral.Epoch));
        Row->SetNumberField(TEXT("final_physics_step_seq"), static_cast<double>(Integral.PhysicsStep));
        Row->SetNumberField(TEXT("battery_capacity_ah"), Integral.CapacityAh);
        Row->SetBoolField(TEXT("battery_model_enabled"), Integral.bBatteryEnabled);
        Row->SetNumberField(TEXT("integrated_current_ah"), Integral.IntegratedCurrentAh);
        Row->SetNumberField(TEXT("observed_consumed_ah"), Integral.ObservedConsumedAh);
        Row->SetNumberField(TEXT("integrated_delivered_j"), Integral.IntegratedDeliveredJ);
        Row->SetNumberField(TEXT("integrated_motor_applied_j"), Integral.IntegratedMotorAppliedJ);
        Row->SetNumberField(TEXT("max_charge_interval_error_ah"), Integral.MaxChargeIntervalErrorAh);
        Row->SetNumberField(TEXT("charge_integral_error_ah"), ChargeError);
        PerBody.Add(MakeShared<FJsonValueObject>(Row));
    }
    if (Failure.IsEmpty())
    {
        if (!bCoverage) Failure = TEXT("Flight trace did not cover the pre-flight reference through the final completion frame.");
        else if (!bSmokeValid) Failure = TEXT("PhysicsSmoke reported failed final metrics.");
        else if (!bCharge) Failure = TEXT("Flight trace charge integral reconciliation failed.");
        else if (!bResets) Failure = TEXT("PhysicsSmoke reset coverage was not exactly three resets on aircraft zero and none on the others.");
    }
    const bool bOk = Failure.IsEmpty();
    auto Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("ok"), bOk);
    Result->SetStringField(TEXT("error"), Failure);
    Result->SetObjectField(TEXT("build"), ImperialBuildInfo::Json());
    Result->SetStringField(TEXT("csv_path"), CsvFile);
    Result->SetNumberField(TEXT("rows"), static_cast<double>(Rows));
    Result->SetNumberField(TEXT("frames"), static_cast<double>(Frames));
    Result->SetNumberField(TEXT("bodies"), 10);
    Result->SetNumberField(TEXT("requested_fps"), RequestedFPS);
    Result->SetNumberField(TEXT("actual_min_game_dt_s"), Frames > 1 ? MinimumGameDt : 0);
    Result->SetNumberField(TEXT("actual_max_game_dt_s"), MaximumGameDt);
    Result->SetNumberField(TEXT("window_start_world_s"), WindowStart);
    Result->SetNumberField(TEXT("window_end_world_s"), WindowEnd);
    Result->SetBoolField(TEXT("first_sample_is_reference"), true);
    Result->SetBoolField(TEXT("initial_interval_omitted"), true);
    Result->SetBoolField(TEXT("completed_by_smoke"), bFinishRequested);
    Result->SetNumberField(TEXT("completion_signal_world_s"), CompletionSignalWorld);
    Result->SetNumberField(TEXT("smoke_completion_elapsed_s"), SmokeCompletionElapsed);
    Result->SetBoolField(TEXT("completion_frame_covered"), bCoverage);
    Result->SetBoolField(TEXT("charge_integrals_match"), bCharge);
    Result->SetBoolField(TEXT("expected_resets_observed"), bResets);
    Result->SetBoolField(TEXT("finite_bodies"), bFiniteBodies);
    Result->SetBoolField(TEXT("force_path_changed"), false);
    Result->SetBoolField(TEXT("solver_observer_enabled"), bObserved);
    Result->SetNumberField(TEXT("maximum_rows"), static_cast<double>(MaximumRows));
    Result->SetNumberField(TEXT("timeout_game_s"), TimeoutSeconds);
    Result->SetArrayField(TEXT("per_body"), PerBody);
    Result->SetStringField(TEXT("scope"), TEXT("Every game physics-frame endpoint of ten synchronous Chaos bodies; not Chaos internal/substep state or a wall-time benchmark. Frame zero is a reference, excluded from integrals and FPS assertions. Charge uses endpoint I*deltaFlightModelSeconds/3600 and explicit reset epochs; energy is the same rectangle sum of recorded delivered/motor power, not independently measured work."));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ResultFile), true);
    FString Json;
    const bool bSaved = FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Json)) &&
        FFileHelper::SaveStringToFile(Json, *ResultFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    RestoreTiming();
    SetActorTickEnabled(false);
    if (bObserved && bFinishRequested)
    {
        if (auto* Probe = Cast<AImperialSolverProbe>(UGameplayStatics::GetActorOfClass(GetWorld(), AImperialSolverProbe::StaticClass())))
        {
            Probe->CompletePassthrough(SmokeCompletionElapsed, [bOk,bSaved](bool ProbeOk){ FPlatformMisc::RequestExitWithStatus(false, bOk && bSaved && ProbeOk ? 0 : 1); });
            return;
        }
    }
    FPlatformMisc::RequestExitWithStatus(false, bOk && bSaved ? 0 : 1);
}

void AImperialFlightTrace::EndPlay(const EEndPlayReason::Type Reason)
{
    if (!bFinished) Finish(TEXT("World ended before the complete flight trace was saved."));
    CloseWriter();
    RestoreTiming();
    Super::EndPlay(Reason);
}
