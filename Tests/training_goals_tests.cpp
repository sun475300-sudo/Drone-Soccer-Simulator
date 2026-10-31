#include "TrainingGoals.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>

namespace
{
using namespace ImperialTraining;
int Checks=0;
int Failures=0;

void Expect(bool Value,const char* Name)
{
    ++Checks;
    if(!Value)
    {
        ++Failures;
        std::cerr<<"FAIL "<<Name<<'\n';
    }
}
bool Near(double A,double B,double Tolerance=1e-9)
{
    return std::abs(A-B)<=Tolerance;
}
SameConditionKey MakeCondition()
{
    SameConditionKey Value;
    Value.DrillVersion=7U;
    Value.PhysicsProfileId=42U;
    Value.GravityMps2=9.80665;
    Value.AssistOptionsMask=0x00000005U;
    Value.Input=InputMode::Keyboard;
    Value.BuildFingerprint=0x8d6a112233445566ULL;
    return Value;
}
TrajectorySample Sample(double TimeS,double X_M,double Y_M,double Z_M)
{
    TrajectorySample Value;
    Value.SimTimeS=TimeS;
    Value.Position={X_M,Y_M,Z_M};
    Value.Attitude={0.0,0.0,0.0};
    return Value;
}
DrillDefinition MakeHoverDefinition()
{
    DrillDefinition Value;
    Value.Kind=DrillKind::HoverBoxHold;
    Value.MaximumSampleGapS=1.00;
    Value.Hover.CentreM={0.0,0.0,1.0};
    Value.Hover.HorizontalRadiusM=0.50;
    Value.Hover.MinimumAltitudeM=0.80;
    Value.Hover.MaximumAltitudeM=1.20;
    Value.Hover.HoldTimeS=1.00;
    return Value;
}
DrillDefinition MakeGateDefinition()
{
    DrillDefinition Value;
    Value.Kind=DrillKind::GateAccuracy;
    Value.MaximumSampleGapS=1.00;
    Value.Gate.Gate.CentreM={0.0,0.0,1.0};
    Value.Gate.Gate.NormalUnit={1.0,0.0,0.0};
    Value.Gate.Gate.MaximumOffsetM=0.25;
    Value.Gate.Gate.Direction=PassDirection::NegativeToPositive;
    return Value;
}
DrillDefinition MakeSegmentDefinition()
{
    DrillDefinition Value;
    Value.Kind=DrillKind::SegmentSpeed;
    Value.MaximumSampleGapS=1.00;
    Value.Segment.Start.CentreM={0.0,0.0,1.0};
    Value.Segment.Start.NormalUnit={1.0,0.0,0.0};
    Value.Segment.Start.MaximumOffsetM=0.50;
    Value.Segment.Start.Direction=PassDirection::NegativeToPositive;
    Value.Segment.End.CentreM={10.0,0.0,1.0};
    Value.Segment.End.NormalUnit={1.0,0.0,0.0};
    Value.Segment.End.MaximumOffsetM=0.50;
    Value.Segment.End.Direction=PassDirection::NegativeToPositive;
    Value.Segment.TargetTimeS=5.00;
    return Value;
}
AttemptResult RunHoverAtFps(int FramesPerSecond)
{
    GoalEvaluator Evaluator;
    const DrillDefinition Definition=MakeHoverDefinition();
    const Rejection BeginResult=Evaluator.Begin(Definition,MakeCondition(),100U+static_cast<std::uint64_t>(FramesPerSecond));
    if(BeginResult!=Rejection::None)
        return Evaluator.GetResult();
    for(int Frame=0;Frame<=FramesPerSecond;++Frame)
    {
        const double TimeS=static_cast<double>(Frame)/static_cast<double>(FramesPerSecond);
        const Rejection AddResult=Evaluator.AddSample(Sample(TimeS,0.0,0.0,1.0));
        if(AddResult!=Rejection::None)
            break;
    }
    return Evaluator.Finish();
}
AttemptResult RunGateAtFps(int FramesPerSecond)
{
    GoalEvaluator Evaluator;
    const DrillDefinition Definition=MakeGateDefinition();
    const Rejection BeginResult=Evaluator.Begin(Definition,MakeCondition(),200U+static_cast<std::uint64_t>(FramesPerSecond));
    if(BeginResult!=Rejection::None)
        return Evaluator.GetResult();
    for(int Frame=0;Frame<=FramesPerSecond;++Frame)
    {
        const double TimeS=static_cast<double>(Frame)/static_cast<double>(FramesPerSecond);
        const double X_M=-1.0+2.0*TimeS;
        const Rejection AddResult=Evaluator.AddSample(Sample(TimeS,X_M,0.25,1.0));
        if(AddResult!=Rejection::None||Evaluator.GetResult().State!=AttemptState::Active)
            break;
    }
    return Evaluator.Finish();
}
AttemptResult RunSegmentAtFps(int FramesPerSecond)
{
    GoalEvaluator Evaluator;
    const DrillDefinition Definition=MakeSegmentDefinition();
    const Rejection BeginResult=Evaluator.Begin(Definition,MakeCondition(),300U+static_cast<std::uint64_t>(FramesPerSecond));
    if(BeginResult!=Rejection::None)
        return Evaluator.GetResult();
    const int FrameCount=FramesPerSecond*6;
    for(int Frame=0;Frame<=FrameCount;++Frame)
    {
        const double TimeS=static_cast<double>(Frame)/static_cast<double>(FramesPerSecond);
        const double X_M=-1.0+2.0*TimeS;
        const Rejection AddResult=Evaluator.AddSample(Sample(TimeS,X_M,0.50,1.0));
        if(AddResult!=Rejection::None||Evaluator.GetResult().State!=AttemptState::Active)
            break;
    }
    return Evaluator.Finish();
}
AttemptResult MakeHistoryResult(std::uint64_t AttemptId,AttemptState State,bool Success,bool Automatic,
    double MetricValue,const SameConditionKey& Condition)
{
    AttemptResult Value;
    Value.State=State;
    Value.Reason=State==AttemptState::Failed?Rejection::GoalNotMet:Rejection::None;
    Value.Kind=DrillKind::GateAccuracy;
    Value.Condition=Condition;
    Value.AttemptId=AttemptId;
    Value.AutomaticTest=Automatic;
    Value.Success=Success;
    Value.HasMetric=true;
    Value.Metric=MetricKind::GateOffsetM;
    Value.MetricValue=MetricValue;
    return Value;
}
}

