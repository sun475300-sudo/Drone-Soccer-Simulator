#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

// Header-only referee ledger. It accepts automatic-rule candidates but never
// performs geometric goal, offside, or physics detection itself.
namespace ImperialOfficiating
{
constexpr std::size_t TeamCount=2U;
constexpr std::size_t MaximumSets=3U;
constexpr double RegulationSetDurationS=180.0;
constexpr double PenaltyShotWindowS=5.0;
constexpr int MinimumActivePlayers=3;
constexpr int MaximumTeamPlayers=5;
constexpr int MaximumParticipantId=9;

// Blue and Red are the FIDA team colours. Invalid enum casts are rejected by
// every mutating API that receives a Team value.
enum class Team : std::uint8_t { Blue=0U, Red=1U };
enum class CandidateStatus : std::uint8_t { Pending, Approved, Cancelled, UnderReview };
enum class PenaltyReason : std::uint8_t { OwnGoalReverseEntry, OffsideConsecutiveScoring, TwoWarnings };
enum class PenaltyStatus : std::uint8_t { Pending, Resolved };
enum class PenaltyShotOutcome : std::uint8_t { Scored, Missed };
enum class ForfeitScope : std::uint8_t { Set, Match };
// Only the minimum-player basis is rule-derived here. Other referee grounds
// are deliberately marked for tournament-guideline confirmation.
enum class ForfeitBasis : std::uint8_t { BelowMinimumActivePlayers, RefereeDeclaredOtherGuidanceRequired };
enum class RoleTransferStatus : std::uint8_t { Pending, Approved, Cancelled };
enum class SetStatus : std::uint8_t { Open, AwaitingPenaltyShots, PenaltyShots, AwaitingDecision, Committed, Forfeited };
enum class SetOutcome : std::uint8_t { Undecided, BlueWin, RedWin, Draw };
enum class MatchState : std::uint8_t { InProgress, SetReview, AwaitingFinalization, Finalized };
enum class MatchOutcome : std::uint8_t { Undecided, BlueWin, RedWin, Draw };
enum class RevisionKind : std::uint8_t {
    CandidateRecorded,
    CandidateApproved,
    CandidateCancelled,
    CandidateReReviewRequested,
    WarningIssued,
    PenaltyScheduled,
    PenaltyShotRecorded,
    RoleTransferRequested,
    RoleTransferApproved,
    RoleTransferCancelled,
    FinalWhistleRecorded,
    SetForfeitDeclared,
    MatchForfeitDeclared,
    SetCommitted,
    MatchFinalized,
    MatchReopened
};

enum class Rejection : std::uint8_t {
    None,
    InvalidConfiguration,
    InvalidTeam,
    InvalidReasonText,
    NonFiniteInput,
    TimeOutOfRange,
    PenaltyShotTimeOutOfRange,
    SetNotAtFinalWhistle,
    SetClosed,
    SetAlreadyCommitted,
    SetNotReadyForDecision,
    MatchComplete,
    FinalizedLocked,
    CandidateNotFound,
    CandidateAlreadyApproved,
    CandidateAlreadyCancelled,
    CandidateNotReviewable,
    CandidateAlreadyUnderReview,
    CandidateReviewUnresolved,
    PenaltyNotFound,
    PenaltyNotPending,
    PenaltyPhaseNotOpen,
    InvalidPenaltyReason,
    InvalidPenaltyOutcome,
    PenaltyAlreadyScheduled,
    ActivePlayersOutOfRange,
    ForfeitBasisNotMet,
    InvalidForfeitScope,
    InvalidForfeitBasis,
    RoleTransferPolicyDisabled,
    ParticipantOutOfRange,
    RoleTransferNotFound,
    RoleTransferNotPending,
    RoleTransferParticipantsDoNotMatch,
    RoleTransferAlreadyRequested,
    NotFinalized,
    MatchNotReadyForFinalization,
    RevisionCapacityExceeded,
    CandidateCapacityExceeded,
    PenaltyCapacityExceeded,
    RoleTransferCapacityExceeded,
    IndexOutOfRange
};

// Fixed storage avoids heap allocation and keeps the core usable when Unreal
// exceptions are disabled. UTF-8 byte sequences are permitted; embedded NULs
// and an absent terminator are rejected.
struct ReasonText
{
    static constexpr std::size_t Capacity=96U;
    std::array<char,Capacity> Characters{};
    std::size_t Length=0U;

    bool IsValid() const
    {
        if(Length==0U||Length>=Capacity||Characters[Length]!='\0')return false;
        for(std::size_t Index=0U;Index<Length;++Index)
            if(Characters[Index]=='\0')return false;
        return true;
    }

    template<std::size_t N>
    static ReasonText FromLiteral(const char (&Text)[N])
    {
        ReasonText Result{};
        if constexpr(N>1U&&N<=Capacity)
        {
            for(std::size_t Index=0U;Index+1U<N;++Index)Result.Characters[Index]=Text[Index];
            Result.Length=N-1U;
            Result.Characters[Result.Length]='\0';
        }
        return Result;
    }
};

struct RoleTransferPolicy
{
    // This policy is a simulator choice. The FIDA source in this repository
    // does not specify the succession procedure; tournament guidance is needed.
    bool Enabled=false;
    std::array<int,TeamCount> InitialStrikerParticipant{{0,5}};
    std::array<int,TeamCount> InitialGuideParticipant{{1,6}};
};

struct MatchConfiguration
{
    RoleTransferPolicy TransferPolicy{};
    // This is a confirmation-required integration hook only. It does not
    // choose an extension or shoot-out procedure in this header.
    bool ExtensionOrShootoutHookRequested=false;
};

struct CommandResult
{
    Rejection Reason=Rejection::None;
    std::uint32_t Id=0U;

