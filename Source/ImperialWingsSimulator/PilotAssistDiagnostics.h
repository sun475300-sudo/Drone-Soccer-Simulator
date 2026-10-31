#pragma once

#include "FirstFlightGuide.h"

#include <array>
#include <cmath>
#include <cstddef>

// Pure diagnostic and explicit-request helpers. They never arm, calibrate,
// move, or reset an aircraft.
namespace ImperialPilotAssist
{
constexpr std::size_t DriftAxisCount = 4U;
constexpr std::size_t ArmingBlockCapacity = 10U;
constexpr std::size_t OtherAircraftCount = 9U;
constexpr std::size_t MaximumDriftSamples = 64U;

// Matches ImperialInput::ArmingEdgeGate's low-throttle arming limit.
constexpr double ArmLowThrottleMaximum = 0.06;
// Matches ImperialRadio::Profile's default deadzone. Its valid range is 0..0.25.
constexpr double DefaultNeutralDeadband = 0.04;
constexpr double MaximumNeutralDeadband = 0.25;
// FIDA Class40's 40 cm exterior implies a 0.20 m protective-guard radius.
constexpr double GuardRadiusM = 0.20;

inline bool IsFinite(const double Value) noexcept
{
    return std::isfinite(Value);
}

inline bool IsNormalizedThrottle(const double Value) noexcept
{
    return IsFinite(Value) && Value >= 0.0 && Value <= 1.0;
}

inline bool IsCenteredAxis(const double Value) noexcept
{
    return IsFinite(Value) && Value >= -1.0 && Value <= 1.0;
}

inline bool IsValidNeutralDeadband(const double Value) noexcept
{
    return IsFinite(Value) && Value >= 0.0 && Value <= MaximumNeutralDeadband;
}

enum class MatchPhase
{
    Practice,
    Ready,
    Playing,
    Break,
    Finished
};

inline bool IsKnownMatchPhase(const MatchPhase Value) noexcept
{
    switch (Value)
    {
    case MatchPhase::Practice:
    case MatchPhase::Ready:
    case MatchPhase::Playing:
    case MatchPhase::Break:
    case MatchPhase::Finished:
        return true;
    }
    return false;
}

inline bool IsManualArmingPhase(const MatchPhase Value) noexcept
{
    return Value == MatchPhase::Practice || Value == MatchPhase::Playing;
}

enum class ArmingEvaluationStatus
{
    Ready,
    InvalidInput
};

enum class ArmingBlockReason
{
    MatchPhaseDisallows,
    AutonomyActive,
    InputSourceDisconnected,
    InputSourceNotEnabled,
    CalibrationNotSaved,
    ArmButtonHeldBeforeConnect,
    AircraftNotLanded,
    AircraftNotUpright,
    ThrottleNotLow,
    OtherAxesNotNeutral
};

// The returned key is locale-neutral and must be resolved by both Korean and
// English localization tables; this core deliberately contains no UI text.
inline const char* MessageKey(const ArmingBlockReason Reason) noexcept
{
    switch (Reason)
    {
    case ArmingBlockReason::MatchPhaseDisallows:
        return "pilot_assist.arm.match_phase_disallows";
    case ArmingBlockReason::AutonomyActive:
        return "pilot_assist.arm.autonomy_active";
    case ArmingBlockReason::InputSourceDisconnected:
        return "pilot_assist.arm.input_source_disconnected";
    case ArmingBlockReason::InputSourceNotEnabled:
        return "pilot_assist.arm.input_source_not_enabled";
    case ArmingBlockReason::CalibrationNotSaved:
        return "pilot_assist.arm.calibration_not_saved";
    case ArmingBlockReason::ArmButtonHeldBeforeConnect:
        return "pilot_assist.arm.button_held_before_connect";
    case ArmingBlockReason::AircraftNotLanded:
        return "pilot_assist.arm.aircraft_not_landed";
    case ArmingBlockReason::AircraftNotUpright:
        return "pilot_assist.arm.aircraft_not_upright";
    case ArmingBlockReason::ThrottleNotLow:
        return "pilot_assist.arm.throttle_not_low";
    case ArmingBlockReason::OtherAxesNotNeutral:
        return "pilot_assist.arm.other_axes_not_neutral";
    }
    return "pilot_assist.arm.unknown";
}

struct ArmingPreconditions
{
    double RawThrottle = 0.0;
    std::array<double, 3U> OtherAxes{{0.0, 0.0, 0.0}};
    double NeutralDeadband = DefaultNeutralDeadband;
    MatchPhase CurrentMatchPhase = MatchPhase::Practice;
    bool InputSourceConnected = true;
    bool InputSourceEnabled = true;
    bool CalibrationSaved = true;
    bool AutonomyActive = false;
    bool AircraftLanded = true;
    bool AircraftUpright = true;
    bool ArmButtonHeldBeforeConnect = false;
};

struct ArmingExplanation
{
    ArmingEvaluationStatus Status = ArmingEvaluationStatus::InvalidInput;
    std::array<ArmingBlockReason, ArmingBlockCapacity> Reasons{};
    std::size_t Count = 0U;