int main()
{
    const SameConditionKey Condition=MakeCondition();
    Expect(ValidateConditionKey(Condition)==Rejection::None,"valid same-condition key is accepted");
    SameConditionKey InvalidCondition=Condition;
    InvalidCondition.DrillVersion=0U;
    Expect(ValidateConditionKey(InvalidCondition)==Rejection::InvalidConditionKey,"zero drill version is rejected");
    InvalidCondition=Condition;
    InvalidCondition.GravityMps2=std::numeric_limits<double>::quiet_NaN();
    Expect(ValidateConditionKey(InvalidCondition)==Rejection::InvalidConditionKey,"non-finite gravity is rejected");
    InvalidCondition=Condition;
    InvalidCondition.AssistOptionsMask=0x80000000U;
    Expect(ValidateConditionKey(InvalidCondition)==Rejection::InvalidConditionKey,"unknown assist option is rejected");
    InvalidCondition=Condition;
    InvalidCondition.Input=static_cast<InputMode>(99);
    Expect(ValidateConditionKey(InvalidCondition)==Rejection::InvalidConditionKey,"unknown input mode is rejected");

    DrillDefinition InvalidDefinition=MakeHoverDefinition();
    InvalidDefinition.Hover.HorizontalRadiusM=0.0;
    Expect(ValidateDefinition(InvalidDefinition)==Rejection::InvalidDefinition,"zero hover radius is rejected");
    InvalidDefinition=MakeHoverDefinition();
    InvalidDefinition.MaximumSampleGapS=1.01;
    Expect(ValidateDefinition(InvalidDefinition)==Rejection::InvalidDefinition,"oversized allowed sample gap is rejected");
    InvalidDefinition=MakeGateDefinition();
    InvalidDefinition.Gate.Gate.NormalUnit={0.0,0.0,0.0};
    Expect(ValidateDefinition(InvalidDefinition)==Rejection::InvalidDefinition,"zero gate normal is rejected");
    InvalidDefinition=MakeSegmentDefinition();
    InvalidDefinition.Segment.End.CentreM=InvalidDefinition.Segment.Start.CentreM;
    Expect(ValidateDefinition(InvalidDefinition)==Rejection::InvalidDefinition,"zero-length saved segment is rejected");

    {
        GoalEvaluator Evaluator;
        const DrillDefinition Definition=MakeHoverDefinition();
        Expect(Evaluator.Begin(Definition,Condition,1U)==Rejection::None,"hover attempt begins with valid definition");
        Expect(Evaluator.AddSample(Sample(0.0,0.50,0.0,0.80))==Rejection::None,"hover lower altitude and radius boundary sample is accepted");
        Expect(Evaluator.AddSample(Sample(0.50,0.50,0.0,1.20))==Rejection::None,"hover upper altitude boundary sample is accepted");
        Expect(Evaluator.GetResult().State==AttemptState::Active,"hover stays active before full elapsed hold");
        Expect(Evaluator.AddSample(Sample(1.00,0.50,0.0,1.20))==Rejection::None,"hover final dwell sample is accepted");
        const AttemptResult Result=Evaluator.Finish();
        Expect(Result.State==AttemptState::Succeeded&&Result.Success,"hover succeeds at exact required duration");
        Expect(Result.HasMetric&&Result.Metric==MetricKind::HoldTimeS&&Near(Result.MetricValue,1.0),"hover result records elapsed hold seconds");
        Expect(Evaluator.AddSample(Sample(1.05,0.0,0.0,1.0))==Rejection::AttemptFinished,"finished hover cannot accept another sample");
    }
    {
        GoalEvaluator Evaluator;
        const DrillDefinition Definition=MakeHoverDefinition();
        Expect(Evaluator.Begin(Definition,Condition,2U)==Rejection::None,"hover reset test begins");
        Evaluator.AddSample(Sample(0.0,0.0,0.0,1.0));
        Evaluator.AddSample(Sample(0.50,0.0,0.0,1.0));
        Evaluator.AddSample(Sample(0.60,0.51,0.0,1.0));
        Evaluator.AddSample(Sample(1.00,0.0,0.0,1.0));
        const AttemptResult Result=Evaluator.Finish();
        Expect(Result.State==AttemptState::Failed&&Result.Reason==Rejection::GoalNotMet,"leaving the hover box resets the continuous hold");
        Expect(Result.HasMetric&&Near(Result.MetricValue,0.50),"failed hover keeps its best continuous hold metric");
    }
    {
        GoalEvaluator Evaluator;
        DrillDefinition Definition=MakeHoverDefinition();
        Definition.MaximumSampleGapS=0.20;
        Expect(Evaluator.Begin(Definition,Condition,3U)==Rejection::None,"sample gap test begins");
        Evaluator.AddSample(Sample(0.0,0.0,0.0,1.0));
        Expect(Evaluator.AddSample(Sample(0.21,0.0,0.0,1.0))==Rejection::SampleGapTooLarge,"gap above saved maximum invalidates attempt");
        Expect(Evaluator.GetResult().State==AttemptState::Invalidated,"large gap has invalidated state");
    }
    {
        GoalEvaluator Evaluator;
        Expect(Evaluator.Begin(MakeHoverDefinition(),Condition,4U)==Rejection::None,"backward-time test begins");
        Evaluator.AddSample(Sample(1.0,0.0,0.0,1.0));
        Expect(Evaluator.AddSample(Sample(0.9,0.0,0.0,1.0))==Rejection::TimeNotIncreasing,"time going backward invalidates attempt");
    }
    {
        GoalEvaluator Evaluator;
        Expect(Evaluator.Begin(MakeHoverDefinition(),Condition,5U)==Rejection::None,"non-finite sample test begins");
        TrajectorySample InvalidSample=Sample(0.0,0.0,0.0,1.0);
        InvalidSample.Position.X_M=std::numeric_limits<double>::infinity();
        Expect(Evaluator.AddSample(InvalidSample)==Rejection::NonFiniteSample,"non-finite position invalidates attempt");
        Expect(Evaluator.GetResult().State==AttemptState::Invalidated,"non-finite input never awards progress");
    }
    {
        GoalEvaluator Evaluator;
        Expect(Evaluator.Begin(MakeHoverDefinition(),Condition,6U)==Rejection::None,"attitude-range test begins");
        TrajectorySample InvalidSample=Sample(0.0,0.0,0.0,1.0);
        InvalidSample.Attitude.YawDeg=721.0;
        Expect(Evaluator.AddSample(InvalidSample)==Rejection::SampleOutOfRange,"out-of-range attitude invalidates attempt");
    }

    {
        GoalEvaluator Evaluator;
        Expect(Evaluator.Begin(MakeGateDefinition(),Condition,10U)==Rejection::None,"gate sweep test begins");
        Expect(Evaluator.AddSample(Sample(0.0,-1.0,0.25,1.0))==Rejection::None,"gate sweep first endpoint is accepted");
        Expect(Evaluator.AddSample(Sample(1.0,1.0,0.25,1.0))==Rejection::None,"swept gate crossing is accepted");
        const AttemptResult Result=Evaluator.Finish();
        Expect(Result.State==AttemptState::Succeeded&&Near(Result.CompletionTimeS,0.50),"gate success uses interpolated plane-crossing time");
        Expect(Result.HasMetric&&Near(Result.MetricValue,0.25),"gate boundary offset is accepted and recorded");
    }
    {
        GoalEvaluator Evaluator;
        Expect(Evaluator.Begin(MakeGateDefinition(),Condition,11U)==Rejection::None,"reverse gate test begins");
        Evaluator.AddSample(Sample(0.0,1.0,0.0,1.0));
        Evaluator.AddSample(Sample(1.0,-1.0,0.0,1.0));
        const AttemptResult Result=Evaluator.Finish();
        Expect(Result.State==AttemptState::Failed&&Result.Reason==Rejection::GoalNotMet,"wrong-direction gate crossing is not accepted");
    }
    {
        GoalEvaluator Evaluator;
        Expect(Evaluator.Begin(MakeGateDefinition(),Condition,12U)==Rejection::None,"outside gate test begins");
        Evaluator.AddSample(Sample(0.0,-1.0,0.25001,1.0));
        Evaluator.AddSample(Sample(1.0,1.0,0.25001,1.0));
        const AttemptResult Result=Evaluator.Finish();
        Expect(Result.State==AttemptState::Failed,"offset just outside gate tolerance does not pass");
    }

    const std::array<int,3> FrameRates{{20,60,240}};
    std::array<AttemptResult,3> HoverResults{};
    std::array<AttemptResult,3> GateResults{};
    std::array<AttemptResult,3> SegmentResults{};
    for(std::size_t Index=0U;Index<FrameRates.size();++Index)
    {
        HoverResults[Index]=RunHoverAtFps(FrameRates[Index]);
        GateResults[Index]=RunGateAtFps(FrameRates[Index]);
        SegmentResults[Index]=RunSegmentAtFps(FrameRates[Index]);
        Expect(HoverResults[Index].State==AttemptState::Succeeded&&Near(HoverResults[Index].MetricValue,1.0),"hover goal is elapsed-time independent at 20/60/240 FPS");
        Expect(GateResults[Index].State==AttemptState::Succeeded&&Near(GateResults[Index].CompletionTimeS,0.50)&&Near(GateResults[Index].MetricValue,0.25),"gate sweep is interpolation-consistent at 20/60/240 FPS");
        Expect(SegmentResults[Index].State==AttemptState::Succeeded&&Near(SegmentResults[Index].MetricValue,5.0),"segment speed is interpolation-consistent at 20/60/240 FPS");
    }
    Expect(Near(HoverResults[0].MetricValue,HoverResults[1].MetricValue)&&Near(HoverResults[1].MetricValue,HoverResults[2].MetricValue),"hover metrics match across all requested frame rates");
    Expect(Near(GateResults[0].CompletionTimeS,GateResults[1].CompletionTimeS)&&Near(GateResults[1].CompletionTimeS,GateResults[2].CompletionTimeS),"gate crossing times match across all requested frame rates");
    Expect(Near(SegmentResults[0].MetricValue,SegmentResults[1].MetricValue)&&Near(SegmentResults[1].MetricValue,SegmentResults[2].MetricValue),"segment elapsed times match across all requested frame rates");

    {
        GoalEvaluator Evaluator;
        Expect(Evaluator.Begin(MakeSegmentDefinition(),Condition,20U)==Rejection::None,"saved segment begins");
        Evaluator.AddSample(Sample(0.0,-1.0,0.0,1.0));
        Expect(Evaluator.AddSample(Sample(6.0,11.0,0.0,1.0))==Rejection::SampleGapTooLarge,"segment also rejects unsafely sparse trajectory input");
    }
    {
        DrillDefinition Definition=MakeSegmentDefinition();
        Definition.MaximumSampleGapS=1.0;
        GoalEvaluator Evaluator;
        Expect(Evaluator.Begin(Definition,Condition,21U)==Rejection::None,"single-interval segment crossing begins");
        Evaluator.AddSample(Sample(0.0,-1.0,0.0,1.0));
        Expect(Evaluator.AddSample(Sample(1.0,11.0,0.0,1.0))==Rejection::None,"one swept segment sample finds both checkpoints in order");
        const AttemptResult Result=Evaluator.Finish();
        Expect(Result.State==AttemptState::Succeeded&&Near(Result.MetricValue,5.0/6.0),"single swept interval uses both interpolated checkpoint times");
    }
    {
        DrillDefinition Definition=MakeSegmentDefinition();
        Definition.MaximumSampleGapS=1.0;
        Definition.Segment.TargetTimeS=0.80;
        GoalEvaluator Evaluator;
        Expect(Evaluator.Begin(Definition,Condition,22U)==Rejection::None,"slow segment test begins");
        Evaluator.AddSample(Sample(0.0,-1.0,0.0,1.0));
        Evaluator.AddSample(Sample(1.0,11.0,0.0,1.0));
        const AttemptResult Result=Evaluator.Finish();
        Expect(Result.State==AttemptState::Failed&&Result.HasMetric&&Near(Result.MetricValue,5.0/6.0),"segment over its time target fails with measured elapsed seconds");
    }
    {
        DrillDefinition Definition=MakeSegmentDefinition();
        Definition.MaximumSampleGapS=1.0;
        Definition.Segment.Start.Direction=PassDirection::Any;
        Definition.Segment.End.Direction=PassDirection::Any;
        GoalEvaluator Evaluator;
        Expect(Evaluator.Begin(Definition,Condition,23U)==Rejection::None,"end-before-start segment test begins");
        Evaluator.AddSample(Sample(0.0,11.0,0.0,1.0));
        Evaluator.AddSample(Sample(1.0,-1.0,0.0,1.0));
        const AttemptResult Result=Evaluator.Finish();
        Expect(Result.State==AttemptState::Failed,"end checkpoint before user-marked start does not complete segment");
    }

    const AttemptResult Reference=RunGateAtFps(60);
    AttemptResult Candidate=Reference;
    Candidate.AttemptId=999U;
    Expect(CanCompare(Reference,Candidate).Eligible,"two completed manual same-condition attempts are comparable");
    Candidate=Reference;
    Expect(CanCompare(Reference,Candidate).Reason==ComparisonBlockReason::SameAttempt,"same saved attempt cannot compare with itself");
    Candidate=Reference;
    Candidate.AttemptId=999U;
    Candidate.AutomaticTest=true;
    Expect(CanCompare(Reference,Candidate).Reason==ComparisonBlockReason::AutomaticTestSession,"automatic-test sessions are blocked from comparison");
    Candidate=Reference;
    Candidate.AttemptId=999U;
    Candidate.State=AttemptState::Aborted;
    Expect(CanCompare(Reference,Candidate).Reason==ComparisonBlockReason::AbortedSession,"aborted sessions are blocked from comparison");
    Candidate=Reference;
    Candidate.AttemptId=999U;
    Candidate.Condition.DrillVersion+=1U;
    Expect(CanCompare(Reference,Candidate).Reason==ComparisonBlockReason::DifferentCondition,"different drill version blocks comparison");
    Candidate=Reference;
    Candidate.AttemptId=999U;
    Candidate.Condition.PhysicsProfileId+=1U;
    Expect(CanCompare(Reference,Candidate).Reason==ComparisonBlockReason::DifferentCondition,"different physics profile blocks comparison");
    Candidate=Reference;
    Candidate.AttemptId=999U;
    Candidate.Condition.GravityMps2=8.0;
    Expect(CanCompare(Reference,Candidate).Reason==ComparisonBlockReason::DifferentCondition,"different gravity blocks comparison");
    Candidate=Reference;
    Candidate.AttemptId=999U;
    Candidate.Condition.AssistOptionsMask^=0x00000001U;
    Expect(CanCompare(Reference,Candidate).Reason==ComparisonBlockReason::DifferentCondition,"different assist setting blocks comparison");
    Candidate=Reference;
    Candidate.AttemptId=999U;
    Candidate.Condition.Input=InputMode::ControllerMode2;
    Expect(CanCompare(Reference,Candidate).Reason==ComparisonBlockReason::DifferentCondition,"different input mode blocks comparison");
    Candidate=Reference;
    Candidate.AttemptId=999U;
    Candidate.Condition.BuildFingerprint+=1U;
    Expect(CanCompare(Reference,Candidate).Reason==ComparisonBlockReason::DifferentCondition,"different build fingerprint blocks comparison");

    std::array<AttemptResult,5> History{{
        MakeHistoryResult(400U,AttemptState::Succeeded,true,false,0.25,Condition),
        MakeHistoryResult(401U,AttemptState::Succeeded,true,false,0.20,Condition),
        MakeHistoryResult(402U,AttemptState::Failed,false,false,0.70,Condition),
        MakeHistoryResult(403U,AttemptState::Succeeded,true,true,0.10,Condition),
        MakeHistoryResult(404U,AttemptState::Succeeded,true,false,0.05,SameConditionKey{8U,42U,9.80665,0x00000005U,InputMode::Keyboard,0x8d6a112233445566ULL})
    }};
    const SummaryBuildResult SummaryResult=BuildProgressSummary(History.data(),History.size(),3U,Condition,DrillKind::GateAccuracy);
    Expect(SummaryResult.Reason==Rejection::None,"same-condition manual history summary is accepted");
    Expect(SummaryResult.Summary.AttemptCount==3U&&SummaryResult.Summary.SuccessCount==2U,"progress exposes success and attempt counts without a percentage");
    Expect(SummaryResult.Summary.HasBest&&Near(SummaryResult.Summary.BestMetric,0.20),"gate best uses lower offset as better");
    Expect(SummaryResult.Summary.HasMedian&&SummaryResult.Summary.MedianWindowCount==3U&&Near(SummaryResult.Summary.MedianOfLastNMetric,0.25),"median uses the last N eligible metric records");
    Expect(BuildProgressSummary(nullptr,1U,3U,Condition,DrillKind::GateAccuracy).Reason==Rejection::InvalidHistory,"null nonempty history is rejected");
    Expect(BuildProgressSummary(History.data(),History.size(),0U,Condition,DrillKind::GateAccuracy).Reason==Rejection::InvalidMedianWindow,"zero median window is rejected");

    AttemptResult NewBest=MakeHistoryResult(405U,AttemptState::Succeeded,true,false,0.10,Condition);
    Expect(IsNewBest(NewBest,SummaryResult.Summary),"strictly lower gate offset is a new best");
    NewBest.MetricValue=0.20;
    Expect(!IsNewBest(NewBest,SummaryResult.Summary),"tied metric is not announced as a new best");
    const AudioCueIds Cues{false,10U,20U,30U};
    const AudioCueDecision NewBestCue=SelectAudioCue(Cues,true,true);
    Expect(NewBestCue.ShouldPlay&&NewBestCue.Kind==AudioCueKind::NewBest&&NewBestCue.CueId==30U,"new-best cue has priority over success cue");
    const AudioCueDecision FailureCue=SelectAudioCue(Cues,false,false);
    Expect(FailureCue.ShouldPlay&&FailureCue.Kind==AudioCueKind::Failure&&FailureCue.CueId==20U,"failure cue stays optional by id");
    AudioCueIds MutedCues=Cues;
    MutedCues.Muted=true;
    Expect(!SelectAudioCue(MutedCues,true,true).ShouldPlay,"mute prevents all optional feedback cues");

    std::cout<<"Pure C++ trajectory-goal checks only; no Unreal build, runtime, controller, network, physical flight, other-PC, or human-playtest claim\n";
    std::cout<<Checks<<" checks, "<<Failures<<" failures\n";
    return Failures==0?0:1;
}