    bool Accepted() const { return Reason==Rejection::None; }
};

struct GoalCandidate
{
    std::uint32_t Id=0U;
    std::uint32_t SubmissionOrder=0U;
    Team ScoringTeam=Team::Blue;
    std::size_t SetIndex=0U;
    double EventTimeS=0.0;
    CandidateStatus Status=CandidateStatus::Pending;
    ReasonText Evidence{};
};

struct PenaltyRecord
{
    std::uint32_t Id=0U;
    std::size_t SetIndex=0U;
    Team OffendingTeam=Team::Blue;
    PenaltyReason Reason=PenaltyReason::OwnGoalReverseEntry;
    PenaltyStatus Status=PenaltyStatus::Pending;
    PenaltyShotOutcome Outcome=PenaltyShotOutcome::Missed;
    double ElapsedS=0.0;
};

struct RoleTransferRequest
{
    std::uint32_t Id=0U;
    Team TeamValue=Team::Blue;
    int GuideParticipant=-1;
    int StrikerParticipant=-1;
    RoleTransferStatus Status=RoleTransferStatus::Pending;
    // Always true because this procedure is not specified by the retained source.
    bool TournamentGuidanceRequired=true;
};

struct RevisionEntry
{
    std::uint32_t Number=0U;
    RevisionKind Kind=RevisionKind::CandidateRecorded;
    std::uint32_t RelatedId=0U;
    Team TeamValue=Team::Blue;
    std::size_t SetIndex=0U;
    ReasonText Reason{};
};

class Ledger
{
public:
    static constexpr std::size_t MaximumCandidates=64U;
    static constexpr std::size_t MaximumPenalties=32U;
    static constexpr std::size_t MaximumRoleTransfers=16U;
    static constexpr std::size_t MaximumRevisions=256U;

    explicit Ledger(const MatchConfiguration& InConfiguration=MatchConfiguration())
        : Configuration(InConfiguration), ConfigurationValid(IsConfigurationValid(InConfiguration))
    {
        ResetStorage();
    }

    Rejection GetConfigurationStatus() const
    {
        return ConfigurationValid?Rejection::None:Rejection::InvalidConfiguration;
    }

    MatchState GetMatchState() const { return State; }
    MatchOutcome GetMatchOutcome() const { return CurrentOutcome; }
    bool IsFinalized() const { return State==MatchState::Finalized; }
    bool AreCorrectionsOpen() const { return CorrectionsOpen; }
    bool IsExtensionOrShootoutHookRequested() const { return Configuration.ExtensionOrShootoutHookRequested; }
    std::size_t GetCurrentSetIndex() const { return CurrentSet; }
    std::size_t GetCandidateCount() const { return CandidateCount; }
    std::size_t GetPenaltyCount() const { return PenaltyCount; }
    std::size_t GetRoleTransferCount() const { return RoleTransferCount; }
    std::size_t GetRevisionCount() const { return RevisionCount; }

    Rejection GetCandidate(std::size_t Index,GoalCandidate& OutCandidate) const
    {
        if(Index>=CandidateCount)return Rejection::IndexOutOfRange;
        OutCandidate=Candidates[Index];
        return Rejection::None;
    }

    Rejection GetPenalty(std::size_t Index,PenaltyRecord& OutPenalty) const
    {
        if(Index>=PenaltyCount)return Rejection::IndexOutOfRange;
        OutPenalty=Penalties[Index];
        return Rejection::None;
    }

    Rejection GetRoleTransfer(std::size_t Index,RoleTransferRequest& OutRequest) const
    {
        if(Index>=RoleTransferCount)return Rejection::IndexOutOfRange;
        OutRequest=RoleTransfers[Index];
        return Rejection::None;
    }

    Rejection GetRevision(std::size_t Index,RevisionEntry& OutRevision) const
    {
        if(Index>=RevisionCount)return Rejection::IndexOutOfRange;
        OutRevision=Revisions[Index];
        return Rejection::None;
    }

    Rejection GetWarnings(Team TeamValue,int& OutWarnings) const
    {
        if(!IsTeamValid(TeamValue))return Rejection::InvalidTeam;
        OutWarnings=Warnings[TeamSlot(TeamValue)];
        return Rejection::None;
    }

    Rejection GetActivePlayers(Team TeamValue,int& OutActivePlayers) const
    {
        if(!IsTeamValid(TeamValue))return Rejection::InvalidTeam;
        OutActivePlayers=ActivePlayers[TeamSlot(TeamValue)];
        return Rejection::None;
    }

    Rejection GetCurrentStrikerParticipant(Team TeamValue,int& OutParticipant) const
    {
        if(!IsTeamValid(TeamValue))return Rejection::InvalidTeam;
        OutParticipant=CurrentStriker[TeamSlot(TeamValue)];
        return Rejection::None;
    }

    Rejection GetCurrentGuideParticipant(Team TeamValue,int& OutParticipant) const
    {
        if(!IsTeamValid(TeamValue))return Rejection::InvalidTeam;
        OutParticipant=CurrentGuide[TeamSlot(TeamValue)];
        return Rejection::None;
    }

    Rejection GetOfficialSetScore(std::size_t SetIndex,Team TeamValue,int& OutScore) const
    {
        if(SetIndex>=MaximumSets)return Rejection::IndexOutOfRange;
        if(!IsTeamValid(TeamValue))return Rejection::InvalidTeam;
        OutScore=OfficialScore(SetIndex,TeamValue);
        return Rejection::None;
    }

    Rejection GetSetOutcome(std::size_t SetIndex,SetOutcome& OutOutcome) const
    {
        if(SetIndex>=MaximumSets)return Rejection::IndexOutOfRange;
        OutOutcome=Sets[SetIndex].Outcome;
        return Rejection::None;
    }

    Rejection SetActivePlayerCount(Team TeamValue,int Count)
    {
        const Rejection StateReason=CanMutateMatch();
        if(StateReason!=Rejection::None)return StateReason;
        if(!IsTeamValid(TeamValue))return Rejection::InvalidTeam;
        if(Count<0||Count>MaximumTeamPlayers)return Rejection::ActivePlayersOutOfRange;
        ActivePlayers[TeamSlot(TeamValue)]=Count;
        return Rejection::None;
    }