    bool CanExplainSafeArm() const noexcept
    {
        return Status == ArmingEvaluationStatus::Ready && Count == 0U;
    }
};

inline bool IsValidArmingPreconditions(const ArmingPreconditions& Value) noexcept
{
    if (!IsNormalizedThrottle(Value.RawThrottle) || !IsValidNeutralDeadband(Value.NeutralDeadband) ||
        !IsKnownMatchPhase(Value.CurrentMatchPhase))
    {
        return false;
    }
    for (const double Axis : Value.OtherAxes)
    {
        if (!IsCenteredAxis(Axis))
        {
            return false;
        }
    }
    return true;
}

inline void AddArmingBlock(ArmingExplanation& Explanation, const ArmingBlockReason Reason) noexcept
{
    if (Explanation.Count < Explanation.Reasons.size())
    {
        Explanation.Reasons[Explanation.Count] = Reason;
        ++Explanation.Count;
    }
}

// Priority is stable: session authority, valid manual source, accidental-edge
// protection, aircraft condition, then current control positions.
inline ArmingExplanation ExplainArmingBlock(const ArmingPreconditions& Value) noexcept
{
    ArmingExplanation Result{};
    if (!IsValidArmingPreconditions(Value))
    {
        Result.Status = ArmingEvaluationStatus::InvalidInput;
        return Result;
    }

    Result.Status = ArmingEvaluationStatus::Ready;
    if (!IsManualArmingPhase(Value.CurrentMatchPhase))
    {
        AddArmingBlock(Result, ArmingBlockReason::MatchPhaseDisallows);
    }
    if (Value.AutonomyActive)
    {
        AddArmingBlock(Result, ArmingBlockReason::AutonomyActive);
    }
    if (!Value.InputSourceConnected)
    {
        AddArmingBlock(Result, ArmingBlockReason::InputSourceDisconnected);
    }
    if (!Value.InputSourceEnabled)
    {
        AddArmingBlock(Result, ArmingBlockReason::InputSourceNotEnabled);
    }
    if (!Value.CalibrationSaved)
    {
        AddArmingBlock(Result, ArmingBlockReason::CalibrationNotSaved);
    }
    if (Value.ArmButtonHeldBeforeConnect)
    {
        AddArmingBlock(Result, ArmingBlockReason::ArmButtonHeldBeforeConnect);
    }
    if (!Value.AircraftLanded)
    {
        AddArmingBlock(Result, ArmingBlockReason::AircraftNotLanded);
    }
    if (!Value.AircraftUpright)
    {
        AddArmingBlock(Result, ArmingBlockReason::AircraftNotUpright);
    }
    if (Value.RawThrottle > ArmLowThrottleMaximum)
    {
        AddArmingBlock(Result, ArmingBlockReason::ThrottleNotLow);
    }
    for (const double Axis : Value.OtherAxes)
    {
        if (std::abs(Axis) > Value.NeutralDeadband)
        {
            AddArmingBlock(Result, ArmingBlockReason::OtherAxesNotNeutral);
            break;
        }
    }
    return Result;
}

enum class DriftAddRejection
{
    None,
    InvalidSettings,
    DisarmedRequired,
    ReleaseRequestRequired,
    InvalidSample,
    WindowFull
};

enum class DriftClassification
{
    Invalid,
    InsufficientSamples,
    Ok,
    DriftBeyondDeadband,
    Noisy
};

struct DriftSettings
{
    std::size_t RequiredSamples = 16U;
    double NeutralDeadband = DefaultNeutralDeadband;
    // Simulator-selected variability cutoff for normalized, release-position samples.
    double NoisyStandardDeviation = 0.015;
};

inline bool IsValidDriftSettings(const DriftSettings& Value) noexcept
{
    return Value.RequiredSamples > 0U && Value.RequiredSamples <= MaximumDriftSamples &&
        IsValidNeutralDeadband(Value.NeutralDeadband) && IsFinite(Value.NoisyStandardDeviation) &&
        Value.NoisyStandardDeviation >= 0.0 && Value.NoisyStandardDeviation <= 1.0;
}

struct DriftCollectionContext
{
    bool AircraftDisarmed = false;
    bool UserAskedToReleaseSticks = false;
};

struct AxisDriftStatistics
{
    double Mean = 0.0;
    double StandardDeviation = 0.0;
    double MaximumAbsoluteValue = 0.0;
};

struct DriftDiagnosis
{
    DriftClassification Classification = DriftClassification::Invalid;
    std::array<AxisDriftStatistics, DriftAxisCount> Axes{};
    std::size_t SampleCount = 0U;
    bool RecommendRecalibration = false;
};

class StickDriftCollector
{
public:
    explicit StickDriftCollector(const DriftSettings& InSettings = DriftSettings()) noexcept
        : Settings(InSettings)
    {
    }

