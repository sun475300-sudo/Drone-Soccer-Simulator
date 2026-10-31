#include "PilotAssistDiagnostics.h"

#include <array>
#include <cstring>
#include <iostream>
#include <limits>

namespace
{
using namespace ImperialPilotAssist;

int Checks = 0;
int Failures = 0;

void Expect(const bool Good, const char* const Name)
{
    ++Checks;
    if (!Good)
    {
        ++Failures;
        std::cerr << "FAIL " << Name << '\n';
    }
}

std::array<OtherAircraftForReset, OtherAircraftCount> SafeOtherAircraft()
{
    std::array<OtherAircraftForReset, OtherAircraftCount> Result{};
    for (std::size_t Index = 0U; Index < Result.size(); ++Index)
    {
        OtherAircraftForReset& Other = Result[Index];
        Other.Present = true;
        Other.Armed = false;
        Other.Autonomous = false;
        Other.AltitudeM = 0.20;
        Other.SpeedMps = 0.10;
        Other.Position = PositionM{1.0 + static_cast<double>(Index) * 0.50, 0.0, 0.20};
    }
    return Result;
}

void AddReleasedSamples(StickDriftCollector& Collector, const std::array<double, DriftAxisCount>& Sample,
    const std::size_t Count)
{
    const DriftCollectionContext Released{true, true};
    bool AllAccepted = true;
    for (std::size_t Index = 0U; Index < Count; ++Index)
    {
        const DriftAddRejection Result = Collector.AddSample(Released, Sample);
        if (Result != DriftAddRejection::None)
        {
            AllAccepted = false;
            break;
        }
    }
    Expect(AllAccepted, "released drift sample window accepted");
}
}

