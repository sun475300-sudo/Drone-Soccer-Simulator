#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// Practice-only goal evaluation. This header neither controls aircraft nor performs I/O.
namespace ImperialTraining
{
struct PositionM
{
    double X_M=0.0;
    double Y_M=0.0;
    double Z_M=0.0;
};
struct AttitudeDeg
{
    double RollDeg=0.0;
    double PitchDeg=0.0;
    double YawDeg=0.0;
};
struct TrajectorySample
{
    double SimTimeS=0.0;
    PositionM Position{};
    AttitudeDeg Attitude{};
};

enum class InputMode
{
    Keyboard,
    ControllerMode1,
    ControllerMode2
};
enum class DrillKind
{
    HoverBoxHold,
    GateAccuracy,
    SegmentSpeed
};
enum class PassDirection
{
    Any,
    NegativeToPositive,
    PositiveToNegative
};
enum class MetricKind
{
    HoldTimeS,
    GateOffsetM,
    SegmentTimeS
};
enum class AttemptState
{
    Idle,
    Active,
    Succeeded,
    Failed,
    Aborted,
    Invalidated
};
enum class Rejection
{
    None,
    InvalidConditionKey,
    InvalidDefinition,
    InvalidAttemptId,
    NonFiniteSample,
    SampleOutOfRange,
    TimeNotIncreasing,
    SampleGapTooLarge,
    AttemptNotActive,
    AttemptFinished,
    GoalNotMet,
    AbortedByCaller,
    InvalidHistory,
    InvalidMedianWindow
};
enum class ComparisonBlockReason
{
    None,
    InvalidAttempt,
    SameAttempt,
    AutomaticTestSession,
    AbortedSession,
    DifferentDrill,
    DifferentCondition
};
enum class AudioCueKind
{
    None,
    Success,
    Failure,
    NewBest
};

struct Limits
{
    // All values in this structure are simulator-selected safety bounds, not FIDA rules.
    static constexpr double MaximumPositionM=1000.0;
    static constexpr double MaximumAttitudeDeg=720.0;
    static constexpr double MaximumAttemptTimeS=86400.0;
    static constexpr double MinimumSampleGapS=0.001;
    static constexpr double MaximumSampleGapS=1.0;
    static constexpr double MaximumGoalRadiusM=50.0;
    static constexpr double MaximumGateOffsetM=20.0;
    static constexpr double MaximumGoalTimeS=600.0;
    static constexpr double MinimumSegmentLengthM=0.01;
    static constexpr std::uint32_t SupportedAssistMask=0x0000ffffU;
    static constexpr std::size_t MaximumHistoryRecords=10000U;
    static constexpr std::size_t MaximumMedianWindow=32U;
};

struct SameConditionKey
{
    std::uint32_t DrillVersion=0U;
    std::uint32_t PhysicsProfileId=0U;
    double GravityMps2=0.0;
    std::uint32_t AssistOptionsMask=0U;
    InputMode Input=InputMode::Keyboard;
    std::uint64_t BuildFingerprint=0U;
};

struct CheckpointDefinition
{
    PositionM CentreM{};
    PositionM NormalUnit{1.0,0.0,0.0};
    double MaximumOffsetM=0.0;
    PassDirection Direction=PassDirection::Any;
};
struct HoverBoxHoldGoal
{
    PositionM CentreM{};
    double HorizontalRadiusM=0.0;
    double MinimumAltitudeM=0.0;
    double MaximumAltitudeM=0.0;
    double HoldTimeS=0.0;
};
struct GateAccuracyGoal
{
    CheckpointDefinition Gate{};
};
struct SegmentSpeedGoal
{
    CheckpointDefinition Start{};
    CheckpointDefinition End{};
    double TargetTimeS=0.0;
};
struct DrillDefinition
{
    DrillKind Kind=DrillKind::HoverBoxHold;
    double MaximumSampleGapS=0.25;
    HoverBoxHoldGoal Hover{};
    GateAccuracyGoal Gate{};
    SegmentSpeedGoal Segment{};
};

struct AttemptResult
{
    AttemptState State=AttemptState::Idle;
    Rejection Reason=Rejection::None;
    DrillKind Kind=DrillKind::HoverBoxHold;
    SameConditionKey Condition{};
    std::uint64_t AttemptId=0U;
    bool AutomaticTest=false;
    bool Success=false;
    bool HasMetric=false;
    MetricKind Metric=MetricKind::HoldTimeS;
    double MetricValue=0.0;
    double CompletionTimeS=0.0;
};
struct ComparisonDecision
{
    bool Eligible=false;
    ComparisonBlockReason Reason=ComparisonBlockReason::InvalidAttempt;
};
struct ProgressSummary
{
    std::uint32_t AttemptCount=0U;
    std::uint32_t SuccessCount=0U;
    std::uint32_t MetricCount=0U;
    std::uint32_t MedianWindowCount=0U;
    bool HasBest=false;
    bool HasMedian=false;
    MetricKind Metric=MetricKind::HoldTimeS;
    double BestMetric=0.0;
    double MedianOfLastNMetric=0.0;
};
struct SummaryBuildResult
{
    Rejection Reason=Rejection::None;
    ProgressSummary Summary{};
};
struct AudioCueIds
{
    bool Muted=false;
    std::uint32_t SuccessCueId=0U;
    std::uint32_t FailureCueId=0U;
    std::uint32_t NewBestCueId=0U;
};
struct AudioCueDecision
{
    bool ShouldPlay=false;
    AudioCueKind Kind=AudioCueKind::None;
    std::uint32_t CueId=0U;
};

inline bool IsFinite(const PositionM& Value) noexcept
{
    return std::isfinite(Value.X_M)&&std::isfinite(Value.Y_M)&&std::isfinite(Value.Z_M);
}
inline bool IsFinite(const AttitudeDeg& Value) noexcept
{
    return std::isfinite(Value.RollDeg)&&std::isfinite(Value.PitchDeg)&&std::isfinite(Value.YawDeg);
}
inline bool SameCondition(const SameConditionKey& A,const SameConditionKey& B) noexcept
{
    return A.DrillVersion==B.DrillVersion&&A.PhysicsProfileId==B.PhysicsProfileId&&
        A.GravityMps2==B.GravityMps2&&A.AssistOptionsMask==B.AssistOptionsMask&&
        A.Input==B.Input&&A.BuildFingerprint==B.BuildFingerprint;
}
inline bool IsFinished(const AttemptResult& Result) noexcept
{
    return Result.State==AttemptState::Succeeded||Result.State==AttemptState::Failed;
}
inline MetricKind MetricFor(DrillKind Kind) noexcept
{
    switch(Kind)
    {
    case DrillKind::HoverBoxHold:return MetricKind::HoldTimeS;
    case DrillKind::GateAccuracy:return MetricKind::GateOffsetM;
    case DrillKind::SegmentSpeed:return MetricKind::SegmentTimeS;
    }
    return MetricKind::HoldTimeS;
}
inline bool LowerMetricIsBetter(MetricKind Metric) noexcept
{
    return Metric==MetricKind::GateOffsetM||Metric==MetricKind::SegmentTimeS;
}
inline Rejection ValidateConditionKey(const SameConditionKey& Key) noexcept
{
    if(Key.DrillVersion==0U||Key.PhysicsProfileId==0U||Key.BuildFingerprint==0U||
        !std::isfinite(Key.GravityMps2)||Key.GravityMps2<0.0||Key.GravityMps2>30.0||
        (Key.AssistOptionsMask&~Limits::SupportedAssistMask)!=0U)
        return Rejection::InvalidConditionKey;
    if(Key.Input!=InputMode::Keyboard&&Key.Input!=InputMode::ControllerMode1&&Key.Input!=InputMode::ControllerMode2)
        return Rejection::InvalidConditionKey;
    return Rejection::None;
}
inline Rejection ValidateSample(const TrajectorySample& Sample) noexcept
{
    if(!std::isfinite(Sample.SimTimeS)||!IsFinite(Sample.Position)||!IsFinite(Sample.Attitude))
        return Rejection::NonFiniteSample;
    if(Sample.SimTimeS<0.0||Sample.SimTimeS>Limits::MaximumAttemptTimeS||
        std::abs(Sample.Position.X_M)>Limits::MaximumPositionM||
        std::abs(Sample.Position.Y_M)>Limits::MaximumPositionM||
        std::abs(Sample.Position.Z_M)>Limits::MaximumPositionM||
        std::abs(Sample.Attitude.RollDeg)>Limits::MaximumAttitudeDeg||
        std::abs(Sample.Attitude.PitchDeg)>Limits::MaximumAttitudeDeg||
        std::abs(Sample.Attitude.YawDeg)>Limits::MaximumAttitudeDeg)
        return Rejection::SampleOutOfRange;
    return Rejection::None;
}
inline Rejection ValidateCheckpoint(const CheckpointDefinition& Checkpoint) noexcept
{
    if(!IsFinite(Checkpoint.CentreM)||!IsFinite(Checkpoint.NormalUnit)||
        !std::isfinite(Checkpoint.MaximumOffsetM))
        return Rejection::InvalidDefinition;
    if(std::abs(Checkpoint.CentreM.X_M)>Limits::MaximumPositionM||
        std::abs(Checkpoint.CentreM.Y_M)>Limits::MaximumPositionM||
        std::abs(Checkpoint.CentreM.Z_M)>Limits::MaximumPositionM||
        Checkpoint.MaximumOffsetM<0.0||Checkpoint.MaximumOffsetM>Limits::MaximumGateOffsetM)
        return Rejection::InvalidDefinition;
    const double LengthSquared=Checkpoint.NormalUnit.X_M*Checkpoint.NormalUnit.X_M+
        Checkpoint.NormalUnit.Y_M*Checkpoint.NormalUnit.Y_M+
        Checkpoint.NormalUnit.Z_M*Checkpoint.NormalUnit.Z_M;
    if(LengthSquared<0.999999||LengthSquared>1.000001)
        return Rejection::InvalidDefinition;
    if(Checkpoint.Direction!=PassDirection::Any&&Checkpoint.Direction!=PassDirection::NegativeToPositive&&
        Checkpoint.Direction!=PassDirection::PositiveToNegative)
        return Rejection::InvalidDefinition;
    return Rejection::None;
}
inline Rejection ValidateDefinition(const DrillDefinition& Definition) noexcept
{
    if(!std::isfinite(Definition.MaximumSampleGapS)||Definition.MaximumSampleGapS<Limits::MinimumSampleGapS||
        Definition.MaximumSampleGapS>Limits::MaximumSampleGapS)
        return Rejection::InvalidDefinition;
    switch(Definition.Kind)
    {
    case DrillKind::HoverBoxHold:
        if(!IsFinite(Definition.Hover.CentreM)||!std::isfinite(Definition.Hover.HorizontalRadiusM)||
            !std::isfinite(Definition.Hover.MinimumAltitudeM)||!std::isfinite(Definition.Hover.MaximumAltitudeM)||
            !std::isfinite(Definition.Hover.HoldTimeS))
            return Rejection::InvalidDefinition;
        if(std::abs(Definition.Hover.CentreM.X_M)>Limits::MaximumPositionM||
            std::abs(Definition.Hover.CentreM.Y_M)>Limits::MaximumPositionM||
            std::abs(Definition.Hover.CentreM.Z_M)>Limits::MaximumPositionM||
            Definition.Hover.HorizontalRadiusM<=0.0||Definition.Hover.HorizontalRadiusM>Limits::MaximumGoalRadiusM||
            Definition.Hover.MinimumAltitudeM>Definition.Hover.MaximumAltitudeM||
            Definition.Hover.MinimumAltitudeM<-Limits::MaximumPositionM||
            Definition.Hover.MaximumAltitudeM>Limits::MaximumPositionM||
            Definition.Hover.HoldTimeS<=0.0||Definition.Hover.HoldTimeS>Limits::MaximumGoalTimeS)
            return Rejection::InvalidDefinition;
        return Rejection::None;
    case DrillKind::GateAccuracy:
        return ValidateCheckpoint(Definition.Gate.Gate);
    case DrillKind::SegmentSpeed:
    {
        const Rejection StartReason=ValidateCheckpoint(Definition.Segment.Start);
        if(StartReason!=Rejection::None)
            return StartReason;
        const Rejection EndReason=ValidateCheckpoint(Definition.Segment.End);
        if(EndReason!=Rejection::None||!std::isfinite(Definition.Segment.TargetTimeS)||
            Definition.Segment.TargetTimeS<=0.0||Definition.Segment.TargetTimeS>Limits::MaximumGoalTimeS)
            return Rejection::InvalidDefinition;
        const double DeltaX=Definition.Segment.End.CentreM.X_M-Definition.Segment.Start.CentreM.X_M;
        const double DeltaY=Definition.Segment.End.CentreM.Y_M-Definition.Segment.Start.CentreM.Y_M;
        const double DeltaZ=Definition.Segment.End.CentreM.Z_M-Definition.Segment.Start.CentreM.Z_M;
        const double LengthSquared=DeltaX*DeltaX+DeltaY*DeltaY+DeltaZ*DeltaZ;
        if(LengthSquared<Limits::MinimumSegmentLengthM*Limits::MinimumSegmentLengthM)
            return Rejection::InvalidDefinition;
        return Rejection::None;
    }
    }
    return Rejection::InvalidDefinition;
}

class GoalEvaluator
{
public:
    Rejection Begin(const DrillDefinition& Definition,const SameConditionKey& Condition,std::uint64_t AttemptId,
        bool AutomaticTest=false) noexcept
    {
        Reset();
        const Rejection ConditionReason=ValidateConditionKey(Condition);
        if(ConditionReason!=Rejection::None)
            return Invalidate(ConditionReason);
        const Rejection DefinitionReason=ValidateDefinition(Definition);
        if(DefinitionReason!=Rejection::None)
            return Invalidate(DefinitionReason);
        if(AttemptId==0U)
            return Invalidate(Rejection::InvalidAttemptId);
        DefinitionValue=Definition;
        Result.Kind=Definition.Kind;
        Result.Condition=Condition;
        Result.AttemptId=AttemptId;
        Result.AutomaticTest=AutomaticTest;
        Result.Metric=MetricFor(Definition.Kind);
        Result.State=AttemptState::Active;
        return Rejection::None;
    }
    Rejection AddSample(const TrajectorySample& Sample) noexcept
    {
        if(Result.State==AttemptState::Idle)
            return Rejection::AttemptNotActive;
        if(Result.State!=AttemptState::Active)
            return Rejection::AttemptFinished;
        const Rejection SampleReason=ValidateSample(Sample);
        if(SampleReason!=Rejection::None)
            return Invalidate(SampleReason);
        if(!HasPrevious)
        {
            Previous=Sample;
            HasPrevious=true;
            return Rejection::None;
        }
        const double DeltaTimeS=Sample.SimTimeS-Previous.SimTimeS;
        if(DeltaTimeS<=0.0)
            return Invalidate(Rejection::TimeNotIncreasing);
        if(DeltaTimeS>DefinitionValue.MaximumSampleGapS)
            return Invalidate(Rejection::SampleGapTooLarge);
        switch(DefinitionValue.Kind)
        {
        case DrillKind::HoverBoxHold:
            EvaluateHover(Sample,DeltaTimeS);
            break;
        case DrillKind::GateAccuracy:
            EvaluateGate(Sample);
            break;
        case DrillKind::SegmentSpeed:
            EvaluateSegment(Sample);
            break;
        }
        if(Result.State==AttemptState::Active)
            Previous=Sample;
        return Result.Reason;
    }
    AttemptResult Finish(bool AbortedByCaller=false) noexcept
    {
        if(Result.State!=AttemptState::Active)
            return Result;
        if(AbortedByCaller)
        {
            Result.State=AttemptState::Aborted;
            Result.Reason=Rejection::AbortedByCaller;
            Result.Success=false;
            return Result;
        }
        if(DefinitionValue.Kind==DrillKind::HoverBoxHold)
        {
            Result.HasMetric=true;
            Result.MetricValue=BestHoverHoldS;
        }
        Result.State=AttemptState::Failed;
        Result.Reason=Rejection::GoalNotMet;
        Result.Success=false;
        return Result;
    }
    const AttemptResult& GetResult() const noexcept { return Result; }
private:
    struct Crossing
    {
        bool Hit=false;
        double TimeS=0.0;
        double OffsetM=0.0;
    };
    DrillDefinition DefinitionValue{};
    AttemptResult Result{};
    TrajectorySample Previous{};
    bool HasPrevious=false;
    bool SegmentStarted=false;
    double SegmentStartTimeS=0.0;
    double CurrentHoverHoldS=0.0;
    double BestHoverHoldS=0.0;