    void Reset() noexcept
    {
        Accumulators = {};
        Samples = 0U;
    }

    DriftAddRejection AddSample(const DriftCollectionContext& Context,
        const std::array<double, DriftAxisCount>& CenteredAxes) noexcept
    {
        if (!IsValidDriftSettings(Settings))
        {
            return DriftAddRejection::InvalidSettings;
        }
        if (!Context.AircraftDisarmed)
        {
            return DriftAddRejection::DisarmedRequired;
        }
        if (!Context.UserAskedToReleaseSticks)
        {
            return DriftAddRejection::ReleaseRequestRequired;
        }
        if (Samples >= MaximumDriftSamples)
        {
            return DriftAddRejection::WindowFull;
        }
        for (const double Axis : CenteredAxes)
        {
            if (!IsCenteredAxis(Axis))
            {
                return DriftAddRejection::InvalidSample;
            }
        }
        for (std::size_t Index = 0U; Index < DriftAxisCount; ++Index)
        {
            AxisAccumulator& Accumulator = Accumulators[Index];
            const double Axis = CenteredAxes[Index];
            Accumulator.Sum += Axis;
            Accumulator.SumSquares += Axis * Axis;
            Accumulator.MaximumAbsoluteValue = std::fmax(Accumulator.MaximumAbsoluteValue, std::abs(Axis));
        }
        ++Samples;
        return DriftAddRejection::None;
    }

    DriftDiagnosis Diagnose() const noexcept
    {
        DriftDiagnosis Result{};
        Result.SampleCount = Samples;
        if (!IsValidDriftSettings(Settings))
        {
            return Result;
        }
        if (Samples < Settings.RequiredSamples)
        {
            Result.Classification = DriftClassification::InsufficientSamples;
            return Result;
        }

        const double SampleCountAsDouble = static_cast<double>(Samples);
        bool HasDrift = false;
        bool HasNoise = false;
        for (std::size_t Index = 0U; Index < DriftAxisCount; ++Index)
        {
            const AxisAccumulator& Accumulator = Accumulators[Index];
            AxisDriftStatistics& Statistics = Result.Axes[Index];
            Statistics.Mean = Accumulator.Sum / SampleCountAsDouble;
            const double Variance = Accumulator.SumSquares / SampleCountAsDouble - Statistics.Mean * Statistics.Mean;
            Statistics.StandardDeviation = std::sqrt(Variance > 0.0 ? Variance : 0.0);
            Statistics.MaximumAbsoluteValue = Accumulator.MaximumAbsoluteValue;
            if (std::abs(Statistics.Mean) > Settings.NeutralDeadband ||
                Statistics.MaximumAbsoluteValue > Settings.NeutralDeadband)
            {
                HasDrift = true;
            }
            if (Statistics.StandardDeviation > Settings.NoisyStandardDeviation)
            {
                HasNoise = true;
            }
        }

        Result.Classification = HasNoise ? DriftClassification::Noisy :
            (HasDrift ? DriftClassification::DriftBeyondDeadband : DriftClassification::Ok);
        Result.RecommendRecalibration = Result.Classification == DriftClassification::Noisy ||
            Result.Classification == DriftClassification::DriftBeyondDeadband;
        return Result;
    }

