#include "TopgunCourseDraft.h"
#include "TopgunRecords.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

namespace
{
using namespace ImperialTopgunLocal;

int Checks=0;
int Failures=0;

void Expect(const bool Good,const char* const Name)
{
    ++Checks;
    if (!Good)
    {
        ++Failures;
        std::cerr<<"FAIL "<<Name<<'\n';
    }
}

bool Near(const double Left,const double Right,const double Tolerance=0.0000001)
{
    return std::abs(Left-Right)<=Tolerance;
}

AttemptConditions StandardConditions()
{
    return {7U,11U,13U,17U};
}

StartObservation StandardStart()
{
    StartObservation Observation{};
    Observation.GuardCenterM={-6.0,0.0,2.0};
    Observation.SimulationClockS=0.0;
    Observation.TimeScale=1.0;
    Observation.Conditions=StandardConditions();
    return Observation;
}

FrameObservation FrameAt(const Vec3M Point,const double ClockS,const double PhysicsDeltaS,
                         const AttemptConditions& Conditions=StandardConditions())
{
    FrameObservation Observation{};
    Observation.GuardCenterM=Point;
    Observation.SimulationClockS=ClockS;
    Observation.PhysicsDeltaS=PhysicsDeltaS;
    Observation.TimeScale=1.0;
    Observation.Conditions=Conditions;
    return Observation;
}

JudgeResult CompleteNormalCourse(CourseJudge& Judge,const std::uint32_t CollisionCount=0U)
{
    Judge.Update(FrameAt({-5.0,0.0,2.0},0.10,0.10));
    Judge.Update(FrameAt({-2.0,1.0,2.0},0.20,0.10));
    Judge.Update(FrameAt({0.0,0.0,2.0},0.30,0.10));
    Judge.Update(FrameAt({2.0,-1.0,2.0},0.40,0.10));
    FrameObservation Finish=FrameAt({5.5,0.0,2.0},0.50,0.10);
    Finish.CollisionCount=CollisionCount;
    return Judge.Update(Finish);
}

std::array<std::uint8_t,16U> RandomBits(const unsigned int Seed)
{
    std::array<std::uint8_t,16U> Bytes{};
    for (std::size_t Index=0U;Index<Bytes.size();++Index)
    {
        const unsigned int Value=Seed+static_cast<unsigned int>(Index*17U);
        Bytes[Index]=static_cast<std::uint8_t>(Value&0xFFU);
    }
    return Bytes;
}

std::string Uuid(const unsigned int Seed)
{
    return FormatUuidV4(RandomBits(Seed));
}

ConditionKey ConditionA()
{
    return {"class40-aircraft-a","physics-s2","gravity-9.81","assist-off","build-0715"};
}

ConditionKey ConditionB()
{
    return {"class40-aircraft-b","physics-s2","gravity-9.81","assist-off","build-0715"};
}

AttemptRecord ValidAttempt(const std::string& AttemptId,const std::string& ProfileId,
                           const double FinishTimeS,const ConditionKey& Conditions)
{
    AttemptRecord Attempt{};
    Attempt.AttemptId=AttemptId;
    Attempt.ProfileId=ProfileId;
    Attempt.CourseVersion="topgun-s2-draft-1";
    Attempt.RuleVersion="fida-class40-2026";
    Attempt.Conditions=Conditions;
    Attempt.FinishTimeS=FinishTimeS;
    Attempt.Valid=true;
    Attempt.InvalidReason=CourseRejection::None;
    Attempt.CollisionCount=0U;
    Attempt.OfficialRecordEligible=true;
    Attempt.LocalUnverified=true;
    return Attempt;
}

std::uint64_t TestFnv1a64(const std::string& Text)
{
    std::uint64_t Hash=14695981039346656037ULL;
    for (const char Character:Text)
    {
        Hash^=static_cast<std::uint64_t>(static_cast<unsigned char>(Character));
        Hash*=1099511628211ULL;
    }
    return Hash;
}

std::string TestHex64(const std::uint64_t Value)
{
    static constexpr char Hex[]="0123456789abcdef";
    std::string Result(16U,'0');
    for (std::size_t Index=0U;Index<16U;++Index)
    {
        const unsigned int Shift=static_cast<unsigned int>((15U-Index)*4U);
        Result[Index]=Hex[static_cast<std::size_t>((Value>>Shift)&0x0FULL)];
    }
    return Result;
}

std::string WithChecksum(const std::string& Payload)
{
    return Payload+"CHECKSUM|"+TestHex64(TestFnv1a64(Payload))+"\n";
}
}

