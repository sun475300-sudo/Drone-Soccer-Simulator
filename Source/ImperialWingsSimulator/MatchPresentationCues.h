#pragma once
#include "DroneSoccerRules.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// Presentation-only adapter for already approved MatchRules state. It never
// examines aircraft geometry or alters the rules core.
namespace ImperialMatchCues
{
enum class CueType
{
    GoalFlash,
    GoalSteadyHighlight,
    CountdownAudio,
    CountdownVisual,
    NoticeReady,
    NoticeMatchStart,
    NoticeSetStart,
    NoticeSetEndBreak,
    NoticeMatchFinished
};

enum class Rejection
{
    None,
    FlashDurationNotFinite,
    FlashDurationOutOfRange,
    RemainingSecondsNotFinite,
    RemainingSecondsOutOfRange,
    CounterOutOfRange,
    CurrentSetOutOfRange,
    InvalidPhase,
    InconsistentPhaseState
};

struct Snapshot
{
    drone_soccer::Phase Phase=drone_soccer::Phase::Practice;
    int CurrentSet=0;
    double RemainingS=0.0;
    std::array<int,2> Scores{};
    std::array<bool,2> Offside{};
    std::array<int,2> Penalties{};
    std::array<int,2> SetWins{};
};

inline Snapshot ReadSnapshot(const drone_soccer::MatchRules& Rules)
{
    Snapshot Value{};
    Value.Phase=Rules.GetPhase();
    Value.CurrentSet=Rules.CurrentSet();
    Value.RemainingS=Rules.Remaining();
    for(int TeamIndex=0;TeamIndex<2;++TeamIndex)
    {
        Value.Scores[static_cast<std::size_t>(TeamIndex)]=Rules.Score(TeamIndex);
        Value.Offside[static_cast<std::size_t>(TeamIndex)]=Rules.IsOffside(TeamIndex);
        Value.Penalties[static_cast<std::size_t>(TeamIndex)]=Rules.Penalties(TeamIndex);
        Value.SetWins[static_cast<std::size_t>(TeamIndex)]=Rules.SetWins(TeamIndex);
    }
    return Value;
}

struct Options
{
    // 시뮬레이터 선택값: renderer cue duration cap, not an official rule value.
    double GoalFlashDurationS=0.75;
    bool Mute=false;
    bool ReducedEffects=false;
};

struct Cue
{
    CueType Type=CueType::GoalFlash;
    int TeamIndex=-1;
    int CountdownSecond=0;
    double DurationS=0.0;
};

struct FrameResult
{
    Rejection Reason=Rejection::None;
    bool BaselineCaptured=false;
    bool ResetApplied=false;
    bool ScoreDecreaseReset=false;
    bool ScoreJumpAnomaly=false;
    bool LastTenSecondsRedBorderActive=false;
    std::size_t CueCount=0U;
    std::array<Cue,256> Cues{};
};

class Generator
{
public:
    // 시뮬레이터 선택값: compact scoreboard counters and per-frame cue capacity.
    static constexpr int MaximumScore=99;
    static constexpr int MaximumPenaltyCount=99;
    static constexpr std::size_t MaximumCuesPerFrame=256U;

    Rejection Configure(const Options& NewOptions)
    {
        const Rejection Reason=ValidateOptions(NewOptions);
        if(Reason!=Rejection::None)return Reason;
        CurrentOptions=NewOptions;
        return Rejection::None;
    }

    Options GetOptions() const { return CurrentOptions; }
    bool HasBaseline() const { return Initialized; }

    void Clear()
    {
        Initialized=false;
        CountdownEmittedMask=0U;
        Previous={};
        PreviousResetEpoch=0U;
    }