    std::size_t SampleCount() const noexcept
    {
        return Samples;
    }

private:
    struct AxisAccumulator
    {
        double Sum = 0.0;
        double SumSquares = 0.0;
        double MaximumAbsoluteValue = 0.0;
    };

    DriftSettings Settings;
    std::array<AxisAccumulator, DriftAxisCount> Accumulators{};
    std::size_t Samples = 0U;
};

enum class FailureKind
{
    None,
    Crash,
    Flipped
};

enum class RestartState
{
    Idle,
    AwaitingSafeDisarm,
    ReadyForExplicitRestart,
    ExplicitRestartRequested
};

enum class RestartRejection
{
    None,
    NoFailureRecorded,
    SafetyConditionsIncomplete,
    InvalidInput,
    SelectedAircraftStillArmed,
    ThrottleNotLow,
    OtherAxesNotNeutral,
    ExplicitUserCommandRequired,
    AlreadyRequested
};

struct RestartSafetyInput
{
    bool SelectedAircraftArmed = true;
    double RawThrottle = 1.0;
    std::array<double, 3U> OtherAxes{{0.0, 0.0, 0.0}};
    double NeutralDeadband = DefaultNeutralDeadband;
};

inline bool IsValidRestartSafetyInput(const RestartSafetyInput& Value) noexcept
{
    if (!IsNormalizedThrottle(Value.RawThrottle) || !IsValidNeutralDeadband(Value.NeutralDeadband))
    {
        return false;
    }
    for (const double Axis : Value.OtherAxes)
    {
        if (!IsCenteredAxis(Axis))
        {
            return false;
        }
    }
    return true;
}

struct RestartDecision
{
    bool RequestRestart = false;
    RestartRejection Rejection = RestartRejection::NoFailureRecorded;
};

class DisarmRestartHelper
{
public:
    void BeginAfterFailure(const FailureKind Kind) noexcept
    {
        CurrentState = (Kind == FailureKind::Crash || Kind == FailureKind::Flipped) ?
            RestartState::AwaitingSafeDisarm : RestartState::Idle;
    }

    RestartRejection ObserveSafety(const RestartSafetyInput& Input) noexcept
    {
        if (CurrentState == RestartState::Idle)
        {
            return RestartRejection::NoFailureRecorded;
        }
        if (CurrentState == RestartState::ExplicitRestartRequested)
        {
            return RestartRejection::AlreadyRequested;
        }
        CurrentState = RestartState::AwaitingSafeDisarm;
        if (!IsValidRestartSafetyInput(Input))
        {
            return RestartRejection::InvalidInput;
        }
        if (Input.SelectedAircraftArmed)
        {
            return RestartRejection::SelectedAircraftStillArmed;
        }
        if (Input.RawThrottle > ArmLowThrottleMaximum)
        {
            return RestartRejection::ThrottleNotLow;
        }
        for (const double Axis : Input.OtherAxes)
        {
            if (std::abs(Axis) > Input.NeutralDeadband)
            {
                return RestartRejection::OtherAxesNotNeutral;
            }
        }
        CurrentState = RestartState::ReadyForExplicitRestart;
        return RestartRejection::None;
    }

