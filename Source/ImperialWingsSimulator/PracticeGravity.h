#pragma once

#include <cmath>

// Engine-independent, practice-only gravity selection. SI values use explicit
// unit suffixes; configurable bounds and presets are simulator choices.
namespace ImperialGravity
{
constexpr double StandardGravityMps2 = 9.80665;
constexpr double MinimumPracticeGravityMps2 = StandardGravityMps2 * 0.5;
constexpr double MaximumPracticeGravityMps2 = StandardGravityMps2 * 1.5;
constexpr double StandardGravityToleranceMps2 = 1.0e-9;
constexpr double DefaultAircraftMassKg = 0.9;
constexpr double DefaultTotalMaximumThrustN = 32.0;
constexpr double MinimumAircraftMassKg = 0.01;
constexpr double MaximumClass40AircraftMassKg = 1.10;
constexpr double MinimumTotalMaximumThrustN = 0.01;
constexpr double MaximumTotalMaximumThrustN = 100.0;
constexpr double DefaultWarningThresholdTwr = 1.5;
constexpr double MinimumWarningThresholdTwr = 1.0;
constexpr double MaximumWarningThresholdTwr = 10.0;
constexpr double MaximumFreeFallDistanceM = 100.0;
constexpr double MaximumFreeFallTimeS = 60.0;

enum class SessionMode { Practice, OfficialMatch, OfficialTopgun };
enum class EditStage { Idle, Selected, Review };
enum class GravityPreset { HalfStandard, Standard, OneAndHalfStandard };
enum class RecordCondition { StandardGravity, NonStandardPracticeGravity };
enum class Rejection
{
    None,
    NonFiniteGravity,
    GravityOutOfRange,
    PracticeOnly,
    OfficialGravityLocked,
    AircraftStillArmed,
    AircraftNotLanded,
    NoSelection,
    ReviewRequired,
    AlreadyReviewing,
    NonFiniteMass,
    MassOutOfRange,
    NonFiniteThrust,
    ThrustOutOfRange,
    NonFiniteDistance,
    DistanceOutOfRange,
    NonFiniteTime,
    TimeOutOfRange,
    NonFiniteWarningThreshold,
    WarningThresholdOutOfRange
};

struct ChangeContext
{
    SessionMode Mode = SessionMode::Practice;
    bool EveryAircraftDisarmed = true;
    bool EveryAircraftLanded = true;
};

struct ValueResult
{
    double Value = 0.0;
    Rejection Reason = Rejection::None;
    bool IsAccepted() const { return Reason == Rejection::None; }
};

struct ThrustMarginResult
{
    double WeightN = 0.0;
    double MaximumThrustN = 0.0;
    double ThrustToWeight = 0.0;
    double MarginN = 0.0;
    bool Warning = false;
    Rejection Reason = Rejection::None;
    bool IsAccepted() const { return Reason == Rejection::None; }
};

struct RecordPolicy
{
    bool IsEligible = false;
    RecordCondition Condition = RecordCondition::StandardGravity;
    Rejection Reason = Rejection::None;
};

inline double PresetGravityMps2(GravityPreset Preset)
{
    switch (Preset)
    {
    case GravityPreset::HalfStandard: return MinimumPracticeGravityMps2;
    case GravityPreset::Standard: return StandardGravityMps2;
    case GravityPreset::OneAndHalfStandard: return MaximumPracticeGravityMps2;
    }
    return StandardGravityMps2;
}

inline bool IsStandardGravity(double GravityMps2)
{
    return std::isfinite(GravityMps2) &&
        std::abs(GravityMps2 - StandardGravityMps2) <= StandardGravityToleranceMps2;
}

inline Rejection ValidateGravityMps2(double GravityMps2)
{
    if (!std::isfinite(GravityMps2)) return Rejection::NonFiniteGravity;
    if (GravityMps2 < MinimumPracticeGravityMps2 || GravityMps2 > MaximumPracticeGravityMps2)
        return Rejection::GravityOutOfRange;
    return Rejection::None;
}

inline Rejection ValidateMassKg(double MassKg)
{
    if (!std::isfinite(MassKg)) return Rejection::NonFiniteMass;
    if (MassKg < MinimumAircraftMassKg || MassKg > MaximumClass40AircraftMassKg)
        return Rejection::MassOutOfRange;
    return Rejection::None;
}

inline Rejection ValidateTotalMaximumThrustN(double TotalMaximumThrustN)
{
    if (!std::isfinite(TotalMaximumThrustN)) return Rejection::NonFiniteThrust;
    if (TotalMaximumThrustN < MinimumTotalMaximumThrustN || TotalMaximumThrustN > MaximumTotalMaximumThrustN)
        return Rejection::ThrustOutOfRange;
    return Rejection::None;
}

inline Rejection ValidateChangeContext(const ChangeContext& Context, double CandidateGravityMps2)
{
    const Rejection GravityReason = ValidateGravityMps2(CandidateGravityMps2);
    if (GravityReason != Rejection::None) return GravityReason;
    if (Context.Mode != SessionMode::Practice)
        return IsStandardGravity(CandidateGravityMps2) ? Rejection::PracticeOnly : Rejection::OfficialGravityLocked;
    if (!Context.EveryAircraftDisarmed) return Rejection::AircraftStillArmed;
    if (!Context.EveryAircraftLanded) return Rejection::AircraftNotLanded;
    return Rejection::None;
}

inline ValueResult ToUnrealGlobalGravityZCmps2(double GravityMps2)
{
    const Rejection GravityReason = ValidateGravityMps2(GravityMps2);
    if (GravityReason != Rejection::None) return {0.0, GravityReason};
    return {-GravityMps2 * 100.0, Rejection::None};
}

inline ValueResult HoverThrustN(double MassKg, double GravityMps2)
{
    const Rejection MassReason = ValidateMassKg(MassKg);
    if (MassReason != Rejection::None) return {0.0, MassReason};
    const Rejection GravityReason = ValidateGravityMps2(GravityMps2);
    if (GravityReason != Rejection::None) return {0.0, GravityReason};
    return {MassKg * GravityMps2, Rejection::None};
}

// Initial vertical speed is zero; drag, lift, ground effect and contact are excluded.
inline ValueResult FreeFallTimeS(double DistanceM, double GravityMps2)
{
    if (!std::isfinite(DistanceM)) return {0.0, Rejection::NonFiniteDistance};
    if (DistanceM < 0.0 || DistanceM > MaximumFreeFallDistanceM) return {0.0, Rejection::DistanceOutOfRange};
    const Rejection GravityReason = ValidateGravityMps2(GravityMps2);
    if (GravityReason != Rejection::None) return {0.0, GravityReason};
    return {std::sqrt((2.0 * DistanceM) / GravityMps2), Rejection::None};
}

// Initial vertical speed is zero; drag, lift, ground effect and contact are excluded.
inline ValueResult FreeFallDistanceM(double TimeS, double GravityMps2)
{
    if (!std::isfinite(TimeS)) return {0.0, Rejection::NonFiniteTime};
    if (TimeS < 0.0 || TimeS > MaximumFreeFallTimeS) return {0.0, Rejection::TimeOutOfRange};
    const Rejection GravityReason = ValidateGravityMps2(GravityMps2);
    if (GravityReason != Rejection::None) return {0.0, GravityReason};
    return {0.5 * GravityMps2 * TimeS * TimeS, Rejection::None};
}

inline ThrustMarginResult EvaluateThrustMargin(double MassKg, double TotalMaximumThrustN,
                                                double GravityMps2,
                                                double WarningThresholdTwr = DefaultWarningThresholdTwr)
{
    const Rejection MassReason = ValidateMassKg(MassKg);
    if (MassReason != Rejection::None) return {0.0, 0.0, 0.0, 0.0, false, MassReason};
    const Rejection ThrustReason = ValidateTotalMaximumThrustN(TotalMaximumThrustN);
    if (ThrustReason != Rejection::None) return {0.0, 0.0, 0.0, 0.0, false, ThrustReason};
    const Rejection GravityReason = ValidateGravityMps2(GravityMps2);
    if (GravityReason != Rejection::None) return {0.0, 0.0, 0.0, 0.0, false, GravityReason};
    if (!std::isfinite(WarningThresholdTwr))
        return {0.0, 0.0, 0.0, 0.0, false, Rejection::NonFiniteWarningThreshold};
    if (WarningThresholdTwr < MinimumWarningThresholdTwr || WarningThresholdTwr > MaximumWarningThresholdTwr)
        return {0.0, 0.0, 0.0, 0.0, false, Rejection::WarningThresholdOutOfRange};

    const double WeightN = MassKg * GravityMps2;
    const double ThrustToWeight = TotalMaximumThrustN / WeightN;
    return {WeightN, TotalMaximumThrustN, ThrustToWeight, TotalMaximumThrustN - WeightN,
            ThrustToWeight < WarningThresholdTwr, Rejection::None};
}

inline RecordPolicy ClassifyRecord(SessionMode Mode, double GravityMps2)
{
    const Rejection GravityReason = ValidateGravityMps2(GravityMps2);
    if (GravityReason != Rejection::None) return {false, RecordCondition::StandardGravity, GravityReason};
    const RecordCondition Condition = IsStandardGravity(GravityMps2) ?
        RecordCondition::StandardGravity : RecordCondition::NonStandardPracticeGravity;
    if (Mode != SessionMode::Practice && Condition != RecordCondition::StandardGravity)
        return {false, Condition, Rejection::OfficialGravityLocked};
    return {true, Condition, Rejection::None};
}

class PracticeGravity
{
public:
    Rejection Select(double CandidateGravityMps2, const ChangeContext& Context)
    {
        const Rejection ContextReason = ValidateChangeContext(Context, CandidateGravityMps2);
        if (ContextReason != Rejection::None) return SetRejection(ContextReason);
        if (Stage == EditStage::Review) return SetRejection(Rejection::AlreadyReviewing);
        PendingGravityMps2 = CandidateGravityMps2;
        Stage = EditStage::Selected;
        return SetRejection(Rejection::None);
    }