    FrameResult Observe(const Snapshot& Current,std::uint64_t ResetEpoch)
    {
        FrameResult Result{};
        const Rejection InputReason=ValidateSnapshot(Current);
        if(InputReason!=Rejection::None)
        {
            Result.Reason=InputReason;
            return Result;
        }
        if(!Initialized)
        {
            Capture(Current,ResetEpoch);
            Result.BaselineCaptured=true;
            Result.LastTenSecondsRedBorderActive=IsLastTenSeconds(Current);
            return Result;
        }
        if(ResetEpoch!=PreviousResetEpoch)
        {
            Capture(Current,ResetEpoch);
            Result.ResetApplied=true;
            return Result;
        }
        if(Current.Scores[0]<Previous.Scores[0]||Current.Scores[1]<Previous.Scores[1])
        {
            Capture(Current,ResetEpoch);
            Result.ResetApplied=true;
            Result.ScoreDecreaseReset=true;
            return Result;
        }

        AppendGoalCues(Current,Result);
        AppendCountdownCues(Current,Result);
        AppendPhaseNotice(Current,Result);
        Result.LastTenSecondsRedBorderActive=IsLastTenSeconds(Current);
        Capture(Current,ResetEpoch);
        return Result;
    }

private:
    Options CurrentOptions{};
    Snapshot Previous{};
    std::uint64_t PreviousResetEpoch=0U;
    std::uint16_t CountdownEmittedMask=0U;
    bool Initialized=false;

    static Rejection ValidateOptions(const Options& Value)
    {
        if(!std::isfinite(Value.GoalFlashDurationS))return Rejection::FlashDurationNotFinite;
        // 시뮬레이터 선택값: 0보다 크고 10초 이하만 HUD 연출에 사용한다.
        if(Value.GoalFlashDurationS<=0.0||Value.GoalFlashDurationS>10.0)return Rejection::FlashDurationOutOfRange;
        return Rejection::None;
    }

    static Rejection ValidateSnapshot(const Snapshot& Value)
    {
        if(!std::isfinite(Value.RemainingS))return Rejection::RemainingSecondsNotFinite;
        if(Value.RemainingS<0.0||Value.RemainingS>drone_soccer::RuleProfileFIDA40::BreakSeconds)
            return Rejection::RemainingSecondsOutOfRange;
        if(Value.CurrentSet<0||Value.CurrentSet>3)return Rejection::CurrentSetOutOfRange;
        for(int TeamIndex=0;TeamIndex<2;++TeamIndex)
        {
            const std::size_t Index=static_cast<std::size_t>(TeamIndex);
            if(Value.Scores[Index]<0||Value.Scores[Index]>MaximumScore||
                Value.Penalties[Index]<0||Value.Penalties[Index]>MaximumPenaltyCount||
                Value.SetWins[Index]<0||Value.SetWins[Index]>2)return Rejection::CounterOutOfRange;
        }
        switch(Value.Phase)
        {
        case drone_soccer::Phase::Practice:
            return Value.CurrentSet==0&&Value.RemainingS==0.0?Rejection::None:Rejection::InconsistentPhaseState;
        case drone_soccer::Phase::Ready:
            return Value.CurrentSet>=1&&Value.CurrentSet<=3&&
                Value.RemainingS<=drone_soccer::RuleProfileFIDA40::ReadySeconds?Rejection::None:Rejection::InconsistentPhaseState;
        case drone_soccer::Phase::Playing:
            return Value.CurrentSet>=1&&Value.CurrentSet<=3&&
                Value.RemainingS<=drone_soccer::RuleProfileFIDA40::SetSeconds?Rejection::None:Rejection::InconsistentPhaseState;
        case drone_soccer::Phase::Break:
            return Value.CurrentSet>=1&&Value.CurrentSet<=2&&
                Value.RemainingS<=drone_soccer::RuleProfileFIDA40::BreakSeconds?Rejection::None:Rejection::InconsistentPhaseState;
        case drone_soccer::Phase::Finished:
            return Value.CurrentSet>=1&&Value.CurrentSet<=3&&Value.RemainingS==0.0?Rejection::None:Rejection::InconsistentPhaseState;
        }
        return Rejection::InvalidPhase;
    }