    CommandResult RecordGoalCandidate(Team TeamValue,double EventTimeS,const ReasonText& Evidence)
    {
        const Rejection StateReason=CanMutateMatch();
        if(StateReason!=Rejection::None)return {StateReason,0U};
        if(!IsTeamValid(TeamValue))return {Rejection::InvalidTeam,0U};
        if(!Evidence.IsValid())return {Rejection::InvalidReasonText,0U};
        if(!std::isfinite(EventTimeS))return {Rejection::NonFiniteInput,0U};
        if(EventTimeS<0.0||EventTimeS>RegulationSetDurationS)return {Rejection::TimeOutOfRange,0U};
        if(CurrentSet>=MaximumSets)return {Rejection::MatchComplete,0U};
        if(Sets[CurrentSet].Status!=SetStatus::Open)return {Rejection::SetClosed,0U};
        if(CandidateCount>=MaximumCandidates)return {Rejection::CandidateCapacityExceeded,0U};
        if(!CanAppendRevisions(1U))return {Rejection::RevisionCapacityExceeded,0U};

        const std::uint32_t Id=NextCandidateId;
        ++NextCandidateId;
        GoalCandidate& Candidate=Candidates[CandidateCount];
        Candidate.Id=Id;
        Candidate.SubmissionOrder=NextSubmissionOrder;
        ++NextSubmissionOrder;
        Candidate.ScoringTeam=TeamValue;
        Candidate.SetIndex=CurrentSet;
        Candidate.EventTimeS=EventTimeS;
        Candidate.Status=CandidateStatus::Pending;
        Candidate.Evidence=Evidence;
        ++CandidateCount;
        AppendRevision(RevisionKind::CandidateRecorded,Id,TeamValue,CurrentSet,Evidence);
        return {Rejection::None,Id};
    }

    Rejection ApproveGoalCandidate(std::uint32_t CandidateId,const ReasonText& Reason)
    {
        GoalCandidate* Candidate=nullptr;
        const Rejection Lookup=FindMutableCandidate(CandidateId,Candidate);
        if(Lookup!=Rejection::None)return Lookup;
        const Rejection StateReason=CanReviseCandidate(*Candidate);
        if(StateReason!=Rejection::None)return StateReason;
        if(!Reason.IsValid())return Rejection::InvalidReasonText;
        if(Candidate->Status==CandidateStatus::Approved)return Rejection::CandidateAlreadyApproved;
        if(Candidate->Status==CandidateStatus::Cancelled)return Rejection::CandidateNotReviewable;
        if(!CanAppendRevisions(1U))return Rejection::RevisionCapacityExceeded;
        Candidate->Status=CandidateStatus::Approved;
        AppendRevision(RevisionKind::CandidateApproved,CandidateId,Candidate->ScoringTeam,Candidate->SetIndex,Reason);
        RecomputeCommittedOutcomes();
        return Rejection::None;
    }

    Rejection CancelGoalCandidate(std::uint32_t CandidateId,const ReasonText& Reason)
    {
        GoalCandidate* Candidate=nullptr;
        const Rejection Lookup=FindMutableCandidate(CandidateId,Candidate);
        if(Lookup!=Rejection::None)return Lookup;
        const Rejection StateReason=CanReviseCandidate(*Candidate);
        if(StateReason!=Rejection::None)return StateReason;
        if(!Reason.IsValid())return Rejection::InvalidReasonText;
        if(Candidate->Status==CandidateStatus::Cancelled)return Rejection::CandidateAlreadyCancelled;
        if(!CanAppendRevisions(1U))return Rejection::RevisionCapacityExceeded;
        Candidate->Status=CandidateStatus::Cancelled;
        AppendRevision(RevisionKind::CandidateCancelled,CandidateId,Candidate->ScoringTeam,Candidate->SetIndex,Reason);
        RecomputeCommittedOutcomes();
        return Rejection::None;
    }

    Rejection RequestGoalReReview(std::uint32_t CandidateId,const ReasonText& Reason)
    {
        GoalCandidate* Candidate=nullptr;
        const Rejection Lookup=FindMutableCandidate(CandidateId,Candidate);
        if(Lookup!=Rejection::None)return Lookup;
        const Rejection StateReason=CanReviseCandidate(*Candidate);
        if(StateReason!=Rejection::None)return StateReason;
        if(!Reason.IsValid())return Rejection::InvalidReasonText;
        if(Candidate->Status==CandidateStatus::Pending||Candidate->Status==CandidateStatus::UnderReview)
            return Rejection::CandidateAlreadyUnderReview;
        if(!CanAppendRevisions(1U))return Rejection::RevisionCapacityExceeded;
        Candidate->Status=CandidateStatus::UnderReview;
        AppendRevision(RevisionKind::CandidateReReviewRequested,CandidateId,Candidate->ScoringTeam,Candidate->SetIndex,Reason);
        RecomputeCommittedOutcomes();
        return Rejection::None;
    }

    Rejection CloseCurrentSetAtFinalWhistle(double WhistleTimeS,const ReasonText& Reason)
    {
        const Rejection StateReason=CanMutateMatch();
        if(StateReason!=Rejection::None)return StateReason;
        if(!Reason.IsValid())return Rejection::InvalidReasonText;
        if(!std::isfinite(WhistleTimeS))return Rejection::NonFiniteInput;
        if(WhistleTimeS<RegulationSetDurationS)return Rejection::SetNotAtFinalWhistle;
        if(WhistleTimeS>RegulationSetDurationS)return Rejection::TimeOutOfRange;
        if(CurrentSet>=MaximumSets)return Rejection::MatchComplete;
        SetRecord& Set=Sets[CurrentSet];
        if(Set.Status!=SetStatus::Open)return Rejection::SetClosed;
        if(!CanAppendRevisions(1U))return Rejection::RevisionCapacityExceeded;
        Set.Status=HasPendingPenalty(CurrentSet)?SetStatus::AwaitingPenaltyShots:SetStatus::AwaitingDecision;
        State=MatchState::SetReview;
        AppendRevision(RevisionKind::FinalWhistleRecorded,0U,Team::Blue,CurrentSet,Reason);
        return Rejection::None;
    }