    Rejection Review(const ChangeContext& Context)
    {
        if (Stage == EditStage::Idle) return SetRejection(Rejection::NoSelection);
        if (Stage == EditStage::Review) return SetRejection(Rejection::AlreadyReviewing);
        const Rejection ContextReason = ValidateChangeContext(Context, PendingGravityMps2);
        if (ContextReason != Rejection::None) return SetRejection(ContextReason);
        Stage = EditStage::Review;
        return SetRejection(Rejection::None);
    }

    Rejection Apply(const ChangeContext& Context)
    {
        if (Stage == EditStage::Idle) return SetRejection(Rejection::NoSelection);
        if (Stage != EditStage::Review) return SetRejection(Rejection::ReviewRequired);
        const Rejection ContextReason = ValidateChangeContext(Context, PendingGravityMps2);
        if (ContextReason != Rejection::None) return SetRejection(ContextReason);
        ActiveGravityMps2 = PendingGravityMps2;
        Stage = EditStage::Idle;
        return SetRejection(Rejection::None);
    }

    void Cancel()
    {
        PendingGravityMps2 = ActiveGravityMps2;
        Stage = EditStage::Idle;
        LastRejection = Rejection::None;
    }

    double GetActiveGravityMps2() const { return ActiveGravityMps2; }
    double GetPendingGravityMps2() const { return PendingGravityMps2; }
    EditStage GetStage() const { return Stage; }
    Rejection GetLastRejection() const { return LastRejection; }
    RecordCondition GetRecordCondition() const
    {
        return IsStandardGravity(ActiveGravityMps2) ? RecordCondition::StandardGravity :
            RecordCondition::NonStandardPracticeGravity;
    }

private:
    Rejection SetRejection(Rejection Reason)
    {
        LastRejection = Reason;
        return Reason;
    }

    double ActiveGravityMps2 = StandardGravityMps2;
    double PendingGravityMps2 = StandardGravityMps2;
    EditStage Stage = EditStage::Idle;
    Rejection LastRejection = Rejection::None;
};
}