    RestartDecision RequestExplicitRestart(const bool ExplicitUserCommand) noexcept
    {
        RestartDecision Result{};
        if (!ExplicitUserCommand)
        {
            Result.Rejection = RestartRejection::ExplicitUserCommandRequired;
            return Result;
        }
        if (CurrentState == RestartState::Idle)
        {
            Result.Rejection = RestartRejection::NoFailureRecorded;
            return Result;
        }
        if (CurrentState == RestartState::ExplicitRestartRequested)
        {
            Result.Rejection = RestartRejection::AlreadyRequested;
            return Result;
        }
        if (CurrentState != RestartState::ReadyForExplicitRestart)
        {
            Result.Rejection = RestartRejection::SafetyConditionsIncomplete;
            return Result;
        }
        CurrentState = RestartState::ExplicitRestartRequested;
        Result.RequestRestart = true;
        Result.Rejection = RestartRejection::None;
        return Result;
    }

    RestartState State() const noexcept
    {
        return CurrentState;
    }

private:
    RestartState CurrentState = RestartState::Idle;
};

struct PositionM
{
    double X_M = 0.0;
    double Y_M = 0.0;
    double Z_M = 0.0;
};

inline bool IsFinitePosition(const PositionM& Value) noexcept
{
    return IsFinite(Value.X_M) && IsFinite(Value.Y_M) && IsFinite(Value.Z_M);
}

struct ArenaBoundsM
{
    // FIDA Class40 recommended 16 m x 7 m x 4.5 m court, centred at X/Y zero.
    double MinimumX_M = -8.0;
    double MaximumX_M = 8.0;
    double MinimumY_M = -3.5;
    double MaximumY_M = 3.5;
    double MinimumZ_M = 0.0;
    double MaximumZ_M = 4.5;
};

inline bool IsValidArenaBounds(const ArenaBoundsM& Value) noexcept
{
    return IsFinite(Value.MinimumX_M) && IsFinite(Value.MaximumX_M) &&
        IsFinite(Value.MinimumY_M) && IsFinite(Value.MaximumY_M) &&
        IsFinite(Value.MinimumZ_M) && IsFinite(Value.MaximumZ_M) &&
        Value.MinimumX_M < Value.MaximumX_M && Value.MinimumY_M < Value.MaximumY_M &&
        Value.MinimumZ_M < Value.MaximumZ_M;
}

inline bool IsInsideArena(const PositionM& Position, const ArenaBoundsM& Bounds) noexcept
{
    return IsFinitePosition(Position) && IsValidArenaBounds(Bounds) &&
        Position.X_M >= Bounds.MinimumX_M && Position.X_M <= Bounds.MaximumX_M &&
        Position.Y_M >= Bounds.MinimumY_M && Position.Y_M <= Bounds.MaximumY_M &&
        Position.Z_M >= Bounds.MinimumZ_M && Position.Z_M <= Bounds.MaximumZ_M;
}

struct FailureCheckpoint
{
    PositionM Position{};
    double YawDeg = 0.0;
    double PitchDeg = 0.0;
    double RollDeg = 0.0;
};

inline bool IsValidFailureCheckpoint(const FailureCheckpoint& Value, const ArenaBoundsM& Bounds) noexcept
{
    return IsInsideArena(Value.Position, Bounds) && IsFinite(Value.YawDeg) && IsFinite(Value.PitchDeg) &&
        IsFinite(Value.RollDeg) && Value.YawDeg >= -180.0 && Value.YawDeg <= 180.0 &&
        Value.PitchDeg >= -180.0 && Value.PitchDeg <= 180.0 &&
        Value.RollDeg >= -180.0 && Value.RollDeg <= 180.0;
}

struct OtherAircraftForReset
{
    bool Present = false;
    bool Armed = false;
    bool Autonomous = false;
    double AltitudeM = 0.0;
    double SpeedMps = 0.0;
    PositionM Position{};
};

enum class CheckpointRejection
{
    None,
    InvalidArenaBounds,
    InvalidCheckpoint
};

enum class PracticeResetRejection
{
    None,
    ExplicitUserCommandRequired,
    NoFailureCheckpoint,
    InvalidArenaBounds,
    InvalidClearanceMargin,
    InvalidOtherAircraftState,
    OtherAircraftNotSettled,
    OtherAircraftTooClose
};

struct PracticeResetRequest
{
    bool ShouldResetSelectedAircraft = false;
    FailureCheckpoint Target{};
    PracticeResetRejection Rejection = PracticeResetRejection::NoFailureCheckpoint;
};

class FailurePointPractice
{
public:
    explicit FailurePointPractice(const ArenaBoundsM& InBounds = ArenaBoundsM(),
        const double InClearanceMarginM = 0.10) noexcept
        : Bounds(InBounds), ClearanceMarginM(InClearanceMarginM)
    {
    }