    Rejection IssueWarning(Team TeamValue,const ReasonText& Reason)
    {
        const Rejection StateReason=CanMutateMatch();
        if(StateReason!=Rejection::None)return StateReason;
        if(!IsTeamValid(TeamValue))return Rejection::InvalidTeam;
        if(!Reason.IsValid())return Rejection::InvalidReasonText;
        if(CurrentSet>=MaximumSets||Sets[CurrentSet].Status==SetStatus::Committed||Sets[CurrentSet].Status==SetStatus::Forfeited)
            return Rejection::SetAlreadyCommitted;

        const std::size_t Slot=TeamSlot(TeamValue);
        const bool AddsPenalty=Warnings[Slot]==1;
        if(AddsPenalty&&PenaltyCount>=MaximumPenalties)return Rejection::PenaltyCapacityExceeded;
        if(!CanAppendRevisions(AddsPenalty?2U:1U))return Rejection::RevisionCapacityExceeded;
        ++Warnings[Slot];
        AppendRevision(RevisionKind::WarningIssued,0U,TeamValue,CurrentSet,Reason);
        if(AddsPenalty)
        {
            const std::uint32_t PenaltyId=AppendPenalty(TeamValue,PenaltyReason::TwoWarnings);
            AppendRevision(RevisionKind::PenaltyScheduled,PenaltyId,TeamValue,CurrentSet,Reason);
            if(Sets[CurrentSet].Status==SetStatus::AwaitingDecision)Sets[CurrentSet].Status=SetStatus::AwaitingPenaltyShots;
        }
        return Rejection::None;
    }

    CommandResult SchedulePenalty(Team OffendingTeam,PenaltyReason Reason,const ReasonText& Explanation)
    {
        const Rejection StateReason=CanMutateMatch();
        if(StateReason!=Rejection::None)return {StateReason,0U};
        if(!IsTeamValid(OffendingTeam))return {Rejection::InvalidTeam,0U};
        if(!IsPenaltyReasonValid(Reason))return {Rejection::InvalidPenaltyReason,0U};
        if(!Explanation.IsValid())return {Rejection::InvalidReasonText,0U};
        if(CurrentSet>=MaximumSets||Sets[CurrentSet].Status==SetStatus::Committed||Sets[CurrentSet].Status==SetStatus::Forfeited)
            return {Rejection::SetAlreadyCommitted,0U};
        if(Reason==PenaltyReason::TwoWarnings)
        {
            const std::size_t Slot=TeamSlot(OffendingTeam);
            if(Warnings[Slot]<2)return {Rejection::ForfeitBasisNotMet,0U};
            if(HasWarningPenalty(OffendingTeam))return {Rejection::PenaltyAlreadyScheduled,0U};
        }
        if(PenaltyCount>=MaximumPenalties)return {Rejection::PenaltyCapacityExceeded,0U};
        if(!CanAppendRevisions(1U))return {Rejection::RevisionCapacityExceeded,0U};
        const std::uint32_t Id=AppendPenalty(OffendingTeam,Reason);
        AppendRevision(RevisionKind::PenaltyScheduled,Id,OffendingTeam,CurrentSet,Explanation);
        if(Sets[CurrentSet].Status==SetStatus::AwaitingDecision)Sets[CurrentSet].Status=SetStatus::AwaitingPenaltyShots;
        return {Rejection::None,Id};
    }

    Rejection BeginPenaltyShots(const ReasonText& Reason)
    {
        const Rejection StateReason=CanMutateMatch();
        if(StateReason!=Rejection::None)return StateReason;
        if(!Reason.IsValid())return Rejection::InvalidReasonText;
        if(CurrentSet>=MaximumSets)return Rejection::MatchComplete;
        SetRecord& Set=Sets[CurrentSet];
        if(Set.Status!=SetStatus::AwaitingPenaltyShots)return Rejection::PenaltyPhaseNotOpen;
        if(HasUnresolvedCandidate(CurrentSet))return Rejection::CandidateReviewUnresolved;
        if(!CanAppendRevisions(1U))return Rejection::RevisionCapacityExceeded;
        Set.Status=SetStatus::PenaltyShots;
        AppendRevision(RevisionKind::PenaltyScheduled,0U,Team::Blue,CurrentSet,Reason);
        return Rejection::None;
    }

    Rejection RecordPenaltyShot(std::uint32_t PenaltyId,PenaltyShotOutcome Outcome,double ElapsedS,const ReasonText& Reason)
    {
        const Rejection StateReason=CanMutateMatch();
        if(StateReason!=Rejection::None)return StateReason;
        if(!Reason.IsValid())return Rejection::InvalidReasonText;
        if(!IsPenaltyOutcomeValid(Outcome))return Rejection::InvalidPenaltyOutcome;
        if(!std::isfinite(ElapsedS))return Rejection::NonFiniteInput;
        if(ElapsedS<0.0||ElapsedS>PenaltyShotWindowS)return Rejection::PenaltyShotTimeOutOfRange;
        PenaltyRecord* Penalty=nullptr;
        const Rejection Lookup=FindMutablePenalty(PenaltyId,Penalty);
        if(Lookup!=Rejection::None)return Lookup;
        if(Penalty->Status!=PenaltyStatus::Pending)return Rejection::PenaltyNotPending;
        if(CurrentSet>=MaximumSets||Penalty->SetIndex!=CurrentSet||Sets[CurrentSet].Status!=SetStatus::PenaltyShots)
            return Rejection::PenaltyPhaseNotOpen;
        if(!CanAppendRevisions(1U))return Rejection::RevisionCapacityExceeded;
        Penalty->Status=PenaltyStatus::Resolved;
        Penalty->Outcome=Outcome;
        Penalty->ElapsedS=ElapsedS;
        AppendRevision(RevisionKind::PenaltyShotRecorded,PenaltyId,OtherTeam(Penalty->OffendingTeam),CurrentSet,Reason);
        if(!HasPendingPenalty(CurrentSet))Sets[CurrentSet].Status=SetStatus::AwaitingDecision;
        return Rejection::None;
    }