    void Reset() noexcept
    {
        DefinitionValue={};
        Result={};
        Previous={};
        HasPrevious=false;
        SegmentStarted=false;
        SegmentStartTimeS=0.0;
        CurrentHoverHoldS=0.0;
        BestHoverHoldS=0.0;
    }
    Rejection Invalidate(Rejection Reason) noexcept
    {
        Result.State=AttemptState::Invalidated;
        Result.Reason=Reason;
        Result.Success=false;
        return Reason;
    }
    static bool DirectionMatches(PassDirection Direction,bool NegativeToPositive) noexcept
    {
        return Direction==PassDirection::Any||
            (Direction==PassDirection::NegativeToPositive&&NegativeToPositive)||
            (Direction==PassDirection::PositiveToNegative&&!NegativeToPositive);
    }
    static Crossing FindCrossing(const CheckpointDefinition& Checkpoint,const TrajectorySample& From,
        const TrajectorySample& To) noexcept
    {
        const double FromX=From.Position.X_M-Checkpoint.CentreM.X_M;
        const double FromY=From.Position.Y_M-Checkpoint.CentreM.Y_M;
        const double FromZ=From.Position.Z_M-Checkpoint.CentreM.Z_M;
        const double ToX=To.Position.X_M-Checkpoint.CentreM.X_M;
        const double ToY=To.Position.Y_M-Checkpoint.CentreM.Y_M;
        const double ToZ=To.Position.Z_M-Checkpoint.CentreM.Z_M;
        const double StartDistance=FromX*Checkpoint.NormalUnit.X_M+FromY*Checkpoint.NormalUnit.Y_M+
            FromZ*Checkpoint.NormalUnit.Z_M;
        const double EndDistance=ToX*Checkpoint.NormalUnit.X_M+ToY*Checkpoint.NormalUnit.Y_M+
            ToZ*Checkpoint.NormalUnit.Z_M;
        const bool NegativeToPositive=StartDistance<0.0&&EndDistance>=0.0;
        const bool PositiveToNegative=StartDistance>0.0&&EndDistance<=0.0;
        if((!NegativeToPositive&&!PositiveToNegative)||!DirectionMatches(Checkpoint.Direction,NegativeToPositive))
            return {};
        const double Denominator=StartDistance-EndDistance;
        if(Denominator==0.0)
            return {};
        const double Fraction=std::clamp(StartDistance/Denominator,0.0,1.0);
        const double HitX=FromX+(ToX-FromX)*Fraction;
        const double HitY=FromY+(ToY-FromY)*Fraction;
        const double HitZ=FromZ+(ToZ-FromZ)*Fraction;
        const double Along=HitX*Checkpoint.NormalUnit.X_M+HitY*Checkpoint.NormalUnit.Y_M+
            HitZ*Checkpoint.NormalUnit.Z_M;
        const double OffsetSquared=std::max(0.0,HitX*HitX+HitY*HitY+HitZ*HitZ-Along*Along);
        const double OffsetM=std::sqrt(OffsetSquared);
        if(OffsetM>Checkpoint.MaximumOffsetM)
            return {};
        Crossing ResultValue;
        ResultValue.Hit=true;
        ResultValue.TimeS=From.SimTimeS+(To.SimTimeS-From.SimTimeS)*Fraction;
        ResultValue.OffsetM=OffsetM;
        return ResultValue;
    }
    bool IsInsideHoverBox(const PositionM& Position) const noexcept
    {
        const double DeltaX=Position.X_M-DefinitionValue.Hover.CentreM.X_M;
        const double DeltaY=Position.Y_M-DefinitionValue.Hover.CentreM.Y_M;
        const double HorizontalDistanceSquared=DeltaX*DeltaX+DeltaY*DeltaY;
        return HorizontalDistanceSquared<=DefinitionValue.Hover.HorizontalRadiusM*DefinitionValue.Hover.HorizontalRadiusM&&
            Position.Z_M>=DefinitionValue.Hover.MinimumAltitudeM&&Position.Z_M<=DefinitionValue.Hover.MaximumAltitudeM;
    }
    void CompleteSuccess(double MetricValue,double CompletionTimeS) noexcept
    {
        Result.State=AttemptState::Succeeded;
        Result.Reason=Rejection::None;
        Result.Success=true;
        Result.HasMetric=true;
        Result.MetricValue=MetricValue;
        Result.CompletionTimeS=CompletionTimeS;
    }
    void CompleteSegment(double EndTimeS) noexcept
    {
        const double ElapsedS=EndTimeS-SegmentStartTimeS;
        Result.HasMetric=true;
        Result.MetricValue=ElapsedS;
        Result.CompletionTimeS=EndTimeS;
        if(ElapsedS<=DefinitionValue.Segment.TargetTimeS)
        {
            CompleteSuccess(ElapsedS,EndTimeS);
            return;
        }
        Result.State=AttemptState::Failed;
        Result.Reason=Rejection::GoalNotMet;
        Result.Success=false;
    }
    void EvaluateHover(const TrajectorySample& Sample,double DeltaTimeS) noexcept
    {
        if(IsInsideHoverBox(Previous.Position)&&IsInsideHoverBox(Sample.Position))
        {
            CurrentHoverHoldS+=DeltaTimeS;
            BestHoverHoldS=std::max(BestHoverHoldS,CurrentHoverHoldS);
            if(CurrentHoverHoldS>=DefinitionValue.Hover.HoldTimeS)
                CompleteSuccess(CurrentHoverHoldS,Sample.SimTimeS);
            return;
        }
        CurrentHoverHoldS=0.0;
    }
    void EvaluateGate(const TrajectorySample& Sample) noexcept
    {
        const Crossing Hit=FindCrossing(DefinitionValue.Gate.Gate,Previous,Sample);
        if(Hit.Hit)
            CompleteSuccess(Hit.OffsetM,Hit.TimeS);
    }
    void EvaluateSegment(const TrajectorySample& Sample) noexcept
    {
        const Crossing StartHit=FindCrossing(DefinitionValue.Segment.Start,Previous,Sample);
        const Crossing EndHit=FindCrossing(DefinitionValue.Segment.End,Previous,Sample);
        if(!SegmentStarted&&StartHit.Hit)
        {
            SegmentStarted=true;
            SegmentStartTimeS=StartHit.TimeS;
            if(EndHit.Hit&&EndHit.TimeS>SegmentStartTimeS)
                CompleteSegment(EndHit.TimeS);
            return;
        }
        if(SegmentStarted&&EndHit.Hit&&EndHit.TimeS>SegmentStartTimeS)
            CompleteSegment(EndHit.TimeS);
    }
};

inline ComparisonDecision CanCompare(const AttemptResult& Reference,const AttemptResult& Candidate) noexcept
{
    if(Reference.State==AttemptState::Aborted||Candidate.State==AttemptState::Aborted)
        return {false,ComparisonBlockReason::AbortedSession};
    if(Reference.AutomaticTest||Candidate.AutomaticTest)
        return {false,ComparisonBlockReason::AutomaticTestSession};
    if(!IsFinished(Reference)||!IsFinished(Candidate))
        return {false,ComparisonBlockReason::InvalidAttempt};
    if(Reference.AttemptId==Candidate.AttemptId)
        return {false,ComparisonBlockReason::SameAttempt};
    if(Reference.Kind!=Candidate.Kind)
        return {false,ComparisonBlockReason::DifferentDrill};
    if(!SameCondition(Reference.Condition,Candidate.Condition))
        return {false,ComparisonBlockReason::DifferentCondition};
    return {true,ComparisonBlockReason::None};
}

inline bool IsProgressEligible(const AttemptResult& ResultValue,const SameConditionKey& Condition,DrillKind Kind) noexcept
{
    return IsFinished(ResultValue)&&!ResultValue.AutomaticTest&&ResultValue.Kind==Kind&&
        SameCondition(ResultValue.Condition,Condition);
}
inline SummaryBuildResult BuildProgressSummary(const AttemptResult* History,std::size_t HistoryCount,std::size_t LastN,
    const SameConditionKey& Condition,DrillKind Kind) noexcept
{
    SummaryBuildResult Output;
    Output.Summary.Metric=MetricFor(Kind);
    if(ValidateConditionKey(Condition)!=Rejection::None||HistoryCount>Limits::MaximumHistoryRecords||
        (History==nullptr&&HistoryCount>0U))
    {
        Output.Reason=Rejection::InvalidHistory;
        return Output;
    }
    if(LastN==0U||LastN>Limits::MaximumMedianWindow)
    {
        Output.Reason=Rejection::InvalidMedianWindow;
        return Output;
    }
    for(std::size_t Index=0U;Index<HistoryCount;++Index)
    {
        const AttemptResult& Value=History[Index];
        if(!IsProgressEligible(Value,Condition,Kind))
            continue;
        ++Output.Summary.AttemptCount;
        if(Value.Success)
            ++Output.Summary.SuccessCount;
        if(!Value.HasMetric||Value.Metric!=Output.Summary.Metric||!std::isfinite(Value.MetricValue))
            continue;
        ++Output.Summary.MetricCount;
        if(!Output.Summary.HasBest||
            (LowerMetricIsBetter(Value.Metric)?Value.MetricValue<Output.Summary.BestMetric:Value.MetricValue>Output.Summary.BestMetric))
        {
            Output.Summary.HasBest=true;
            Output.Summary.BestMetric=Value.MetricValue;
        }
    }
    std::array<double,Limits::MaximumMedianWindow> LastMetrics{};
    std::size_t MetricIndex=0U;
    for(std::size_t ReverseIndex=HistoryCount;ReverseIndex>0U&&MetricIndex<LastN;--ReverseIndex)
    {
        const AttemptResult& Value=History[ReverseIndex-1U];
        if(IsProgressEligible(Value,Condition,Kind)&&Value.HasMetric&&Value.Metric==Output.Summary.Metric&&
            std::isfinite(Value.MetricValue))
        {
            LastMetrics[MetricIndex]=Value.MetricValue;
            ++MetricIndex;
        }
    }
    if(MetricIndex>0U)
    {
        std::sort(LastMetrics.begin(),LastMetrics.begin()+static_cast<std::ptrdiff_t>(MetricIndex));
        Output.Summary.HasMedian=true;
        Output.Summary.MedianWindowCount=static_cast<std::uint32_t>(MetricIndex);
        const std::size_t Middle=MetricIndex/2U;
        Output.Summary.MedianOfLastNMetric=MetricIndex%2U==0U?
            (LastMetrics[Middle-1U]+LastMetrics[Middle])*0.5:LastMetrics[Middle];
    }
    return Output;
}

inline bool IsNewBest(const AttemptResult& ResultValue,const ProgressSummary& PreviousSummary) noexcept
{
    if(!ResultValue.Success||!ResultValue.HasMetric||!std::isfinite(ResultValue.MetricValue)||
        ResultValue.Metric!=PreviousSummary.Metric)
        return false;
    if(!PreviousSummary.HasBest)
        return true;
    return LowerMetricIsBetter(ResultValue.Metric)?ResultValue.MetricValue<PreviousSummary.BestMetric:
        ResultValue.MetricValue>PreviousSummary.BestMetric;
}
inline AudioCueDecision SelectAudioCue(const AudioCueIds& Cues,bool Success,bool NewBest) noexcept
{
    if(Cues.Muted)
        return {};
    if(Success&&NewBest&&Cues.NewBestCueId!=0U)
        return {true,AudioCueKind::NewBest,Cues.NewBestCueId};
    if(Success&&Cues.SuccessCueId!=0U)
        return {true,AudioCueKind::Success,Cues.SuccessCueId};
    if(!Success&&Cues.FailureCueId!=0U)
        return {true,AudioCueKind::Failure,Cues.FailureCueId};
    return {};
}
}
