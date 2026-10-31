#include "MatchOfficiating.h"

#include <cstddef>
#include <iostream>
#include <limits>

namespace
{
using namespace ImperialOfficiating;

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

template<std::size_t N>
ReasonText Text(const char (&Value)[N])
{
    return ReasonText::FromLiteral(Value);
}

void CloseAndCommitDraw(Ledger& Match,const char* CloseName,const char* CommitName)
{
    Expect(Match.CloseCurrentSetAtFinalWhistle(RegulationSetDurationS,Text("final whistle"))==Rejection::None,CloseName);
    Expect(Match.CommitCurrentSet(Text("all decisions entered"))==Rejection::None,CommitName);
}
}

int main()
{
    std::cout<<"Scope: deterministic header-only officiating-ledger tests; no Unreal build/runtime, physical controller, other-PC, human playtest, or network test.\n";

    {
        Ledger Match;
        Expect(Match.GetConfigurationStatus()==Rejection::None,"default configuration is valid");
        const CommandResult BeforeWhistle=Match.RecordGoalCandidate(Team::Blue,179.999,Text("automatic crossing before whistle"));
        const CommandResult AtWhistle=Match.RecordGoalCandidate(Team::Blue,180.0,Text("automatic crossing at whistle"));
        Expect(BeforeWhistle.Accepted()&&AtWhistle.Accepted()&&BeforeWhistle.Id<AtWhistle.Id,"near-whistle candidates preserve submission order");
        Expect(Match.RecordGoalCandidate(Team::Red,180.000001,Text("late crossing")).Reason==Rejection::TimeOutOfRange,"candidate after 180 seconds is rejected");
        Expect(Match.CloseCurrentSetAtFinalWhistle(179.999,Text("early whistle"))==Rejection::SetNotAtFinalWhistle,"early final whistle is rejected");
        Expect(Match.CloseCurrentSetAtFinalWhistle(RegulationSetDurationS,Text("regulation whistle"))==Rejection::None,"exact 180 second final whistle closes intake");
        Expect(Match.RecordGoalCandidate(Team::Red,180.0,Text("candidate after closure")).Reason==Rejection::SetClosed,"post-whistle candidate cannot enter closed set");
        Expect(Match.ApproveGoalCandidate(BeforeWhistle.Id,Text("referee approves earlier crossing"))==Rejection::None,"referee approves candidate after whistle");
        Expect(Match.ApproveGoalCandidate(AtWhistle.Id,Text("referee approves boundary crossing"))==Rejection::None,"boundary candidate can become official");
        int BlueScore=0;
        Expect(Match.GetOfficialSetScore(0U,Team::Blue,BlueScore)==Rejection::None&&BlueScore==2,"official score uses approved candidates only");
        Expect(Match.CommitCurrentSet(Text("all near-whistle candidates resolved"))==Rejection::None,"resolved set commits after whistle review");
    }

    {
        Ledger Match;
        const CommandResult Candidate=Match.RecordGoalCandidate(Team::Blue,12.0,Text("automatic candidate"));
        Expect(Candidate.Accepted(),"candidate for revision workflow is recorded");
        Expect(Match.ApproveGoalCandidate(Candidate.Id,Text("initial approval"))==Rejection::None,"first approval succeeds");
        Expect(Match.ApproveGoalCandidate(Candidate.Id,Text("duplicate approval"))==Rejection::CandidateAlreadyApproved,"double approval is explicit rejection");
        int BlueScore=0;
        Expect(Match.GetOfficialSetScore(0U,Team::Blue,BlueScore)==Rejection::None&&BlueScore==1,"approved candidate increments official score");
        Expect(Match.CancelGoalCandidate(Candidate.Id,Text("video review reverses goal"))==Rejection::None,"approved candidate can be cancelled");
        Expect(Match.GetOfficialSetScore(0U,Team::Blue,BlueScore)==Rejection::None&&BlueScore==0,"cancelled candidate is removed from official score");
        Expect(Match.CancelGoalCandidate(Candidate.Id,Text("duplicate cancellation"))==Rejection::CandidateAlreadyCancelled,"double cancellation is explicit rejection");
        Expect(Match.RequestGoalReReview(Candidate.Id,Text("referee requests second review"))==Rejection::None,"cancelled candidate can enter re-review");
        Expect(Match.GetOfficialSetScore(0U,Team::Blue,BlueScore)==Rejection::None&&BlueScore==0,"under-review candidate remains unofficial");
        Expect(Match.ApproveGoalCandidate(Candidate.Id,Text("second review approval"))==Rejection::None,"re-reviewed candidate can be approved");
        GoalCandidate Stored{};
        Expect(Match.GetCandidate(0U,Stored)==Rejection::None&&Stored.Status==CandidateStatus::Approved,"candidate state records final referee decision");
        Expect(Match.GetRevisionCount()==5U,"candidate revisions are append-only and ordered");
    }

    {
        Ledger Match;
        const CommandResult Pending=Match.RecordGoalCandidate(Team::Red,20.0,Text("unreviewed candidate"));
        Expect(Pending.Accepted(),"pending candidate is stored");
        Expect(Match.CloseCurrentSetAtFinalWhistle(180.0,Text("whistle with pending review"))==Rejection::None,"whistle accepts pending review ledger");
        Expect(Match.CommitCurrentSet(Text("attempt before review"))==Rejection::CandidateReviewUnresolved,"set cannot commit while candidate is pending");
        Expect(Match.CancelGoalCandidate(Pending.Id,Text("referee rejects pending candidate"))==Rejection::None,"pending candidate can be cancelled during set review");
        Expect(Match.CommitCurrentSet(Text("review completed"))==Rejection::None,"set commits after cancellation resolves review");
    }

    {
        Ledger Match;
        int Warnings=0;
        Expect(Match.IssueWarning(Team::Blue,Text("first warning"))==Rejection::None,"first warning is recorded");
        Expect(Match.GetWarnings(Team::Blue,Warnings)==Rejection::None&&Warnings==1,"warning count starts at one");
        CloseAndCommitDraw(Match,"first warning set closes","first warning set commits");
        Expect(Match.IssueWarning(Team::Blue,Text("second warning in later set"))==Rejection::None,"second cross-set warning is recorded");
        Expect(Match.GetWarnings(Team::Blue,Warnings)==Rejection::None&&Warnings==2,"warnings persist across sets");
        Expect(Match.GetPenaltyCount()==1U,"two accumulated warnings schedule one penalty");
        PenaltyRecord WarningPenalty{};
        Expect(Match.GetPenalty(0U,WarningPenalty)==Rejection::None&&WarningPenalty.Reason==PenaltyReason::TwoWarnings&&WarningPenalty.OffendingTeam==Team::Blue,"warning penalty retains rule-derived reason and offending team");
        Expect(Match.CloseCurrentSetAtFinalWhistle(180.0,Text("second set whistle"))==Rejection::None,"set with warning penalty reaches review");
        Expect(Match.BeginPenaltyShots(Text("striker versus keeper window opens"))==Rejection::None,"penalty phase starts after set");
        Expect(Match.RecordPenaltyShot(WarningPenalty.Id,PenaltyShotOutcome::Scored,PenaltyShotWindowS,Text("referee enters scored 1v1 shot"))==Rejection::None,"referee records five-second penalty outcome");
        int RedScore=0;
        Expect(Match.GetOfficialSetScore(1U,Team::Red,RedScore)==Rejection::None&&RedScore==1,"scored penalty is attributed to non-offending team");
        Expect(Match.CommitCurrentSet(Text("penalty result complete"))==Rejection::None,"set commits only after penalty entry");
        CloseAndCommitDraw(Match,"third set closes","third set commits");
        Expect(Match.GetMatchOutcome()==MatchOutcome::RedWin,"one set win plus two draws is a match win");
        Expect(Match.FinalizeMatch(Text("referee locks result"))==Rejection::None,"match finalization locks the result");
        Expect(Match.GetWarnings(Team::Blue,Warnings)==Rejection::None&&Warnings==0,"warnings reset after finalized match");
    }

    {
        Ledger Match;
        const CommandResult Penalty=Match.SchedulePenalty(Team::Red,PenaltyReason::OwnGoalReverseEntry,Text("reverse entry penalty"));
        Expect(Penalty.Accepted(),"source-listed reverse-entry penalty is scheduled");
        Expect(Match.CloseCurrentSetAtFinalWhistle(180.0,Text("penalty set whistle"))==Rejection::None,"penalty set closes");
        Expect(Match.BeginPenaltyShots(Text("penalty check"))==Rejection::None,"penalty phase opens with no unresolved goals");
        Expect(Match.RecordPenaltyShot(Penalty.Id,PenaltyShotOutcome::Scored,std::numeric_limits<double>::quiet_NaN(),Text("bad elapsed"))==Rejection::NonFiniteInput,"non-finite penalty elapsed time is rejected");
        Expect(Match.RecordPenaltyShot(Penalty.Id,PenaltyShotOutcome::Scored,5.001,Text("late penalty"))==Rejection::PenaltyShotTimeOutOfRange,"penalty elapsed time beyond five seconds is rejected");
        Expect(Match.RecordPenaltyShot(Penalty.Id,static_cast<PenaltyShotOutcome>(9U),1.0,Text("bad outcome"))==Rejection::InvalidPenaltyOutcome,"invalid penalty outcome is rejected");
        Expect(Match.RecordPenaltyShot(Penalty.Id,PenaltyShotOutcome::Missed,0.0,Text("referee enters miss"))==Rejection::None,"zero-second recorded miss is within penalty window");
        Expect(Match.RecordPenaltyShot(Penalty.Id,PenaltyShotOutcome::Missed,1.0,Text("duplicate outcome"))==Rejection::PenaltyNotPending,"penalty outcome cannot be entered twice");
        Expect(Match.CommitCurrentSet(Text("penalty review complete"))==Rejection::None,"missed penalty set commits");
    }

    {
        Ledger Match;
        Expect(Match.SetActivePlayerCount(Team::Blue,-1)==Rejection::ActivePlayersOutOfRange,"negative active-player count is rejected");
        Expect(Match.SetActivePlayerCount(Team::Blue,6)==Rejection::ActivePlayersOutOfRange,"more than five active players is rejected");
        Expect(Match.SetActivePlayerCount(Team::Blue,MinimumActivePlayers)==Rejection::None,"three active players is the inclusive minimum");
        Expect(Match.DeclareForfeit(Team::Blue,ForfeitScope::Set,ForfeitBasis::BelowMinimumActivePlayers,Text("incorrect minimum claim"))==Rejection::ForfeitBasisNotMet,"minimum-player forfeit basis rejects a team still at three");
        Expect(Match.SetActivePlayerCount(Team::Blue,2)==Rejection::None,"two active players can be recorded for referee review");
        Expect(Match.DeclareForfeit(Team::Blue,ForfeitScope::Set,ForfeitBasis::BelowMinimumActivePlayers,Text("below three active players"))==Rejection::None,"set forfeit is explicit when the team is below minimum");
        SetOutcome Outcome=SetOutcome::Undecided;
        Expect(Match.GetSetOutcome(0U,Outcome)==Rejection::None&&Outcome==SetOutcome::RedWin,"set forfeit awards the set to the opposing team");

        Ledger MatchForfeit;
        Expect(MatchForfeit.DeclareForfeit(Team::Red,ForfeitScope::Match,ForfeitBasis::RefereeDeclaredOtherGuidanceRequired,Text("tournament guidance reviewed"))==Rejection::None,"match forfeit supports explicit guidance-required referee reason");
        Expect(MatchForfeit.RecordGoalCandidate(Team::Blue,1.0,Text("after match forfeit")).Reason==Rejection::MatchComplete,"no new candidate follows declared match forfeit");
        Expect(MatchForfeit.FinalizeMatch(Text("lock declared match forfeit"))==Rejection::None&&MatchForfeit.GetMatchOutcome()==MatchOutcome::BlueWin,"finalized match forfeit has opposing winner");
    }

    {
        Ledger DisabledPolicy;
        Expect(DisabledPolicy.RequestStrikerRoleTransfer(Team::Blue,1,0,Text("disabled policy request")).Reason==Rejection::RoleTransferPolicyDisabled,"role transfer requires configured policy");
        MatchConfiguration Configuration{};
        Configuration.TransferPolicy.Enabled=true;
        Ledger Match(Configuration);
        Expect(Match.RequestStrikerRoleTransfer(Team::Blue,7,0,Text("wrong guide")).Reason==Rejection::RoleTransferParticipantsDoNotMatch,"transfer request validates live guide and striker participants");
        const CommandResult Request=Match.RequestStrikerRoleTransfer(Team::Blue,1,0,Text("guide requests striker succession"));
        Expect(Request.Accepted(),"configured guide-to-striker request is recorded");
        Expect(Match.RequestStrikerRoleTransfer(Team::Blue,1,0,Text("duplicate transfer request")).Reason==Rejection::RoleTransferAlreadyRequested,"one pending role transfer per team is enforced");
        Expect(Match.ApproveStrikerRoleTransfer(Request.Id,Text("referee approves policy-based transfer"))==Rejection::None,"referee approval changes current striker");
        int Striker=-1;
        int Guide=-1;
        Expect(Match.GetCurrentStrikerParticipant(Team::Blue,Striker)==Rejection::None&&Striker==1,"approved guide becomes current striker");
        Expect(Match.GetCurrentGuideParticipant(Team::Blue,Guide)==Rejection::None&&Guide==-1,"former guide slot is intentionally unassigned after succession");
        RoleTransferRequest Stored{};
        Expect(Match.GetRoleTransfer(0U,Stored)==Rejection::None&&Stored.Status==RoleTransferStatus::Approved&&Stored.TournamentGuidanceRequired,"role transfer record remains marked for tournament guidance");
    }

    {
        Ledger Match;
        const CommandResult BlueGoal=Match.RecordGoalCandidate(Team::Blue,10.0,Text("first-set blue goal"));
        Expect(BlueGoal.Accepted(),"finalization correction fixture records goal");
        Expect(Match.ApproveGoalCandidate(BlueGoal.Id,Text("approve first-set blue goal"))==Rejection::None,"fixture goal is approved");
        Expect(Match.CloseCurrentSetAtFinalWhistle(180.0,Text("first set whistle"))==Rejection::None,"first fixture set closes");
        Expect(Match.CommitCurrentSet(Text("first fixture set commits"))==Rejection::None,"first fixture set commits");
        CloseAndCommitDraw(Match,"second fixture set closes","second fixture set commits");
        CloseAndCommitDraw(Match,"third fixture set closes","third fixture set commits");
        Expect(Match.GetMatchOutcome()==MatchOutcome::BlueWin,"one blue win plus two draws is provisional win");
        Expect(Match.FinalizeMatch(Text("initial final result"))==Rejection::None,"initial final result locks");
        Expect(Match.CancelGoalCandidate(BlueGoal.Id,Text("late correction without reopen"))==Rejection::FinalizedLocked,"finalized ledger rejects silent correction");
        const ReasonText EmptyReason{};
        Expect(Match.ReopenFinalizedMatch(EmptyReason)==Rejection::InvalidReasonText,"reopen requires explicit reason text");
        Expect(Match.ReopenFinalizedMatch(Text("video evidence requires correction"))==Rejection::None&&Match.AreCorrectionsOpen(),"explicit reopen is logged and unlocks correction path");
        Expect(Match.CancelGoalCandidate(BlueGoal.Id,Text("video evidence cancels first goal"))==Rejection::None,"reopened ledger permits reviewed correction");
        int BlueScore=1;
        Expect(Match.GetOfficialSetScore(0U,Team::Blue,BlueScore)==Rejection::None&&BlueScore==0,"reopened cancellation updates derived official score");
        Expect(Match.FinalizeMatch(Text("corrected result is locked"))==Rejection::None&&Match.GetMatchOutcome()==MatchOutcome::Draw,"corrected three-draw result re-finalizes as draw");
    }

    {
        Ledger Match;
        const ReasonText EmptyReason{};
        Expect(Match.RecordGoalCandidate(static_cast<Team>(9U),10.0,Text("invalid team")).Reason==Rejection::InvalidTeam,"invalid team enum is rejected");
        Expect(Match.RecordGoalCandidate(Team::Blue,std::numeric_limits<double>::infinity(),Text("infinite time")).Reason==Rejection::NonFiniteInput,"non-finite candidate time is rejected");
        Expect(Match.RecordGoalCandidate(Team::Blue,10.0,EmptyReason).Reason==Rejection::InvalidReasonText,"empty evidence text is rejected");
        MatchConfiguration BadConfiguration{};
        BadConfiguration.TransferPolicy.InitialGuideParticipant[0U]=BadConfiguration.TransferPolicy.InitialStrikerParticipant[0U];
        Ledger Invalid(BadConfiguration);
        Expect(Invalid.GetConfigurationStatus()==Rejection::InvalidConfiguration,"invalid role policy configuration is rejected deterministically");
        Expect(Invalid.SetActivePlayerCount(Team::Blue,3)==Rejection::InvalidConfiguration,"invalid configuration blocks mutations");
    }

    std::cout<<Checks<<" checks, "<<Failures<<" failures\n";
    return Failures==0?0:1;
}