    CommandResult RequestStrikerRoleTransfer(Team TeamValue,int GuideParticipant,int StrikerParticipant,const ReasonText& Reason)
    {
        const Rejection StateReason=CanMutateMatch();
        if(StateReason!=Rejection::None)return {StateReason,0U};
        if(!Configuration.TransferPolicy.Enabled)return {Rejection::RoleTransferPolicyDisabled,0U};
        if(!IsTeamValid(TeamValue))return {Rejection::InvalidTeam,0U};
        if(!IsParticipantValid(GuideParticipant)||!IsParticipantValid(StrikerParticipant))return {Rejection::ParticipantOutOfRange,0U};
        if(!Reason.IsValid())return {Rejection::InvalidReasonText,0U};
        const std::size_t Slot=TeamSlot(TeamValue);
        if(GuideParticipant!=CurrentGuide[Slot]||StrikerParticipant!=CurrentStriker[Slot])
            return {Rejection::RoleTransferParticipantsDoNotMatch,0U};
        if(HasPendingRoleTransfer(TeamValue))return {Rejection::RoleTransferAlreadyRequested,0U};
        if(RoleTransferCount>=MaximumRoleTransfers)return {Rejection::RoleTransferCapacityExceeded,0U};
        if(!CanAppendRevisions(1U))return {Rejection::RevisionCapacityExceeded,0U};
        const std::uint32_t Id=NextRoleTransferId;
        ++NextRoleTransferId;
        RoleTransferRequest& Request=RoleTransfers[RoleTransferCount];
        Request.Id=Id;
        Request.TeamValue=TeamValue;
        Request.GuideParticipant=GuideParticipant;
        Request.StrikerParticipant=StrikerParticipant;
        Request.Status=RoleTransferStatus::Pending;
        Request.TournamentGuidanceRequired=true;
        ++RoleTransferCount;
        AppendRevision(RevisionKind::RoleTransferRequested,Id,TeamValue,CurrentSet,Reason);
        return {Rejection::None,Id};
    }

    Rejection ApproveStrikerRoleTransfer(std::uint32_t RequestId,const ReasonText& Reason)
    {
        RoleTransferRequest* Request=nullptr;
        const Rejection Lookup=FindMutableRoleTransfer(RequestId,Request);
        if(Lookup!=Rejection::None)return Lookup;
        const Rejection StateReason=CanMutateMatch();
        if(StateReason!=Rejection::None)return StateReason;
        if(!Reason.IsValid())return Rejection::InvalidReasonText;
        if(Request->Status!=RoleTransferStatus::Pending)return Rejection::RoleTransferNotPending;
        const std::size_t Slot=TeamSlot(Request->TeamValue);
        if(Request->GuideParticipant!=CurrentGuide[Slot]||Request->StrikerParticipant!=CurrentStriker[Slot])
            return Rejection::RoleTransferParticipantsDoNotMatch;
        if(!CanAppendRevisions(1U))return Rejection::RevisionCapacityExceeded;
        Request->Status=RoleTransferStatus::Approved;
        CurrentStriker[Slot]=Request->GuideParticipant;
        CurrentGuide[Slot]=-1;
        AppendRevision(RevisionKind::RoleTransferApproved,RequestId,Request->TeamValue,CurrentSet,Reason);
        return Rejection::None;
    }

    Rejection CancelStrikerRoleTransfer(std::uint32_t RequestId,const ReasonText& Reason)
    {
        RoleTransferRequest* Request=nullptr;
        const Rejection Lookup=FindMutableRoleTransfer(RequestId,Request);
        if(Lookup!=Rejection::None)return Lookup;
        const Rejection StateReason=CanMutateMatch();
        if(StateReason!=Rejection::None)return StateReason;
        if(!Reason.IsValid())return Rejection::InvalidReasonText;
        if(Request->Status!=RoleTransferStatus::Pending)return Rejection::RoleTransferNotPending;
        if(!CanAppendRevisions(1U))return Rejection::RevisionCapacityExceeded;
        Request->Status=RoleTransferStatus::Cancelled;
        AppendRevision(RevisionKind::RoleTransferCancelled,RequestId,Request->TeamValue,CurrentSet,Reason);
        return Rejection::None;
    }