    CheckpointRejection RecordFailure(const FailureCheckpoint& Candidate) noexcept
    {
        if (!IsValidArenaBounds(Bounds))
        {
            return CheckpointRejection::InvalidArenaBounds;
        }
        if (!IsValidFailureCheckpoint(Candidate, Bounds))
        {
            return CheckpointRejection::InvalidCheckpoint;
        }
        Checkpoint = Candidate;
        HasStoredCheckpoint = true;
        return CheckpointRejection::None;
    }

    PracticeResetRequest RequestReset(const bool ExplicitUserCommand,
        const std::array<OtherAircraftForReset, OtherAircraftCount>& Others) const noexcept
    {
        PracticeResetRequest Result{};
        if (!ExplicitUserCommand)
        {
            Result.Rejection = PracticeResetRejection::ExplicitUserCommandRequired;
            return Result;
        }
        if (!HasStoredCheckpoint)
        {
            Result.Rejection = PracticeResetRejection::NoFailureCheckpoint;
            return Result;
        }
        if (!IsValidArenaBounds(Bounds) || !IsValidFailureCheckpoint(Checkpoint, Bounds))
        {
            Result.Rejection = PracticeResetRejection::InvalidArenaBounds;
            return Result;
        }
        if (!IsFinite(ClearanceMarginM) || ClearanceMarginM < 0.0 || ClearanceMarginM > 1.0)
        {
            Result.Rejection = PracticeResetRejection::InvalidClearanceMargin;
            return Result;
        }

        std::array<ImperialFirstFlight::RestartOtherAircraft, OtherAircraftCount> SettledOthers{};
        for (std::size_t Index = 0U; Index < Others.size(); ++Index)
        {
            const OtherAircraftForReset& Other = Others[Index];
            if (!IsFinite(Other.AltitudeM) || !IsFinite(Other.SpeedMps) || Other.AltitudeM < 0.0 ||
                Other.SpeedMps < 0.0 || !IsInsideArena(Other.Position, Bounds))
            {
                Result.Rejection = PracticeResetRejection::InvalidOtherAircraftState;
                return Result;
            }
            SettledOthers[Index].Present = Other.Present;
            SettledOthers[Index].Armed = Other.Armed;
            SettledOthers[Index].Autonomous = Other.Autonomous;
            SettledOthers[Index].AltitudeM = Other.AltitudeM;
            SettledOthers[Index].SpeedMps = Other.SpeedMps;
        }
        if (!ImperialFirstFlight::CanResetSelectedAircraft(10, SettledOthers))
        {
            Result.Rejection = PracticeResetRejection::OtherAircraftNotSettled;
            return Result;
        }

        const double MinimumDistanceM = GuardRadiusM + ClearanceMarginM;
        const double MinimumDistanceSquaredM2 = MinimumDistanceM * MinimumDistanceM;
        for (const OtherAircraftForReset& Other : Others)
        {
            const double DeltaX_M = Other.Position.X_M - Checkpoint.Position.X_M;
            const double DeltaY_M = Other.Position.Y_M - Checkpoint.Position.Y_M;
            const double DeltaZ_M = Other.Position.Z_M - Checkpoint.Position.Z_M;
            const double DistanceSquaredM2 = DeltaX_M * DeltaX_M + DeltaY_M * DeltaY_M + DeltaZ_M * DeltaZ_M;
            if (DistanceSquaredM2 < MinimumDistanceSquaredM2)
            {
                Result.Rejection = PracticeResetRejection::OtherAircraftTooClose;
                return Result;
            }
        }

        Result.ShouldResetSelectedAircraft = true;
        Result.Target = Checkpoint;
        Result.Rejection = PracticeResetRejection::None;
        return Result;
    }

    bool HasCheckpoint() const noexcept
    {
        return HasStoredCheckpoint;
    }

private:
    ArenaBoundsM Bounds;
    double ClearanceMarginM = 0.10;
    FailureCheckpoint Checkpoint{};
    bool HasStoredCheckpoint = false;
};
}
