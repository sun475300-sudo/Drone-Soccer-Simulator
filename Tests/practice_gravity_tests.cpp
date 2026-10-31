#include "PracticeGravity.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace
{
using namespace ImperialGravity;
int Checks = 0;
int Failures = 0;

void Expect(bool Good, const char* Name)
{
    ++Checks;
    if (!Good)
    {
        ++Failures;
        std::cerr << "FAIL " << Name << '\n';
    }
}

bool Near(double Left, double Right, double Tolerance = 1.0e-9)
{
    return std::abs(Left - Right) <= Tolerance;
}
}

int main()
{
    std::cout << "Pure C++ practice-gravity core tests; no Unreal build/run, hardware, network, or human validation.\n";

    const ChangeContext SafePractice{};
    PracticeGravity Gravity;
    Expect(Near(Gravity.GetActiveGravityMps2(), StandardGravityMps2) &&
        Gravity.GetStage() == EditStage::Idle, "default is idle at standard gravity");
    Expect(Gravity.GetRecordCondition() == RecordCondition::StandardGravity,
        "default gravity has standard record condition");
    Expect(Near(PresetGravityMps2(GravityPreset::HalfStandard), MinimumPracticeGravityMps2) &&
        Near(PresetGravityMps2(GravityPreset::Standard), StandardGravityMps2) &&
        Near(PresetGravityMps2(GravityPreset::OneAndHalfStandard), MaximumPracticeGravityMps2),
        "all documented presets resolve to their gravity values");

    Expect(Gravity.Select(MinimumPracticeGravityMps2, SafePractice) == Rejection::None,
        "practice selection accepts lower preset boundary");
    Expect(Gravity.GetStage() == EditStage::Selected &&
        Near(Gravity.GetActiveGravityMps2(), StandardGravityMps2) &&
        Near(Gravity.GetPendingGravityMps2(), MinimumPracticeGravityMps2),
        "selection changes pending value only");
    Expect(Gravity.Review(SafePractice) == Rejection::None && Gravity.GetStage() == EditStage::Review,
        "selection requires an explicit review state");
    Expect(Near(Gravity.GetActiveGravityMps2(), StandardGravityMps2),
        "review never changes active gravity");
    Expect(Gravity.Apply(SafePractice) == Rejection::None && Gravity.GetStage() == EditStage::Idle,
        "reviewed practice selection applies explicitly");
    Expect(Near(Gravity.GetActiveGravityMps2(), MinimumPracticeGravityMps2) &&
        Gravity.GetRecordCondition() == RecordCondition::NonStandardPracticeGravity,
        "applied nonstandard practice gravity is record-tagged");

    const ValueResult HalfConversion = ToUnrealGlobalGravityZCmps2(MinimumPracticeGravityMps2);
    Expect(HalfConversion.IsAccepted() && Near(HalfConversion.Value, -490.3325),
        "gravity converts to negative Unreal centimetres per second squared");
    Expect(Gravity.Select(MaximumPracticeGravityMps2, SafePractice) == Rejection::None &&
        Gravity.Review(SafePractice) == Rejection::None, "high preset can be selected and reviewed");
    Gravity.Cancel();
    Expect(Gravity.GetStage() == EditStage::Idle &&
        Near(Gravity.GetActiveGravityMps2(), MinimumPracticeGravityMps2) &&
        Near(Gravity.GetPendingGravityMps2(), MinimumPracticeGravityMps2),
        "cancel preserves old active gravity and discards the pending value");

    PracticeGravity Ordered;
    Expect(Ordered.Apply(SafePractice) == Rejection::NoSelection,
        "apply without selection is rejected");
    Expect(Ordered.Select(StandardGravityMps2, SafePractice) == Rejection::None,
        "standard gravity can enter the staged workflow");
    Expect(Ordered.Apply(SafePractice) == Rejection::ReviewRequired &&
        Near(Ordered.GetActiveGravityMps2(), StandardGravityMps2),
        "apply before review is rejected without changing active gravity");
    Expect(Ordered.Review(SafePractice) == Rejection::None &&
        Ordered.Apply(SafePractice) == Rejection::None &&
        Ordered.GetRecordCondition() == RecordCondition::StandardGravity,
        "review then apply preserves standard gravity record condition");

    PracticeGravity Revalidated;
    ChangeContext ArmedPractice = SafePractice;
    ArmedPractice.EveryAircraftDisarmed = false;
    ChangeContext AirbornePractice = SafePractice;
    AirbornePractice.EveryAircraftLanded = false;
    Expect(Revalidated.Select(MinimumPracticeGravityMps2, SafePractice) == Rejection::None &&
        Revalidated.Review(SafePractice) == Rejection::None, "apply revalidation fixture reaches review");
    Expect(Revalidated.Apply(ArmedPractice) == Rejection::AircraftStillArmed &&
        Revalidated.GetStage() == EditStage::Review &&
        Near(Revalidated.GetActiveGravityMps2(), StandardGravityMps2),
        "armed aircraft reject apply and retain old gravity");
    Expect(Revalidated.Apply(AirbornePractice) == Rejection::AircraftNotLanded &&
        Revalidated.GetStage() == EditStage::Review, "airborne aircraft reject apply");
    Expect(Revalidated.Apply(SafePractice) == Rejection::None &&
        Near(Revalidated.GetActiveGravityMps2(), MinimumPracticeGravityMps2),
        "safe state permits a previously reviewed selection");

    PracticeGravity UnsafeStart;
    Expect(UnsafeStart.Select(MinimumPracticeGravityMps2, ArmedPractice) == Rejection::AircraftStillArmed &&
        UnsafeStart.GetStage() == EditStage::Idle, "armed aircraft cannot begin a gravity change");
    Expect(UnsafeStart.Select(MinimumPracticeGravityMps2, AirbornePractice) == Rejection::AircraftNotLanded &&
        UnsafeStart.GetStage() == EditStage::Idle, "airborne aircraft cannot begin a gravity change");

    ChangeContext OfficialMatch = SafePractice;
    OfficialMatch.Mode = SessionMode::OfficialMatch;
    ChangeContext OfficialTopgun = SafePractice;
    OfficialTopgun.Mode = SessionMode::OfficialTopgun;
    PracticeGravity Locked;
    Expect(Locked.Select(MinimumPracticeGravityMps2, OfficialMatch) == Rejection::OfficialGravityLocked,
        "official match rejects nonstandard gravity");
    Expect(Locked.Select(StandardGravityMps2, OfficialMatch) == Rejection::PracticeOnly,
        "official match does not open practice gravity editing even at g0");
    Expect(Locked.Select(MaximumPracticeGravityMps2, OfficialTopgun) == Rejection::OfficialGravityLocked,
        "official Topgun rejects nonstandard gravity");

    const RecordPolicy PracticeHalf = ClassifyRecord(SessionMode::Practice, MinimumPracticeGravityMps2);
    const RecordPolicy PracticeStandard = ClassifyRecord(SessionMode::Practice, StandardGravityMps2);
    const RecordPolicy MatchStandard = ClassifyRecord(SessionMode::OfficialMatch, StandardGravityMps2);
    const RecordPolicy MatchHalf = ClassifyRecord(SessionMode::OfficialMatch, MinimumPracticeGravityMps2);
    const RecordPolicy TopgunStandard = ClassifyRecord(SessionMode::OfficialTopgun, StandardGravityMps2);
    const RecordPolicy TopgunHigh = ClassifyRecord(SessionMode::OfficialTopgun, MaximumPracticeGravityMps2);
    Expect(PracticeHalf.IsEligible && PracticeHalf.Condition == RecordCondition::NonStandardPracticeGravity,
        "practice nonstandard record receives a distinct condition tag");
    Expect(PracticeStandard.IsEligible && PracticeStandard.Condition == RecordCondition::StandardGravity,
        "practice g0 retains standard record condition");
    Expect(MatchStandard.IsEligible && MatchStandard.Condition == RecordCondition::StandardGravity,
        "official match record accepts only standard gravity condition");
    Expect(!MatchHalf.IsEligible && MatchHalf.Reason == Rejection::OfficialGravityLocked &&
        MatchHalf.Condition == RecordCondition::NonStandardPracticeGravity,
        "official match record rejects nonstandard condition rather than mixing it");
    Expect(TopgunStandard.IsEligible && TopgunStandard.Condition == RecordCondition::StandardGravity,
        "official Topgun record accepts g0 condition");
    Expect(!TopgunHigh.IsEligible && TopgunHigh.Reason == Rejection::OfficialGravityLocked,
        "official Topgun record rejects high-gravity condition");

    PracticeGravity Bounds;
    Expect(Bounds.Select(MinimumPracticeGravityMps2, SafePractice) == Rejection::None,
        "minimum allowed gravity is inclusive");
    Bounds.Cancel();
    Expect(Bounds.Select(MaximumPracticeGravityMps2, SafePractice) == Rejection::None,
        "maximum allowed gravity is inclusive");
    Bounds.Cancel();
    Expect(Bounds.Select(MinimumPracticeGravityMps2 - 1.0e-6, SafePractice) == Rejection::GravityOutOfRange,
        "gravity below practice range is rejected");
    Expect(Bounds.Select(MaximumPracticeGravityMps2 + 1.0e-6, SafePractice) == Rejection::GravityOutOfRange,
        "gravity above practice range is rejected");
    Expect(Bounds.Select(std::numeric_limits<double>::infinity(), SafePractice) == Rejection::NonFiniteGravity,
        "infinite gravity is rejected");
    Expect(Bounds.Select(std::numeric_limits<double>::quiet_NaN(), SafePractice) == Rejection::NonFiniteGravity,
        "NaN gravity is rejected");

    const ValueResult StandardConversion = ToUnrealGlobalGravityZCmps2(StandardGravityMps2);
    Expect(StandardConversion.IsAccepted() && Near(StandardConversion.Value, -980.665),
        "standard gravity conversion uses centimetres and negative Z");
    Expect(ToUnrealGlobalGravityZCmps2(std::numeric_limits<double>::quiet_NaN()).Reason == Rejection::NonFiniteGravity,
        "conversion rejects nonfinite gravity");

    const ValueResult Hover = HoverThrustN(DefaultAircraftMassKg, StandardGravityMps2);
    Expect(Hover.IsAccepted() && Near(Hover.Value, 8.825985),
        "hover thrust equals 0.9 kg times standard gravity");
    Expect(HoverThrustN(0.0, StandardGravityMps2).Reason == Rejection::MassOutOfRange,
        "zero aircraft mass is rejected");
    Expect(HoverThrustN(1.100001, StandardGravityMps2).Reason == Rejection::MassOutOfRange,
        "mass above Class40 ceiling is rejected");

    const ValueResult OneMeterFall = FreeFallTimeS(1.0, StandardGravityMps2);
    Expect(OneMeterFall.IsAccepted() && Near(OneMeterFall.Value, 0.45160076, 1.0e-8),
        "one metre rest free-fall time matches standard gravity helper");
    const ValueResult RoundTripFall = FreeFallDistanceM(OneMeterFall.Value, StandardGravityMps2);
    Expect(RoundTripFall.IsAccepted() && Near(RoundTripFall.Value, 1.0, 1.0e-8),
        "free-fall time and distance helpers round trip one metre");
    Expect(FreeFallTimeS(0.0, StandardGravityMps2).IsAccepted() &&
        Near(FreeFallTimeS(0.0, StandardGravityMps2).Value, 0.0),
        "zero fall distance is a valid boundary");
    Expect(FreeFallDistanceM(0.0, StandardGravityMps2).IsAccepted() &&
        Near(FreeFallDistanceM(0.0, StandardGravityMps2).Value, 0.0),
        "zero fall time is a valid boundary");
    Expect(FreeFallTimeS(-0.01, StandardGravityMps2).Reason == Rejection::DistanceOutOfRange,
        "negative fall distance is rejected");
    Expect(FreeFallTimeS(MaximumFreeFallDistanceM + 0.01, StandardGravityMps2).Reason == Rejection::DistanceOutOfRange,
        "excessive fall distance is rejected");
    Expect(FreeFallDistanceM(-0.01, StandardGravityMps2).Reason == Rejection::TimeOutOfRange,
        "negative fall time is rejected");
    Expect(FreeFallDistanceM(MaximumFreeFallTimeS + 0.01, StandardGravityMps2).Reason == Rejection::TimeOutOfRange,
        "excessive fall time is rejected");
    Expect(FreeFallDistanceM(std::numeric_limits<double>::infinity(), StandardGravityMps2).Reason == Rejection::NonFiniteTime,
        "infinite fall time is rejected");

    const ThrustMarginResult StandardMargin = EvaluateThrustMargin(DefaultAircraftMassKg,
        DefaultTotalMaximumThrustN, StandardGravityMps2);
    Expect(StandardMargin.IsAccepted() && Near(StandardMargin.WeightN, 8.825985) &&
        Near(StandardMargin.MaximumThrustN, 32.0) && Near(StandardMargin.MarginN, 23.174015),
        "existing 0.9 kg and four-times-8 N model values produce expected thrust margin");
    Expect(!StandardMargin.Warning && Near(StandardMargin.ThrustToWeight, 3.62565765, 1.0e-8),
        "existing model exceeds simulator warning threshold");
    const ThrustMarginResult LowMargin = EvaluateThrustMargin(DefaultAircraftMassKg, 12.0, StandardGravityMps2);
    Expect(LowMargin.IsAccepted() && LowMargin.Warning && LowMargin.ThrustToWeight < DefaultWarningThresholdTwr,
        "low thrust-to-weight produces an explicit warning");
    Expect(EvaluateThrustMargin(DefaultAircraftMassKg, DefaultTotalMaximumThrustN,
        StandardGravityMps2, 4.0).Warning, "caller-selected higher warning threshold is respected");
    Expect(EvaluateThrustMargin(DefaultAircraftMassKg, DefaultTotalMaximumThrustN,
        StandardGravityMps2, 0.99).Reason == Rejection::WarningThresholdOutOfRange,
        "warning threshold below one is rejected");
    Expect(EvaluateThrustMargin(DefaultAircraftMassKg, std::numeric_limits<double>::infinity(),
        StandardGravityMps2).Reason == Rejection::NonFiniteThrust,
        "nonfinite maximum thrust is rejected");
    const ThrustMarginResult HighGravityMargin = EvaluateThrustMargin(DefaultAircraftMassKg,
        DefaultTotalMaximumThrustN, MaximumPracticeGravityMps2);
    Expect(HighGravityMargin.IsAccepted() && HighGravityMargin.ThrustToWeight < StandardMargin.ThrustToWeight &&
        Near(HighGravityMargin.ThrustToWeight, 2.41710510, 1.0e-8),
        "higher practice gravity reduces available thrust-to-weight margin");

    Expect(IsStandardGravity(StandardGravityMps2 + 0.5 * StandardGravityToleranceMps2) &&
        !IsStandardGravity(StandardGravityMps2 + 4.0 * StandardGravityToleranceMps2),
        "standard gravity comparison has a bounded representation tolerance");
    Expect(!ClassifyRecord(SessionMode::Practice, std::numeric_limits<double>::quiet_NaN()).IsEligible &&
        ClassifyRecord(SessionMode::Practice, std::numeric_limits<double>::quiet_NaN()).Reason == Rejection::NonFiniteGravity,
        "record classification rejects adversarial nonfinite gravity");

    std::cout << Checks << " checks, " << Failures << " failures\n";
    return Failures == 0 ? 0 : 1;
}