    Rejection DeclareForfeit(Team ForfeitingTeam,ForfeitScope Scope,ForfeitBasis Basis,const ReasonText& Reason)
    {
        const Rejection StateReason=CanMutateMatch();
        if(StateReason!=Rejection::None)return StateReason;
        if(!IsTeamValid(ForfeitingTeam))return Rejection::InvalidTeam;
        if(!IsForfeitScopeValid(Scope))return Rejection::InvalidForfeitScope;
        if(!IsForfeitBasisValid(Basis))return Rejection::InvalidForfeitBasis;
        if(!Reason.IsValid())return Rejection::InvalidReasonText;
        if(Basis==ForfeitBasis::BelowMinimumActivePlayers&&ActivePlayers[TeamSlot(ForfeitingTeam)]>=MinimumActivePlayers)
            return Rejection::ForfeitBasisNotMet;
        if(!CanAppendRevisions(1U))return Rejection::RevisionCapacityExceeded;

        if(Scope==ForfeitScope::Set)
        {
            if(CurrentSet>=MaximumSets)return Rejection::MatchComplete;
            SetRecord& Set=Sets[CurrentSet];
            if(Set.Status==SetStatus::Committed||Set.Status==SetStatus::Forfeited)return Rejection::SetAlreadyCommitted;
            Set.Status=SetStatus::Forfeited;
            Set.Outcome=OutcomeForWinner(OtherTeam(ForfeitingTeam));
            Set.Forfeited=true;
            AppendRevision(RevisionKind::SetForfeitDeclared,0U,ForfeitingTeam,CurrentSet,Reason);
            CompleteCurrentSet();
            return Rejection::None;
        }

        MatchForfeitDeclared=true;
        ForfeitingMatchTeam=ForfeitingTeam;
        CurrentOutcome=MatchOutcomeForWinner(OtherTeam(ForfeitingTeam));
        State=MatchState::AwaitingFinalization;
        AppendRevision(RevisionKind::MatchForfeitDeclared,0U,ForfeitingTeam,CurrentSet,Reason);
        return Rejection::None;
    }

    Rejection CommitCurrentSet(const ReasonText& Reason)
    {
        const Rejection StateReason=CanMutateMatch();
        if(StateReason!=Rejection::None)return StateReason;
        if(!Reason.IsValid())return Rejection::InvalidReasonText;
        if(CurrentSet>=MaximumSets)return Rejection::MatchComplete;
        SetRecord& Set=Sets[CurrentSet];
        if(Set.Status==SetStatus::Committed||Set.Status==SetStatus::Forfeited)return Rejection::SetAlreadyCommitted;
        if(Set.Status!=SetStatus::AwaitingDecision)return Rejection::SetNotReadyForDecision;
        if(HasUnresolvedCandidate(CurrentSet))return Rejection::CandidateReviewUnresolved;
        if(HasPendingPenalty(CurrentSet))return Rejection::PenaltyPhaseNotOpen;
        if(!CanAppendRevisions(1U))return Rejection::RevisionCapacityExceeded;
        Set.Outcome=ComputeScoreOutcome(CurrentSet);
        Set.Status=SetStatus::Committed;
        AppendRevision(RevisionKind::SetCommitted,0U,Team::Blue,CurrentSet,Reason);
        CompleteCurrentSet();
        return Rejection::None;
    }

    Rejection FinalizeMatch(const ReasonText& Reason)
    {
        if(!ConfigurationValid)return Rejection::InvalidConfiguration;
        if(State==MatchState::Finalized)return Rejection::FinalizedLocked;
        if(!Reason.IsValid())return Rejection::InvalidReasonText;
        if(State!=MatchState::AwaitingFinalization)return Rejection::MatchNotReadyForFinalization;
        RecomputeCommittedOutcomes();
        const MatchOutcome Outcome=DetermineMatchOutcome();
        if(Outcome==MatchOutcome::Undecided)return Rejection::MatchNotReadyForFinalization;
        if(!CanAppendRevisions(1U))return Rejection::RevisionCapacityExceeded;
        CurrentOutcome=Outcome;
        WarningsBeforeFinalization=Warnings;
        Warnings={{0,0}};
        WarningsWereReset=true;
        CorrectionsOpen=false;
        State=MatchState::Finalized;
        AppendRevision(RevisionKind::MatchFinalized,0U,Team::Blue,CurrentSet,Reason);
        return Rejection::None;
    }

    Rejection ReopenFinalizedMatch(const ReasonText& Reason)
    {
        if(!ConfigurationValid)return Rejection::InvalidConfiguration;
        if(State!=MatchState::Finalized)return Rejection::NotFinalized;
        if(!Reason.IsValid())return Rejection::InvalidReasonText;
        if(!CanAppendRevisions(1U))return Rejection::RevisionCapacityExceeded;
        if(WarningsWereReset)Warnings=WarningsBeforeFinalization;
        WarningsWereReset=false;
        CorrectionsOpen=true;
        State=MatchState::AwaitingFinalization;
        CurrentOutcome=DetermineMatchOutcome();
        AppendRevision(RevisionKind::MatchReopened,0U,Team::Blue,CurrentSet,Reason);
        return Rejection::None;
    }

private:
    struct SetRecord
    {
        SetStatus Status=SetStatus::Open;
        SetOutcome Outcome=SetOutcome::Undecided;
        bool Forfeited=false;
    };

    MatchConfiguration Configuration{};
    bool ConfigurationValid=false;
    MatchState State=MatchState::InProgress;
    MatchOutcome CurrentOutcome=MatchOutcome::Undecided;
    std::size_t CurrentSet=0U;
    bool CorrectionsOpen=false;
    bool MatchForfeitDeclared=false;
    Team ForfeitingMatchTeam=Team::Blue;
    std::array<int,TeamCount> ActivePlayers{{MaximumTeamPlayers,MaximumTeamPlayers}};
    std::array<int,TeamCount> Warnings{{0,0}};
    std::array<int,TeamCount> WarningsBeforeFinalization{{0,0}};
    bool WarningsWereReset=false;
    std::array<int,TeamCount> CurrentStriker{{0,5}};
    std::array<int,TeamCount> CurrentGuide{{1,6}};
    std::array<SetRecord,MaximumSets> Sets{};
    std::array<GoalCandidate,MaximumCandidates> Candidates{};
    std::array<PenaltyRecord,MaximumPenalties> Penalties{};
    std::array<RoleTransferRequest,MaximumRoleTransfers> RoleTransfers{};
    std::array<RevisionEntry,MaximumRevisions> Revisions{};
    std::size_t CandidateCount=0U;
    std::size_t PenaltyCount=0U;
    std::size_t RoleTransferCount=0U;
    std::size_t RevisionCount=0U;
    std::uint32_t NextCandidateId=1U;
    std::uint32_t NextPenaltyId=1U;
    std::uint32_t NextRoleTransferId=1U;
    std::uint32_t NextSubmissionOrder=1U;