int main()
{
    std::cout << "범위: 헤더 전용 결정 로직 검사이며 Unreal 실행·실물 조종기·사람 비행을 검증하지 않습니다.\n";

    {
        const ArmingExplanation Ready = ExplainArmingBlock(ArmingPreconditions{});
        Expect(Ready.Status == ArmingEvaluationStatus::Ready && Ready.Count == 0U && Ready.CanExplainSafeArm(),
            "safe manual arming has no block explanation");

        ArmingPreconditions AllBlocked{};
        AllBlocked.RawThrottle = 0.50;
        AllBlocked.OtherAxes = {{0.05, 0.0, 0.0}};
        AllBlocked.CurrentMatchPhase = MatchPhase::Ready;
        AllBlocked.InputSourceConnected = false;
        AllBlocked.InputSourceEnabled = false;
        AllBlocked.CalibrationSaved = false;
        AllBlocked.AutonomyActive = true;
        AllBlocked.AircraftLanded = false;
        AllBlocked.AircraftUpright = false;
        AllBlocked.ArmButtonHeldBeforeConnect = true;
        const ArmingExplanation Explanation = ExplainArmingBlock(AllBlocked);
        constexpr std::array<ArmingBlockReason, ArmingBlockCapacity> ExpectedOrder{{
            ArmingBlockReason::MatchPhaseDisallows,
            ArmingBlockReason::AutonomyActive,
            ArmingBlockReason::InputSourceDisconnected,
            ArmingBlockReason::InputSourceNotEnabled,
            ArmingBlockReason::CalibrationNotSaved,
            ArmingBlockReason::ArmButtonHeldBeforeConnect,
            ArmingBlockReason::AircraftNotLanded,
            ArmingBlockReason::AircraftNotUpright,
            ArmingBlockReason::ThrottleNotLow,
            ArmingBlockReason::OtherAxesNotNeutral
        }};
        Expect(Explanation.Status == ArmingEvaluationStatus::Ready && Explanation.Count == ExpectedOrder.size(),
            "all simultaneous arming blocks are retained");
        Expect(Explanation.Reasons == ExpectedOrder, "arming block priority order is stable");
        Expect(std::strcmp(MessageKey(Explanation.Reasons[0U]), "pilot_assist.arm.match_phase_disallows") == 0 &&
            std::strcmp(MessageKey(Explanation.Reasons[9U]), "pilot_assist.arm.other_axes_not_neutral") == 0,
            "arming explanation exposes stable localization keys without text");

        ArmingPreconditions Boundary{};
        Boundary.RawThrottle = ArmLowThrottleMaximum;
        Boundary.OtherAxes = {{DefaultNeutralDeadband, -DefaultNeutralDeadband, 0.0}};
        Expect(ExplainArmingBlock(Boundary).Count == 0U, "low throttle and neutral deadband boundaries are inclusive");
        Boundary.RawThrottle = ArmLowThrottleMaximum + 0.000001;
        Expect(ExplainArmingBlock(Boundary).Reasons[0U] == ArmingBlockReason::ThrottleNotLow,
            "throttle immediately above established arm threshold blocks");
        Boundary.RawThrottle = ArmLowThrottleMaximum;
        Boundary.OtherAxes[1U] = -DefaultNeutralDeadband - 0.000001;
        const ArmingExplanation AxisBlocked = ExplainArmingBlock(Boundary);
        Expect(AxisBlocked.Count == 1U && AxisBlocked.Reasons[0U] == ArmingBlockReason::OtherAxesNotNeutral,
            "axis immediately outside deadband blocks");

        ArmingPreconditions Invalid{};
        Invalid.RawThrottle = std::numeric_limits<double>::quiet_NaN();
        Expect(ExplainArmingBlock(Invalid).Status == ArmingEvaluationStatus::InvalidInput,
            "nonfinite throttle is explicitly invalid");
        Invalid = ArmingPreconditions{};
        Invalid.NeutralDeadband = 0.250001;
        Expect(ExplainArmingBlock(Invalid).Status == ArmingEvaluationStatus::InvalidInput,
            "out of range calibration deadband is invalid");
        Invalid = ArmingPreconditions{};
        Invalid.OtherAxes[2U] = -1.000001;
        Expect(ExplainArmingBlock(Invalid).Status == ArmingEvaluationStatus::InvalidInput,
            "out of range normalized axis is invalid");
        Invalid = ArmingPreconditions{};
        Invalid.CurrentMatchPhase = static_cast<MatchPhase>(99);
        Expect(ExplainArmingBlock(Invalid).Status == ArmingEvaluationStatus::InvalidInput,
            "unknown match phase is invalid");
    }

    {
        StickDriftCollector Empty;
        Expect(Empty.Diagnose().Classification == DriftClassification::InsufficientSamples,
            "empty drift window reports insufficient samples");
        const DriftCollectionContext Armed{false, true};
        Expect(Empty.AddSample(Armed, {{0.0, 0.0, 0.0, 0.0}}) == DriftAddRejection::DisarmedRequired,
            "drift sampling is rejected while armed");
        const DriftCollectionContext NotAsked{true, false};
        Expect(Empty.AddSample(NotAsked, {{0.0, 0.0, 0.0, 0.0}}) == DriftAddRejection::ReleaseRequestRequired,
            "drift sampling requires an explicit release-sticks prompt");
        const DriftCollectionContext Released{true, true};
        Expect(Empty.AddSample(Released, {{0.0, 0.0, std::numeric_limits<double>::infinity(), 0.0}}) ==
            DriftAddRejection::InvalidSample, "nonfinite drift sample is rejected");

        StickDriftCollector Stable;
        AddReleasedSamples(Stable, {{0.0, 0.0, 0.0, 0.0}}, 16U);
        const DriftDiagnosis StableResult = Stable.Diagnose();
        Expect(StableResult.Classification == DriftClassification::Ok && !StableResult.RecommendRecalibration &&
            StableResult.SampleCount == 16U, "stable released sticks diagnose as OK after full window");
        Expect(StableResult.Axes[0U].Mean == 0.0 && StableResult.Axes[0U].StandardDeviation == 0.0 &&
            StableResult.Axes[0U].MaximumAbsoluteValue == 0.0, "stable axis statistics contain mean deviation and peak");

        StickDriftCollector Offset;
        AddReleasedSamples(Offset, {{0.05, 0.0, 0.0, 0.0}}, 16U);
        const DriftDiagnosis OffsetResult = Offset.Diagnose();
        Expect(OffsetResult.Classification == DriftClassification::DriftBeyondDeadband &&
            OffsetResult.RecommendRecalibration && OffsetResult.Axes[0U].Mean > DefaultNeutralDeadband &&
            OffsetResult.Axes[0U].MaximumAbsoluteValue == 0.05,
            "consistent released-stick offset requests recalibration without changing it");

        DriftSettings NoiseSettings{};
        NoiseSettings.RequiredSamples = 4U;
        NoiseSettings.NoisyStandardDeviation = 0.015;
        StickDriftCollector Noisy(NoiseSettings);
        AddReleasedSamples(Noisy, {{0.03, 0.0, 0.0, 0.0}}, 1U);
        AddReleasedSamples(Noisy, {{-0.03, 0.0, 0.0, 0.0}}, 1U);
        AddReleasedSamples(Noisy, {{0.03, 0.0, 0.0, 0.0}}, 1U);
        AddReleasedSamples(Noisy, {{-0.03, 0.0, 0.0, 0.0}}, 1U);
        const DriftDiagnosis NoisyResult = Noisy.Diagnose();
        Expect(NoisyResult.Classification == DriftClassification::Noisy && NoisyResult.RecommendRecalibration &&
            NoisyResult.Axes[0U].Mean == 0.0 && NoisyResult.Axes[0U].StandardDeviation > NoiseSettings.NoisyStandardDeviation &&
            NoisyResult.Axes[0U].MaximumAbsoluteValue == 0.03,
            "noise is separated from neutral offset and retains all requested statistics");

        DriftSettings InvalidSettings{};
        InvalidSettings.RequiredSamples = MaximumDriftSamples + 1U;
        StickDriftCollector InvalidCollector(InvalidSettings);
        Expect(InvalidCollector.AddSample(Released, {{0.0, 0.0, 0.0, 0.0}}) == DriftAddRejection::InvalidSettings &&
            InvalidCollector.Diagnose().Classification == DriftClassification::Invalid,
            "invalid drift window configuration is rejected explicitly");
    }

    {
        DisarmRestartHelper Restart;
        Restart.BeginAfterFailure(FailureKind::None);
        Expect(Restart.ObserveSafety(RestartSafetyInput{}) == RestartRejection::NoFailureRecorded,
            "restart safety cannot begin without crash or flip");

        Restart.BeginAfterFailure(FailureKind::Crash);
        RestartSafetyInput Safety{};
        Expect(Restart.ObserveSafety(Safety) == RestartRejection::SelectedAircraftStillArmed &&
            Restart.State() == RestartState::AwaitingSafeDisarm, "crashed aircraft must disarm before restart is offered");
        Safety.SelectedAircraftArmed = false;
        Safety.RawThrottle = ArmLowThrottleMaximum + 0.000001;
        Expect(Restart.ObserveSafety(Safety) == RestartRejection::ThrottleNotLow,
            "disarmed crashed aircraft still needs low throttle");
        Safety.RawThrottle = ArmLowThrottleMaximum;
        Safety.OtherAxes[0U] = DefaultNeutralDeadband + 0.000001;
        Expect(Restart.ObserveSafety(Safety) == RestartRejection::OtherAxesNotNeutral,
            "restart needs neutral non-throttle axes");
        Safety.OtherAxes[0U] = DefaultNeutralDeadband;
        Expect(Restart.ObserveSafety(Safety) == RestartRejection::None &&
            Restart.State() == RestartState::ReadyForExplicitRestart, "safe DISARM low-throttle neutral state enables an offer only");
        Expect(Restart.RequestExplicitRestart(false).Rejection == RestartRejection::ExplicitUserCommandRequired &&
            Restart.State() == RestartState::ReadyForExplicitRestart, "restart offer does not execute without an explicit command");
        const RestartDecision CommandedRestart = Restart.RequestExplicitRestart(true);
        Expect(CommandedRestart.RequestRestart && CommandedRestart.Rejection == RestartRejection::None &&
            Restart.State() == RestartState::ExplicitRestartRequested, "explicit user restart request is emitted once");
        Expect(Restart.RequestExplicitRestart(true).Rejection == RestartRejection::AlreadyRequested,
            "duplicate explicit restart is rejected");

        Restart.BeginAfterFailure(FailureKind::Flipped);
        Safety = RestartSafetyInput{};
        Safety.SelectedAircraftArmed = false;
        Safety.RawThrottle = std::numeric_limits<double>::quiet_NaN();
        Expect(Restart.ObserveSafety(Safety) == RestartRejection::InvalidInput,
            "nonfinite restart safety input is rejected after flip");
    }

    {
        FailurePointPractice Practice;
        const FailureCheckpoint Checkpoint{{0.0, 0.0, 0.20}, 45.0, 5.0, -5.0};
        Expect(Practice.RecordFailure(Checkpoint) == CheckpointRejection::None && Practice.HasCheckpoint(),
            "valid failure checkpoint records position yaw and attitude");
        std::array<OtherAircraftForReset, OtherAircraftCount> Others = SafeOtherAircraft();
        const PracticeResetRequest NoCommand = Practice.RequestReset(false, Others);
        Expect(!NoCommand.ShouldResetSelectedAircraft && NoCommand.Rejection == PracticeResetRejection::ExplicitUserCommandRequired,
            "failure location never produces a reset without explicit command");
        const PracticeResetRequest SafeReset = Practice.RequestReset(true, Others);
        Expect(SafeReset.ShouldResetSelectedAircraft && SafeReset.Rejection == PracticeResetRejection::None &&
            SafeReset.Target.Position.X_M == 0.0 && SafeReset.Target.YawDeg == 45.0 && SafeReset.Target.PitchDeg == 5.0 &&
            SafeReset.Target.RollDeg == -5.0, "safe explicit request returns checkpoint without teleporting itself");

        Others[0U].Position.X_M = 0.29;
        Expect(Practice.RequestReset(true, Others).Rejection == PracticeResetRejection::OtherAircraftTooClose,
            "guard radius plus margin rejects an otherwise settled nearby aircraft");
        Others = SafeOtherAircraft();
        Others[1U].Armed = true;
        Expect(Practice.RequestReset(true, Others).Rejection == PracticeResetRejection::OtherAircraftNotSettled,
            "other armed aircraft blocks selected-aircraft practice reset");
        Others = SafeOtherAircraft();
        Others[2U].AltitudeM = 0.04;
        Expect(Practice.RequestReset(true, Others).Rejection == PracticeResetRejection::OtherAircraftNotSettled,
            "existing settled-altitude lower boundary is reused for other aircraft");
        Others = SafeOtherAircraft();
        Others[3U].Position.Z_M = std::numeric_limits<double>::quiet_NaN();
        Expect(Practice.RequestReset(true, Others).Rejection == PracticeResetRejection::InvalidOtherAircraftState,
            "nonfinite other-aircraft state blocks reset request");

        const FailureCheckpoint Outside{{8.000001, 0.0, 0.20}, 0.0, 0.0, 0.0};
        Expect(Practice.RecordFailure(Outside) == CheckpointRejection::InvalidCheckpoint && Practice.HasCheckpoint(),
            "out of arena checkpoint is rejected without discarding previous safe checkpoint");
        const FailureCheckpoint Boundary{{8.0, 3.5, 4.5}, 180.0, -180.0, 180.0};
        Expect(Practice.RecordFailure(Boundary) == CheckpointRejection::None,
            "FIDA arena boundary and canonical attitude boundaries are accepted");

        ArenaBoundsM InvalidBounds{};
        InvalidBounds.MaximumX_M = InvalidBounds.MinimumX_M;
        FailurePointPractice InvalidPractice(InvalidBounds);
        Expect(InvalidPractice.RecordFailure(Checkpoint) == CheckpointRejection::InvalidArenaBounds,
            "invalid arena dimensions are rejected explicitly");
        FailurePointPractice NoCheckpoint;
        Expect(NoCheckpoint.RequestReset(true, SafeOtherAircraft()).Rejection == PracticeResetRejection::NoFailureCheckpoint,
            "explicit request without a recorded failed attempt is rejected");
    }

    std::cout << Checks << " checks, " << Failures << " failures\n";
    return Failures == 0 ? 0 : 1;
}
