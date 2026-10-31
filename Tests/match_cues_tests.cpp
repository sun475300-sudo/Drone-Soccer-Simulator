#include "MatchPresentationCues.h"
#include <array>
#include <iostream>
#include <limits>

namespace
{
using namespace drone_soccer;
using namespace ImperialMatchCues;

int Checks=0;
int Failures=0;

void Expect(bool Good,const char* Name)
{
    ++Checks;
    if(!Good)
    {
        ++Failures;
        std::cerr<<"FAIL "<<Name<<'\n';
    }
}

std::array<Vec3,10> Homes()
{
    std::array<Vec3,10> Positions{};
    for(int Index=0;Index<10;++Index)
    {
        Positions[static_cast<std::size_t>(Index)]={Index<5?-6.5:6.5,-2.4+static_cast<double>(Index%5)*1.2,0.2};
    }
    return Positions;
}

FrameResult Observe(Generator& Cues,const MatchRules& Rules,std::uint64_t Epoch=0U)
{
    return Cues.Observe(ReadSnapshot(Rules),Epoch);
}

void UpdateAt(MatchRules& Rules,std::array<Vec3,10>& Positions,int DroneId,double X,double Dt=0.01)
{
    Positions[static_cast<std::size_t>(DroneId)]={X,0.0,3.0};
    Rules.Update(Positions,Dt);
}

int CountType(const FrameResult& Result,CueType Type)
{
    int Count=0;
    for(std::size_t Index=0U;Index<Result.CueCount;++Index)
    {
        if(Result.Cues[Index].Type==Type)++Count;
    }
    return Count;
}

bool HasCue(const FrameResult& Result,CueType Type,int TeamIndex=-2,int CountdownSecond=-1)
{
    for(std::size_t Index=0U;Index<Result.CueCount;++Index)
    {
        const Cue& Value=Result.Cues[Index];
        if(Value.Type==Type&&(TeamIndex==-2||Value.TeamIndex==TeamIndex)&&
            (CountdownSecond==-1||Value.CountdownSecond==CountdownSecond))return true;
    }
    return false;
}

std::array<int,11> CollectCountdownsAtFps(int Fps)
{
    MatchRules Rules;
    std::array<Vec3,10> Positions=Homes();
    Generator Cues;
    static_cast<void>(Observe(Cues,Rules));
    Rules.StartMatch();
    static_cast<void>(Observe(Cues,Rules));
    Rules.Update(Positions,10.0);
    static_cast<void>(Observe(Cues,Rules));
    Rules.Update(Positions,169.9);
    static_cast<void>(Observe(Cues,Rules));

    std::array<int,11> Hits{};
    const double Dt=1.0/static_cast<double>(Fps);
    const int StepCount=Fps*11;
    for(int Step=0;Step<StepCount;++Step)
    {
        Rules.Update(Positions,Dt);
        const FrameResult Result=Observe(Cues,Rules);
        for(std::size_t Index=0U;Index<Result.CueCount;++Index)
        {
            const Cue& Value=Result.Cues[Index];
            if(Value.Type==CueType::CountdownAudio&&Value.CountdownSecond>=1&&Value.CountdownSecond<=10)
                ++Hits[static_cast<std::size_t>(Value.CountdownSecond)];
        }
    }
    return Hits;
}

bool EverySecondExactlyOnce(const std::array<int,11>& Hits)
{
    for(int Second=1;Second<=10;++Second)
    {
        if(Hits[static_cast<std::size_t>(Second)]!=1)return false;
    }
    return true;
}

void TestApprovedGoalsAndRejectedTransits()
{
    {
        MatchRules Rules;
        std::array<Vec3,10> Positions=Homes();
        Generator Cues;
        const FrameResult Initial=Observe(Cues,Rules);
        Expect(Initial.BaselineCaptured&&Initial.CueCount==0U,"practice snapshot establishes a silent baseline");
        UpdateAt(Rules,Positions,0,6.1);
        static_cast<void>(Observe(Cues,Rules));
        UpdateAt(Rules,Positions,0,6.9);
        const FrameResult Goal=Observe(Cues,Rules);
        Expect(Rules.Score(0)==1,"real approved blue striker goal increments MatchRules score");
        Expect(Goal.CueCount==1U&&HasCue(Goal,CueType::GoalFlash,0),"one approved blue score creates exactly one team-colour flash");
        Expect(Goal.Cues[0].DurationS==0.75,"default goal flash uses the documented simulator duration");
        Rules.Update(Positions,0.01);
        const FrameResult Duplicate=Observe(Cues,Rules);
        Expect(Duplicate.CueCount==0U,"unchanged approved score never repeats a goal flash");
    }
    {
        MatchRules Rules;
        std::array<Vec3,10> Positions=Homes();
        Generator Cues;
        static_cast<void>(Observe(Cues,Rules));
        UpdateAt(Rules,Positions,5,-6.1);
        static_cast<void>(Observe(Cues,Rules));
        UpdateAt(Rules,Positions,5,-6.9);
        const FrameResult Goal=Observe(Cues,Rules);
        Expect(Rules.Score(1)==1&&HasCue(Goal,CueType::GoalFlash,1),"real mirrored red score retains red team index");
    }
    {
        MatchRules Rules;
        std::array<Vec3,10> Positions=Homes();
        Generator Cues;
        static_cast<void>(Observe(Cues,Rules));
        UpdateAt(Rules,Positions,0,6.9);
        static_cast<void>(Observe(Cues,Rules));
        UpdateAt(Rules,Positions,0,6.1);
        const FrameResult Reverse=Observe(Cues,Rules);
        Expect(Rules.Score(0)==0&&Rules.IsOffside(0),"MatchRules rejects reverse opponent-goal transit and records offside");
        Expect(Reverse.CueCount==0U,"reverse transit has no presentation goal cue");
    }
    {
        MatchRules Rules;
        std::array<Vec3,10> Positions=Homes();
        Generator Cues;
        static_cast<void>(Observe(Cues,Rules));
        UpdateAt(Rules,Positions,0,6.1);
        static_cast<void>(Observe(Cues,Rules));
        UpdateAt(Rules,Positions,0,6.6);
        const FrameResult Partial=Observe(Cues,Rules);
        UpdateAt(Rules,Positions,0,6.1);
        const FrameResult Bounce=Observe(Cues,Rules);
        Expect(Rules.Score(0)==0,"MatchRules rejects partial goal passage and bounce-back");
        Expect(Partial.CueCount==0U&&Bounce.CueCount==0U,"partial passage and bounce-back produce no cue");
    }
    {
        MatchRules Rules;
        std::array<Vec3,10> Positions=Homes();
        Generator Cues;
        static_cast<void>(Observe(Cues,Rules));
        UpdateAt(Rules,Positions,0,6.1);
        static_cast<void>(Observe(Cues,Rules));
        UpdateAt(Rules,Positions,0,6.9);
        const FrameResult FirstGoal=Observe(Cues,Rules);
        UpdateAt(Rules,Positions,0,6.1);
        static_cast<void>(Observe(Cues,Rules));
        UpdateAt(Rules,Positions,0,6.9);
        const FrameResult OffsideRepeat=Observe(Cues,Rules);
        Expect(CountType(FirstGoal,CueType::GoalFlash)==1,"first approved score has one flash before offside repeat");
        Expect(Rules.Score(0)==1&&Rules.Penalties(0)==1,"real offside repeat is a penalty rather than a second score");
        Expect(OffsideRepeat.CueCount==0U,"offside repeat has no goal cue");
    }
}

void TestScoreDiscontinuitiesAndOptions()
{
    Snapshot Base{};
    Snapshot Advanced{};
    Advanced.Scores[0]=3;
    {
        Generator Cues;
        static_cast<void>(Cues.Observe(Base,0U));
        const FrameResult Jump=Cues.Observe(Advanced,0U);
        Expect(Jump.ScoreJumpAnomaly&&CountType(Jump,CueType::GoalFlash)==3,"multi-point score jump is flagged and emits one flash per increment");
        Expect(HasCue(Jump,CueType::GoalFlash,0),"multi-point jump keeps the scoring team index");
        Advanced.Scores[0]=2;
        const FrameResult Decrease=Cues.Observe(Advanced,0U);
        Expect(Decrease.ResetApplied&&Decrease.ScoreDecreaseReset&&Decrease.CueCount==0U,"score decrease resets baselines without a stale cue");
    }
    {
        Generator Cues;
        static_cast<void>(Cues.Observe(Base,40U));
        Snapshot ResetState=Base;
        ResetState.Scores[1]=1;
        const FrameResult Reset=Cues.Observe(ResetState,41U);
        Expect(Reset.ResetApplied&&!Reset.ScoreDecreaseReset&&Reset.CueCount==0U,"caller reset epoch suppresses cross-session score cues");
        const FrameResult Stable=Cues.Observe(ResetState,41U);
        Expect(Stable.CueCount==0U,"reset baseline does not invent a later cue");
    }
    {
        Generator Cues;
        Options Invalid{};
        Invalid.GoalFlashDurationS=std::numeric_limits<double>::quiet_NaN();
        Expect(Cues.Configure(Invalid)==Rejection::FlashDurationNotFinite,"non-finite flash duration is rejected explicitly");
        Invalid.GoalFlashDurationS=0.0;
        Expect(Cues.Configure(Invalid)==Rejection::FlashDurationOutOfRange,"zero flash duration is rejected explicitly");
        Invalid.GoalFlashDurationS=10.01;
        Expect(Cues.Configure(Invalid)==Rejection::FlashDurationOutOfRange,"overlong flash duration is rejected explicitly");
        Options Reduced{};
        Reduced.GoalFlashDurationS=1.25;
        Reduced.ReducedEffects=true;
        Expect(Cues.Configure(Reduced)==Rejection::None,"valid reduced-effect options are accepted");
        static_cast<void>(Cues.Observe(Base,0U));
        Snapshot Scored=Base;
        Scored.Scores[1]=1;
        const FrameResult Steady=Cues.Observe(Scored,0U);
        Expect(HasCue(Steady,CueType::GoalSteadyHighlight,1)&&Steady.Cues[0].DurationS==1.25,"reduced effects changes flash to configured steady highlight");
    }
}

void TestPhaseNoticesAndLastTenSeconds()
{
    MatchRules Rules;
    std::array<Vec3,10> Positions=Homes();
    Generator Cues;
    static_cast<void>(Observe(Cues,Rules));
    Rules.StartMatch();
    const FrameResult Ready=Observe(Cues,Rules);
    Expect(Ready.CueCount==1U&&HasCue(Ready,CueType::NoticeReady),"practice-to-ready produces a ready notice");
    Rules.Update(Positions,10.0);
    const FrameResult MatchStart=Observe(Cues,Rules);
    Expect(Rules.GetPhase()==Phase::Playing&&HasCue(MatchStart,CueType::NoticeMatchStart),"real ready clock transition produces match-start notice");
    Rules.Update(Positions,180.0);
    const FrameResult Break=Observe(Cues,Rules);
    Expect(Rules.GetPhase()==Phase::Break&&HasCue(Break,CueType::NoticeSetEndBreak),"real set end produces one set-end/break notice");
    Rules.Update(Positions,300.0);
    const FrameResult NextReady=Observe(Cues,Rules);
    Expect(Rules.CurrentSet()==2&&Rules.GetPhase()==Phase::Ready&&HasCue(NextReady,CueType::NoticeReady),"real break transition produces next-set ready notice");
    Rules.Update(Positions,10.0);
    const FrameResult SetStart=Observe(Cues,Rules);
    Expect(Rules.GetPhase()==Phase::Playing&&HasCue(SetStart,CueType::NoticeSetStart),"second real set start produces set-start notice");
    Rules.Update(Positions,1000000000000.0);
    const FrameResult Finished=Observe(Cues,Rules);
    Expect(Rules.GetPhase()==Phase::Finished&&HasCue(Finished,CueType::NoticeMatchFinished),"large real clock advance still reports final match notice");

    MatchRules CountdownRules;
    std::array<Vec3,10> CountdownPositions=Homes();
    Generator CountdownCues;
    static_cast<void>(Observe(CountdownCues,CountdownRules));
    CountdownRules.StartMatch();
    static_cast<void>(Observe(CountdownCues,CountdownRules));
    CountdownRules.Update(CountdownPositions,10.0);
    static_cast<void>(Observe(CountdownCues,CountdownRules));
    CountdownRules.Update(CountdownPositions,169.95);
    const FrameResult BeforeTen=Observe(CountdownCues,CountdownRules);
    Expect(!BeforeTen.LastTenSecondsRedBorderActive,"red border stays off above ten remaining seconds");
    CountdownRules.Update(CountdownPositions,0.06);
    const FrameResult Ten=Observe(CountdownCues,CountdownRules);
    Expect(Ten.LastTenSecondsRedBorderActive&&HasCue(Ten,CueType::CountdownAudio,-2,10),"red border and ten-second audio start at the inclusive boundary");
    CountdownRules.Update(CountdownPositions,0.01);
    const FrameResult Repeat=Observe(CountdownCues,CountdownRules);
    Expect(CountType(Repeat,CueType::CountdownAudio)==0,"a repeated countdown interval does not duplicate audio");
    CountdownRules.Update(CountdownPositions,10.0);
    const FrameResult Ended=Observe(CountdownCues,CountdownRules);
    Expect(CountdownRules.GetPhase()==Phase::Break&&!Ended.LastTenSecondsRedBorderActive,"red border is removed immediately when MatchRules enters break");
}

void TestCountdownRatesAndMute()
{
    for(const int Fps:{20,60,240})
    {
        const std::array<int,11> Hits=CollectCountdownsAtFps(Fps);
        Expect(EverySecondExactlyOnce(Hits),"20/60/240 FPS countdown emits every whole second exactly once");
    }
    {
        MatchRules Rules;
        std::array<Vec3,10> Positions=Homes();
        Generator Cues;
        static_cast<void>(Observe(Cues,Rules));
        Rules.StartMatch();
        static_cast<void>(Observe(Cues,Rules));
        Rules.Update(Positions,10.0);
        static_cast<void>(Observe(Cues,Rules));
        Rules.Update(Positions,169.5);
        static_cast<void>(Observe(Cues,Rules));
        Rules.Update(Positions,9.6);
        const FrameResult Jump=Observe(Cues,Rules);
        Expect(CountType(Jump,CueType::CountdownAudio)==10,"large dt jump emits every crossed countdown id rather than skipping them");
        bool Descending=true;
        for(std::size_t Index=0U;Index<Jump.CueCount;++Index)
        {
            if(Jump.Cues[Index].CountdownSecond!=10-static_cast<int>(Index))Descending=false;
        }
        Expect(Descending,"large dt countdown ids are emitted in descending temporal order");
    }
    {
        MatchRules Rules;
        std::array<Vec3,10> Positions=Homes();
        Generator Cues;
        Options Muted{};
        Muted.Mute=true;
        Expect(Cues.Configure(Muted)==Rejection::None,"mute option is accepted");
        static_cast<void>(Observe(Cues,Rules));
        Rules.StartMatch();
        static_cast<void>(Observe(Cues,Rules));
        Rules.Update(Positions,10.0);
        static_cast<void>(Observe(Cues,Rules));
        Rules.Update(Positions,169.5);
        static_cast<void>(Observe(Cues,Rules));
        Rules.Update(Positions,9.6);
        const FrameResult MutedJump=Observe(Cues,Rules);
        Expect(CountType(MutedJump,CueType::CountdownAudio)==0&&CountType(MutedJump,CueType::CountdownVisual)==10,"mute replaces countdown audio with visual alternatives");
    }
}

void TestInvalidSnapshotsAndResetBorder()
{
    MatchRules Rules;
    Snapshot Valid=ReadSnapshot(Rules);
    Generator Cues;
    static_cast<void>(Cues.Observe(Valid,0U));
    Snapshot Invalid=Valid;
    Invalid.RemainingS=std::numeric_limits<double>::quiet_NaN();
    Expect(Cues.Observe(Invalid,0U).Reason==Rejection::RemainingSecondsNotFinite,"non-finite remaining seconds are rejected explicitly");
    Invalid=Valid;
    Invalid.Scores[0]=Generator::MaximumScore+1;
    Expect(Cues.Observe(Invalid,0U).Reason==Rejection::CounterOutOfRange,"out-of-range score is rejected explicitly");
    Invalid=Valid;
    Invalid.Penalties[1]=Generator::MaximumPenaltyCount+1;
    Expect(Cues.Observe(Invalid,0U).Reason==Rejection::CounterOutOfRange,"out-of-range penalty count is rejected explicitly");
    Invalid=Valid;
    Invalid.CurrentSet=4;
    Expect(Cues.Observe(Invalid,0U).Reason==Rejection::CurrentSetOutOfRange,"out-of-range current set is rejected explicitly");
    Invalid=Valid;
    Invalid.Phase=static_cast<Phase>(99);
    Expect(Cues.Observe(Invalid,0U).Reason==Rejection::InvalidPhase,"unknown phase is rejected explicitly");
    Invalid=Valid;
    Invalid.Phase=Phase::Playing;
    Invalid.CurrentSet=0;
    Invalid.RemainingS=5.0;
    Expect(Cues.Observe(Invalid,0U).Reason==Rejection::InconsistentPhaseState,"inconsistent phase and set state is rejected explicitly");
    Snapshot Scored=Valid;
    Scored.Scores[0]=1;
    const FrameResult GoalAfterInvalid=Cues.Observe(Scored,0U);
    Expect(HasCue(GoalAfterInvalid,CueType::GoalFlash,0),"rejected snapshot does not corrupt the previous valid cue baseline");

    Snapshot FinalSeconds{};
    FinalSeconds.Phase=Phase::Playing;
    FinalSeconds.CurrentSet=1;
    FinalSeconds.RemainingS=5.0;
    Generator ResetCues;
    const FrameResult Initial=ResetCues.Observe(FinalSeconds,7U);
    Expect(Initial.LastTenSecondsRedBorderActive,"initial valid playing snapshot exposes last-ten border state");
    const FrameResult Reset=ResetCues.Observe(FinalSeconds,8U);
    Expect(Reset.ResetApplied&&!Reset.LastTenSecondsRedBorderActive&&Reset.CueCount==0U,"reset epoch removes last-ten border without an event cue");
    const FrameResult AfterReset=ResetCues.Observe(FinalSeconds,8U);
    Expect(AfterReset.LastTenSecondsRedBorderActive,"stable post-reset playing snapshot can expose current border state again");
}
}

int main()
{
    TestApprovedGoalsAndRejectedTransits();
    TestScoreDiscontinuitiesAndOptions();
    TestPhaseNoticesAndLastTenSeconds();
    TestCountdownRatesAndMute();
    TestInvalidSnapshotsAndResetBorder();
    std::cout<<"Scope: portable MatchRules presentation-cue state tests only; no Unreal build/runtime, physical controller, other-PC, network, or human playtest.\n";
    std::cout<<Checks<<" checks, "<<Failures<<" failures\n";
    return Failures==0?0:1;
}