    void ResetStorage()
    {
        State=MatchState::InProgress;
        CurrentOutcome=MatchOutcome::Undecided;
        CurrentSet=0U;
        CorrectionsOpen=false;
        MatchForfeitDeclared=false;
        ForfeitingMatchTeam=Team::Blue;
        ActivePlayers={{MaximumTeamPlayers,MaximumTeamPlayers}};
        Warnings={{0,0}};
        WarningsBeforeFinalization={{0,0}};
        WarningsWereReset=false;
        CurrentStriker=Configuration.TransferPolicy.InitialStrikerParticipant;
        CurrentGuide=Configuration.TransferPolicy.InitialGuideParticipant;
        Sets={};
        Candidates={};
        Penalties={};
        RoleTransfers={};
        Revisions={};
        CandidateCount=0U;
        PenaltyCount=0U;
        RoleTransferCount=0U;
        RevisionCount=0U;
        NextCandidateId=1U;
        NextPenaltyId=1U;
        NextRoleTransferId=1U;
        NextSubmissionOrder=1U;
    }

    static bool IsTeamValid(Team TeamValue)
    {
        return TeamValue==Team::Blue||TeamValue==Team::Red;
    }

    static std::size_t TeamSlot(Team TeamValue)
    {
        return TeamValue==Team::Blue?0U:1U;
    }

    static Team OtherTeam(Team TeamValue)
    {
        return TeamValue==Team::Blue?Team::Red:Team::Blue;
    }

    static bool IsParticipantValid(int Participant)
    {
        return Participant>=0&&Participant<=MaximumParticipantId;
    }

    static bool IsPenaltyReasonValid(PenaltyReason Reason)
    {
        return Reason==PenaltyReason::OwnGoalReverseEntry||Reason==PenaltyReason::OffsideConsecutiveScoring||Reason==PenaltyReason::TwoWarnings;
    }

    static bool IsPenaltyOutcomeValid(PenaltyShotOutcome Outcome)
    {
        return Outcome==PenaltyShotOutcome::Scored||Outcome==PenaltyShotOutcome::Missed;
    }

    static bool IsForfeitScopeValid(ForfeitScope Scope)
    {
        return Scope==ForfeitScope::Set||Scope==ForfeitScope::Match;
    }

    static bool IsForfeitBasisValid(ForfeitBasis Basis)
    {
        return Basis==ForfeitBasis::BelowMinimumActivePlayers||Basis==ForfeitBasis::RefereeDeclaredOtherGuidanceRequired;
    }

    static bool IsConfigurationValid(const MatchConfiguration& InConfiguration)
    {
        for(std::size_t Index=0U;Index<TeamCount;++Index)
        {
            if(!IsParticipantValid(InConfiguration.TransferPolicy.InitialStrikerParticipant[Index])||
                !IsParticipantValid(InConfiguration.TransferPolicy.InitialGuideParticipant[Index])||
                InConfiguration.TransferPolicy.InitialStrikerParticipant[Index]==InConfiguration.TransferPolicy.InitialGuideParticipant[Index])
                return false;
        }
        return true;
    }

    static SetOutcome OutcomeForWinner(Team Winner)
    {
        return Winner==Team::Blue?SetOutcome::BlueWin:SetOutcome::RedWin;
    }

    static MatchOutcome MatchOutcomeForWinner(Team Winner)
    {
        return Winner==Team::Blue?MatchOutcome::BlueWin:MatchOutcome::RedWin;
    }

    Rejection CanMutateMatch() const
    {
        if(!ConfigurationValid)return Rejection::InvalidConfiguration;
        if(State==MatchState::Finalized)return Rejection::FinalizedLocked;
        if(MatchForfeitDeclared)return Rejection::MatchComplete;
        return Rejection::None;
    }

    Rejection CanReviseCandidate(const GoalCandidate& Candidate) const
    {
        const Rejection StateReason=CanMutateMatch();
        if(StateReason!=Rejection::None)return StateReason;
        if(Candidate.SetIndex!=CurrentSet&&!CorrectionsOpen)return Rejection::SetAlreadyCommitted;
        return Rejection::None;
    }

    bool CanAppendRevisions(std::size_t Count) const
    {
        return Count<=MaximumRevisions&&RevisionCount<=MaximumRevisions-Count;
    }

    void AppendRevision(RevisionKind Kind,std::uint32_t RelatedId,Team TeamValue,std::size_t SetIndex,const ReasonText& Reason)
    {
        RevisionEntry& Entry=Revisions[RevisionCount];
        Entry.Number=static_cast<std::uint32_t>(RevisionCount+1U);
        Entry.Kind=Kind;
        Entry.RelatedId=RelatedId;
        Entry.TeamValue=TeamValue;
        Entry.SetIndex=SetIndex;
        Entry.Reason=Reason;
        ++RevisionCount;
    }

    Rejection FindMutableCandidate(std::uint32_t CandidateId,GoalCandidate*& OutCandidate)
    {
        if(CandidateId==0U)return Rejection::CandidateNotFound;
        for(std::size_t Index=0U;Index<CandidateCount;++Index)
            if(Candidates[Index].Id==CandidateId)
            {
                OutCandidate=&Candidates[Index];
                return Rejection::None;
            }
        return Rejection::CandidateNotFound;
    }

    Rejection FindMutablePenalty(std::uint32_t PenaltyId,PenaltyRecord*& OutPenalty)
    {
        if(PenaltyId==0U)return Rejection::PenaltyNotFound;
        for(std::size_t Index=0U;Index<PenaltyCount;++Index)
            if(Penalties[Index].Id==PenaltyId)
            {
                OutPenalty=&Penalties[Index];
                return Rejection::None;
            }
        return Rejection::PenaltyNotFound;
    }