int main()
{
    {
        const CourseDefinition Definition=CourseDefinition::TopgunS2Draft1();
        Expect(std::string(Definition.Version)=="topgun-s2-draft-1","draft course has the required version");
        Expect(Near(Definition.GuardRadiusM,0.20),"course keeps the FIDA Class40 guard radius");
        Expect(Definition.PillarOneSide.PlaneXM<Definition.BetweenPillars.PlaneXM &&
            Definition.BetweenPillars.PlaneXM<Definition.PillarTwoOppositeSide.PlaneXM,"course planes are forward ordered");

        CourseJudge Judge;
        const JudgeResult Started=Judge.Begin(StandardStart());
        Expect(Started.AcceptedFrame && Started.Stage==CourseStage::AwaitStart,"normal course arms a waiting start state");
        const JudgeResult Finish=CompleteNormalCourse(Judge);
        Expect(Finish.Finished && Finish.Stage==CourseStage::Finished && Finish.Rejection==CourseRejection::None,"normal S order reaches a separate finish line");
        Expect(Near(Finish.EventClockS,0.50) && Near(Finish.ElapsedTimeS,0.45),"start and finish times use interpolated simulation-clock crossings");
        Expect(Finish.CollisionCount==0U && Finish.OfficialRecordEligible,"clean completion is eligible under the draft policy");
        Expect(!Judge.Update(FrameAt({6.0,0.0,2.0},0.60,0.10)).AcceptedFrame,"finished course ignores later frames");
    }

    {
        CourseJudge Judge;
        Expect(Judge.Begin(StandardStart()).AcceptedFrame,"fast sweep setup arms");
        Judge.Update(FrameAt({-5.0,2.0,2.0},0.10,0.10));
        const JudgeResult Swept=Judge.Update(FrameAt({0.0,0.25,2.0},0.20,0.10));
        Expect(Swept.AcceptedFrame && Swept.Advanced && Swept.Stage==CourseStage::PassPillarTwoOppositeSide,
            "one swept segment recognizes the first-side and between-pillar zones in order");
        const JudgeResult ReverseResult=Judge.Update(FrameAt({-5.0,0.0,2.0},0.30,0.10));
        Expect(ReverseResult.Stage==CourseStage::Invalid && ReverseResult.Rejection==CourseRejection::ReverseDirection,
            "a swept attempt still rejects later reverse movement");
    }

    {
        CourseJudge Judge;
        Expect(Judge.Begin(StandardStart()).AcceptedFrame,"fast complete setup arms");
        Judge.Update(FrameAt({-5.0,2.0,2.0},0.10,0.10));
        Judge.Update(FrameAt({0.0,0.25,2.0},0.20,0.10));
        const JudgeResult PillarTwo=Judge.Update(FrameAt({2.0,-1.0,2.0},0.30,0.10));
        const JudgeResult Finish=Judge.Update(FrameAt({5.5,0.0,2.0},0.40,0.10));
        Expect(PillarTwo.Stage==CourseStage::PassFinish && Finish.Finished,"swept-zone course still requires opposite side then finish");
        Expect(Near(Finish.ElapsedTimeS,0.35),"fast crossings preserve fractional start timing");
    }

    {
        CourseJudge WrongSide;
        WrongSide.Begin(StandardStart());
        WrongSide.Update(FrameAt({-5.0,0.0,2.0},0.10,0.10));
        const JudgeResult Result=WrongSide.Update(FrameAt({-2.0,0.0,2.0},0.20,0.10));
        Expect(Result.Stage==CourseStage::Invalid && Result.Rejection==CourseRejection::WrongSide,"centreline cannot replace the required first pillar side");

        CourseJudge Skipped;
        Skipped.Begin(StandardStart());
        Skipped.Update(FrameAt({-5.0,0.0,2.0},0.10,0.10));
        Skipped.Update(FrameAt({-2.0,1.0,2.0},0.20,0.10));
        const JudgeResult SkippedResult=Skipped.Update(FrameAt({0.0,0.50,2.0},0.30,0.10));
        Expect(SkippedResult.Stage==CourseStage::Invalid && SkippedResult.Rejection==CourseRejection::SkippedZone,"out-of-zone middle crossing is a skipped required zone");

        CourseJudge Boundary;
        Boundary.Begin(StandardStart());
        Boundary.Update(FrameAt({-5.0,0.0,2.0},0.10,0.10));
        const JudgeResult BoundaryResult=Boundary.Update(FrameAt({-2.0,0.949999,2.0},0.20,0.10));
        Expect(BoundaryResult.Stage==CourseStage::Invalid && BoundaryResult.Rejection==CourseRejection::WrongSide,"whole-guard side-zone clearance rejects a sub-radius boundary point");
    }

    {
        CourseJudge Reverse;
        Reverse.Begin(StandardStart());
        Reverse.Update(FrameAt({-5.0,0.0,2.0},0.10,0.10));
        const JudgeResult Result=Reverse.Update(FrameAt({-5.1,0.0,2.0},0.20,0.10));
        Expect(Result.Rejection==CourseRejection::ReverseDirection,"any reverse progress invalidates an active attempt");

        CourseJudge Collision;
        Collision.Begin(StandardStart());
        const JudgeResult Finish=CompleteNormalCourse(Collision,2U);
        Expect(Finish.Finished && Finish.CollisionCount==2U && !Finish.OfficialRecordEligible,"collision continues practice but removes default official eligibility");

        CollisionPolicy AllowCollision{};
        AllowCollision.RequireNoCollisionForOfficial=false;
        CourseJudge Alternate(CourseDefinition::TopgunS2Draft1(),AllowCollision);
        Alternate.Begin(StandardStart());
        const JudgeResult AlternateFinish=CompleteNormalCourse(Alternate,1U);
        Expect(AlternateFinish.Finished && AlternateFinish.OfficialRecordEligible,"explicit alternative draft can retain collision eligibility");
    }

    {
        CourseDefinition InvalidDefinition=CourseDefinition::TopgunS2Draft1();
        InvalidDefinition.MaximumFrameStepS=0.0;
        CourseJudge InvalidCourse(InvalidDefinition);
        Expect(InvalidCourse.Begin(StandardStart()).Rejection==CourseRejection::CourseDefinitionInvalid,"invalid course parameters reject before timing starts");

        StartObservation BadStart=StandardStart();
        BadStart.GuardCenterM.XM=101.0;
        CourseJudge OutOfRange;
        Expect(OutOfRange.Begin(BadStart).Rejection==CourseRejection::NonFiniteInput,"out-of-range start telemetry is rejected");

        StartObservation NonFiniteStart=StandardStart();
        NonFiniteStart.SimulationClockS=std::numeric_limits<double>::quiet_NaN();
        CourseJudge NonFinite;
        Expect(NonFinite.Begin(NonFiniteStart).Rejection==CourseRejection::NonFiniteInput,"non-finite start clock is rejected");
    }

    {
        CourseJudge Reset;
        Reset.Begin(StandardStart());
        FrameObservation ResetFrame=FrameAt({-5.0,0.0,2.0},0.10,0.10);
        ResetFrame.ExplicitReset=true;
        Expect(Reset.Update(ResetFrame).Rejection==CourseRejection::ExplicitReset,"explicit reset invalidates an armed attempt");

        CourseJudge Pause;
        Pause.Begin(StandardStart());
        FrameObservation PauseFrame=FrameAt({-5.0,0.0,2.0},0.10,0.10);
        PauseFrame.Paused=true;
        Expect(Pause.Update(PauseFrame).Rejection==CourseRejection::Paused,"pause invalidates an armed attempt");

        CourseJudge Scale;
        Scale.Begin(StandardStart());
        FrameObservation ScaleFrame=FrameAt({-5.0,0.0,2.0},0.10,0.10);
        ScaleFrame.TimeScale=1.01;
        Expect(Scale.Update(ScaleFrame).Rejection==CourseRejection::AbnormalTimeScale,"non-normal time scale invalidates an attempt");

        CourseJudge NoClockAdvance;
        NoClockAdvance.Begin(StandardStart());
        Expect(NoClockAdvance.Update(FrameAt({-5.0,0.0,2.0},0.0,0.10)).Rejection==CourseRejection::NonMonotonicClock,"non-monotonic simulation clock is rejected");

        CourseJudge ZeroPhysics;
        ZeroPhysics.Begin(StandardStart());
        Expect(ZeroPhysics.Update(FrameAt({-5.0,0.0,2.0},0.10,0.0)).Rejection==CourseRejection::PhysicsTimeGap,"zero physics advance is rejected");

        CourseJudge LargePhysics;
        LargePhysics.Begin(StandardStart());
        Expect(LargePhysics.Update(FrameAt({-5.0,0.0,2.0},0.30,0.30)).Rejection==CourseRejection::PhysicsTimeGap,"oversized physics gap is rejected");

        CourseJudge MismatchedPhysics;
        MismatchedPhysics.Begin(StandardStart());
        Expect(MismatchedPhysics.Update(FrameAt({-5.0,0.0,2.0},0.10,0.09)).Rejection==CourseRejection::PhysicsTimeGap,"clock versus physics-time gap is rejected");

        CourseJudge BadPosition;
        BadPosition.Begin(StandardStart());
        Expect(BadPosition.Update(FrameAt({std::numeric_limits<double>::infinity(),0.0,2.0},0.10,0.10)).Rejection==CourseRejection::NonFiniteInput,"non-finite position is rejected");
    }

    {
        for (const CourseRejection Expected:{CourseRejection::AircraftSwap,CourseRejection::PhysicsSettingChanged,
            CourseRejection::GravitySettingChanged,CourseRejection::AssistSettingChanged})
        {
            CourseJudge Judge;
            Judge.Begin(StandardStart());
            AttemptConditions Changed=StandardConditions();
            if (Expected==CourseRejection::AircraftSwap) ++Changed.AircraftId;
            if (Expected==CourseRejection::PhysicsSettingChanged) ++Changed.PhysicsRevision;
            if (Expected==CourseRejection::GravitySettingChanged) ++Changed.GravityRevision;
            if (Expected==CourseRejection::AssistSettingChanged) ++Changed.AssistRevision;
            const JudgeResult Result=Judge.Update(FrameAt({-5.0,0.0,2.0},0.10,0.10,Changed));
            Expect(Result.Rejection==Expected,"aircraft and fixed-condition revisions invalidate a record attempt");
        }
    }

    {
        const std::string Formatted=Uuid(1U);
        Expect(IsRfc4122V4Uuid(Formatted) && Formatted[14U]=='4' && (Formatted[19U]=='8' || Formatted[19U]=='9' || Formatted[19U]=='a' || Formatted[19U]=='b'),
            "caller-supplied random bits format an RFC 4122 v4 UUID");
        Expect(!IsRfc4122V4Uuid("not-a-uuid") && !IsRfc4122V4Uuid("00000000-0000-1000-8000-000000000000"),"non-v4 UUID forms are rejected");
        Expect(LocalRecordBook::ValidateNickname("한글 Pilot 7")==ProfileValidation::None,"UTF-8 user nickname is accepted");
        Expect(LocalRecordBook::ValidateNickname(std::string(25U,'A'))==ProfileValidation::NicknameTooLong,"nickname code-point maximum is enforced");
        Expect(LocalRecordBook::ValidateNickname("   ")==ProfileValidation::NicknameEmpty,"all-space nickname is rejected");
        Expect(LocalRecordBook::ValidateNickname(std::string("bad\x01",4U))==ProfileValidation::NicknameInvalidUtf8OrControl,"nickname control characters are rejected");

        LocalProfile Korean{Uuid(2U),"동일 이름","KR",true};
        LocalProfile DuplicateName{Uuid(3U),"동일 이름","US",true};
        LocalProfile BadCountry{Uuid(4U),"Bad","ZZ",true};
        Expect(LocalRecordBook::ValidateProfile(Korean)==ProfileValidation::None && LocalRecordBook::ValidateProfile(DuplicateName)==ProfileValidation::None,
            "duplicate nicknames are independently valid profiles");
        Expect(LocalRecordBook::ValidateProfile(BadCountry)==ProfileValidation::NationalityInvalid,"unknown ISO alpha-2 country is rejected rather than inferred");

        LocalRecordBook Book;
        Expect(Book.AddProfile(Korean)==RecordError::None && Book.AddProfile(DuplicateName)==RecordError::None,"profiles with duplicate names retain separate UUID identities");
        Expect(Book.AddProfile(Korean)==RecordError::DuplicateProfileId,"duplicate profile UUID is rejected");
        Expect(Book.RenameProfile(Korean.Id,"새|이름\\A")==RecordError::None && Book.GetProfiles()[0U].Id==Korean.Id,
            "nickname change preserves the profile identity");

        const AttemptRecord First=ValidAttempt(Uuid(10U),Korean.Id,12.0,ConditionA());
        const AttemptRecord PersonalBest=ValidAttempt(Uuid(11U),Korean.Id,10.0,ConditionA());
        const AttemptRecord TiedLocal=ValidAttempt(Uuid(12U),DuplicateName.Id,10.0,ConditionA());
        const AttemptRecord OtherCondition=ValidAttempt(Uuid(13U),Korean.Id,5.0,ConditionB());
        Expect(Book.AddAttempt(First)==RecordError::None && Book.AddAttempt(PersonalBest)==RecordError::None &&
            Book.AddAttempt(TiedLocal)==RecordError::None && Book.AddAttempt(OtherCondition)==RecordError::None,"valid local attempts append with their full condition key");
        Expect(Book.AddAttempt(PersonalBest)==RecordError::DuplicateAttemptId,"duplicate attempt UUID is rejected");
        AttemptRecord UnknownProfile=ValidAttempt(Uuid(14U),Uuid(99U),8.0,ConditionA());
        Expect(Book.AddAttempt(UnknownProfile)==RecordError::ProfileNotFound,"attempt cannot refer to a missing local profile");

        AttemptRecord InvalidValidity=ValidAttempt(Uuid(15U),Korean.Id,9.0,ConditionA());
        InvalidValidity.InvalidReason=CourseRejection::WrongSide;
        Expect(Book.AddAttempt(InvalidValidity)==RecordError::InvalidAttempt,"valid record cannot carry an invalidation reason");
        AttemptRecord InvalidRun=ValidAttempt(Uuid(16U),Korean.Id,0.0,ConditionA());
        InvalidRun.Valid=false;
        InvalidRun.InvalidReason=CourseRejection::ReverseDirection;
        InvalidRun.OfficialRecordEligible=false;
        Expect(Book.AddAttempt(InvalidRun)==RecordError::None,"invalid local attempt keeps its explicit rejection reason");

        const RecordBucket BucketA{"topgun-s2-draft-1","fida-class40-2026",ConditionA()};
        AttemptRecord FoundPersonal{};
        AttemptRecord FoundLocal{};
        Expect(Book.FindPersonalBest(Korean.Id,BucketA,FoundPersonal) && Near(FoundPersonal.FinishTimeS,10.0),"personal best uses only the same versioned condition key");
        Expect(Book.FindLocalBest(BucketA,FoundLocal) && FoundLocal.AttemptId==PersonalBest.AttemptId,"equal local times use earlier achievement as deterministic tie-break");
        Expect(FoundLocal.AttemptId!=OtherCondition.AttemptId,"different aircraft condition never mixes into the local best");
        BestSummary Summary{};
        Expect(Book.GetBestSummaryForAttempt(First.AttemptId,Summary) && Near(Summary.PersonalDeltaS,2.0) && Near(Summary.LocalDeltaS,2.0),
            "best summary reports personal and local deltas under one condition key");
        Expect(!Book.IsOnlineVerified() && Book.GetAttempts()[0U].LocalUnverified,"stored records remain local and unverified");

        const std::string Serialized=Book.Serialize();
        LocalRecordBook Restored;
        Expect(LocalRecordBook::Deserialize(Serialized,Restored)==ParseError::None,"versioned escaped record text and checksum round-trip");
        Expect(Restored.GetProfiles().size()==2U && Restored.GetAttempts().size()==5U && Restored.GetProfiles()[0U].Nickname=="새|이름\\A",
            "round-trip preserves duplicate identities, escaped nickname, and invalid attempt");

        LocalRecordBook Preserved;
        Expect(Preserved.AddProfile(Korean)==RecordError::None,"preservation fixture starts with prior data");
        std::string Corrupted=Serialized;
        const std::size_t FirstProfile=Corrupted.find("PROFILE|");
        Corrupted[FirstProfile]='X';
        Expect(LocalRecordBook::Deserialize(Corrupted,Preserved)==ParseError::BadChecksum && Preserved.GetProfiles().size()==1U,
            "checksum corruption rejects without replacing previous loaded data");
        std::string Truncated=Serialized;
        Truncated.pop_back();
        Expect(LocalRecordBook::Deserialize(Truncated,Preserved)==ParseError::TruncatedInput && Preserved.GetProfiles().size()==1U,
            "truncated record text rejects without replacing previous data");
        std::string Future=Serialized;
        Future.replace(15U,1U,"9");
        Expect(LocalRecordBook::Deserialize(Future,Preserved)==ParseError::FutureVersion && Preserved.GetProfiles().size()==1U,
            "future schema is explicitly rejected before any data replacement");

        const std::size_t ChecksumStart=Serialized.rfind("CHECKSUM|");
        const std::string Payload=Serialized.substr(0U,ChecksumStart);
        const std::string BadEscape=WithChecksum(Payload+"PROFILE|bad\\q|name|KR\n");
        Expect(LocalRecordBook::Deserialize(BadEscape,Preserved)==ParseError::MalformedEscape && Preserved.GetProfiles().size()==1U,
            "well-checksummed malformed escaping rejects atomically with an explicit error");
    }

    std::cout<<"Scope: engine-independent local Topgun course and record-core tests only; no Unreal build, runtime, controller, network, or human flight test.\n";
    std::cout<<Checks<<" checks, "<<Failures<<" failures\n";
    return Failures==0 ? 0 : 1;
}