    static bool IsLastTenSeconds(const Snapshot& Value)
    {
        return Value.Phase==drone_soccer::Phase::Playing&&Value.RemainingS>0.0&&Value.RemainingS<=10.0;
    }

    void Capture(const Snapshot& Value,std::uint64_t ResetEpoch)
    {
        const bool EnteredPlaying=Value.Phase==drone_soccer::Phase::Playing&&
            (!Initialized||Previous.Phase!=drone_soccer::Phase::Playing||Value.CurrentSet!=Previous.CurrentSet);
        Previous=Value;
        PreviousResetEpoch=ResetEpoch;
        Initialized=true;
        if(EnteredPlaying)CountdownEmittedMask=0U;
    }

    static std::uint16_t CountdownBit(int Second)
    {
        return static_cast<std::uint16_t>(1U<<static_cast<unsigned int>(Second-1));
    }

    static void Append(FrameResult& Result,const Cue& Value)
    {
        if(Result.CueCount<MaximumCuesPerFrame)
        {
            Result.Cues[Result.CueCount]=Value;
            ++Result.CueCount;
        }
    }

    void AppendGoalCues(const Snapshot& Current,FrameResult& Result) const
    {
        for(int TeamIndex=0;TeamIndex<2;++TeamIndex)
        {
            const std::size_t Index=static_cast<std::size_t>(TeamIndex);
            const int Increment=Current.Scores[Index]-Previous.Scores[Index];
            if(Increment<=0)continue;
            if(Increment>1)Result.ScoreJumpAnomaly=true;
            for(int Count=0;Count<Increment;++Count)
            {
                Cue Value{};
                Value.Type=CurrentOptions.ReducedEffects?CueType::GoalSteadyHighlight:CueType::GoalFlash;
                Value.TeamIndex=TeamIndex;
                Value.DurationS=CurrentOptions.GoalFlashDurationS;
                Append(Result,Value);
            }
        }
    }

    void AppendCountdownCues(const Snapshot& Current,FrameResult& Result)
    {
        if(Previous.Phase!=drone_soccer::Phase::Playing)return;
        const double EndRemainingS=Current.Phase==drone_soccer::Phase::Playing?Current.RemainingS:0.0;
        if(EndRemainingS>=Previous.RemainingS)return;
        for(int Second=10;Second>=1;--Second)
        {
            const double ThresholdS=static_cast<double>(Second);
            const std::uint16_t Bit=CountdownBit(Second);
            if(Previous.RemainingS>=ThresholdS&&EndRemainingS<=ThresholdS&&
                (CountdownEmittedMask&Bit)==0U)
            {
                Cue Value{};
                Value.Type=CurrentOptions.Mute?CueType::CountdownVisual:CueType::CountdownAudio;
                Value.CountdownSecond=Second;
                Append(Result,Value);
                CountdownEmittedMask=static_cast<std::uint16_t>(CountdownEmittedMask|Bit);
            }
        }
    }

    void AppendPhaseNotice(const Snapshot& Current,FrameResult& Result) const
    {
        if(Current.Phase==Previous.Phase)return;
        Cue Value{};
        if(Current.Phase==drone_soccer::Phase::Ready)
        {
            Value.Type=CueType::NoticeReady;
            Append(Result,Value);
        }
        else if(Current.Phase==drone_soccer::Phase::Playing)
        {
            Value.Type=Current.CurrentSet==1?CueType::NoticeMatchStart:CueType::NoticeSetStart;
            Append(Result,Value);
        }
        else if(Current.Phase==drone_soccer::Phase::Break)
        {
            Value.Type=CueType::NoticeSetEndBreak;
            Append(Result,Value);
        }
        else if(Current.Phase==drone_soccer::Phase::Finished)
        {
            Value.Type=CueType::NoticeMatchFinished;
            Append(Result,Value);
        }
    }
};
}