    Rejection FindMutableRoleTransfer(std::uint32_t RequestId,RoleTransferRequest*& OutRequest)
    {
        if(RequestId==0U)return Rejection::RoleTransferNotFound;
        for(std::size_t Index=0U;Index<RoleTransferCount;++Index)
            if(RoleTransfers[Index].Id==RequestId)
            {
                OutRequest=&RoleTransfers[Index];
                return Rejection::None;
            }
        return Rejection::RoleTransferNotFound;
    }

    bool HasUnresolvedCandidate(std::size_t SetIndex) const
    {
        for(std::size_t Index=0U;Index<CandidateCount;++Index)
            if(Candidates[Index].SetIndex==SetIndex&&
                (Candidates[Index].Status==CandidateStatus::Pending||Candidates[Index].Status==CandidateStatus::UnderReview))
                return true;
        return false;
    }

    bool HasPendingPenalty(std::size_t SetIndex) const
    {
        for(std::size_t Index=0U;Index<PenaltyCount;++Index)
            if(Penalties[Index].SetIndex==SetIndex&&Penalties[Index].Status==PenaltyStatus::Pending)return true;
        return false;
    }

    bool HasWarningPenalty(Team OffendingTeam) const
    {
        for(std::size_t Index=0U;Index<PenaltyCount;++Index)
            if(Penalties[Index].OffendingTeam==OffendingTeam&&Penalties[Index].Reason==PenaltyReason::TwoWarnings)
                return true;
        return false;
    }

    bool HasPendingRoleTransfer(Team TeamValue) const
    {
        for(std::size_t Index=0U;Index<RoleTransferCount;++Index)
            if(RoleTransfers[Index].TeamValue==TeamValue&&RoleTransfers[Index].Status==RoleTransferStatus::Pending)return true;
        return false;
    }

    std::uint32_t AppendPenalty(Team OffendingTeam,PenaltyReason Reason)
    {
        const std::uint32_t Id=NextPenaltyId;
        ++NextPenaltyId;
        PenaltyRecord& Penalty=Penalties[PenaltyCount];
        Penalty.Id=Id;
        Penalty.SetIndex=CurrentSet;
        Penalty.OffendingTeam=OffendingTeam;
        Penalty.Reason=Reason;
        Penalty.Status=PenaltyStatus::Pending;
        Penalty.Outcome=PenaltyShotOutcome::Missed;
        Penalty.ElapsedS=0.0;
        ++PenaltyCount;
        return Id;
    }

    int OfficialScore(std::size_t SetIndex,Team TeamValue) const
    {
        int Score=0;
        for(std::size_t Index=0U;Index<CandidateCount;++Index)
            if(Candidates[Index].SetIndex==SetIndex&&Candidates[Index].ScoringTeam==TeamValue&&Candidates[Index].Status==CandidateStatus::Approved)
                ++Score;
        for(std::size_t Index=0U;Index<PenaltyCount;++Index)
            if(Penalties[Index].SetIndex==SetIndex&&Penalties[Index].Status==PenaltyStatus::Resolved&&
                Penalties[Index].Outcome==PenaltyShotOutcome::Scored&&OtherTeam(Penalties[Index].OffendingTeam)==TeamValue)
                ++Score;
        return Score;
    }

    SetOutcome ComputeScoreOutcome(std::size_t SetIndex) const
    {
        const int BlueScore=OfficialScore(SetIndex,Team::Blue);
        const int RedScore=OfficialScore(SetIndex,Team::Red);
        if(BlueScore>RedScore)return SetOutcome::BlueWin;
        if(RedScore>BlueScore)return SetOutcome::RedWin;
        return SetOutcome::Draw;
    }

    void RecomputeCommittedOutcomes()
    {
        if(MatchForfeitDeclared)
        {
            CurrentOutcome=MatchOutcomeForWinner(OtherTeam(ForfeitingMatchTeam));
            return;
        }
        for(std::size_t Index=0U;Index<MaximumSets;++Index)
            if(Sets[Index].Status==SetStatus::Committed)
                Sets[Index].Outcome=HasUnresolvedCandidate(Index)?SetOutcome::Undecided:ComputeScoreOutcome(Index);
        CurrentOutcome=DetermineMatchOutcome();
    }

    MatchOutcome DetermineMatchOutcome() const
    {
        if(MatchForfeitDeclared)return MatchOutcomeForWinner(OtherTeam(ForfeitingMatchTeam));
        int BlueWins=0;
        int RedWins=0;
        std::size_t CommittedSets=0U;
        for(std::size_t Index=0U;Index<MaximumSets;++Index)
        {
            const SetRecord& Set=Sets[Index];
            if(Set.Status!=SetStatus::Committed&&Set.Status!=SetStatus::Forfeited)continue;
            ++CommittedSets;
            if(Set.Outcome==SetOutcome::Undecided)return MatchOutcome::Undecided;
            if(Set.Outcome==SetOutcome::BlueWin)++BlueWins;
            if(Set.Outcome==SetOutcome::RedWin)++RedWins;
            if(BlueWins>=2)return MatchOutcome::BlueWin;
            if(RedWins>=2)return MatchOutcome::RedWin;
        }
        if(CommittedSets<MaximumSets)return MatchOutcome::Undecided;
        if(BlueWins>RedWins)return MatchOutcome::BlueWin;
        if(RedWins>BlueWins)return MatchOutcome::RedWin;
        return MatchOutcome::Draw;
    }

    void CompleteCurrentSet()
    {
        RecomputeCommittedOutcomes();
        const MatchOutcome Outcome=DetermineMatchOutcome();
        if(Outcome!=MatchOutcome::Undecided)
        {
            State=MatchState::AwaitingFinalization;
            CurrentOutcome=Outcome;
            return;
        }
        ++CurrentSet;
        if(CurrentSet>=MaximumSets)
        {
            State=MatchState::AwaitingFinalization;
            return;
        }
        State=MatchState::InProgress;
    }
};
}
