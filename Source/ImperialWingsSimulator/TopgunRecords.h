#pragma once

#include "TopgunCourseDraft.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

// Local-only Topgun profile and record storage. This header never generates
// randomness, opens files, contacts a server, or treats a local record as
// online-verified.
namespace ImperialTopgunLocal
{
enum class ProfileValidation
{
    None,
    LocalVerificationClaimed,
    InvalidUuidV4,
    NicknameEmpty,
    NicknameTooLong,
    NicknameInvalidUtf8OrControl,
    NationalityInvalid
};

enum class RecordError
{
    None,
    InvalidProfile,
    DuplicateProfileId,
    ProfileNotFound,
    ProfileCapacityExceeded,
    InvalidAttempt,
    DuplicateAttemptId,
    AttemptCapacityExceeded,
    InvalidNickname,
    InvalidBucket
};

enum class ParseError
{
    None,
    EmptyInput,
    TruncatedInput,
    BadHeader,
    FutureVersion,
    UnsupportedVersion,
    BadChecksum,
    MalformedEscape,
    MalformedLine,
    InvalidProfile,
    InvalidAttempt,
    DuplicateIdentifier,
    CapacityExceeded
};

struct LocalProfile
{
    std::string Id;
    std::string Nickname;
    // Empty means no nationality was selected or inferred.
    std::string NationalityAlpha2;
    bool LocalUnverified=true;
};

struct ConditionKey
{
    std::string Aircraft;
    std::string Physics;
    std::string Gravity;
    std::string Assist;
    std::string Build;
};

struct RecordBucket
{
    std::string CourseVersion;
    std::string RuleVersion;
    ConditionKey Conditions{};
};

struct AttemptRecord
{
    std::string AttemptId;
    std::string ProfileId;
    std::string CourseVersion;
    std::string RuleVersion;
    ConditionKey Conditions{};
    double FinishTimeS=0.0;
    bool Valid=false;
    CourseRejection InvalidReason=CourseRejection::NotArmed;
    std::uint32_t CollisionCount=0U;
    // This is supplied by the course policy. It does not turn this local row
    // into an online-verified result.
    bool OfficialRecordEligible=false;
    bool LocalUnverified=true;
};

struct BestSummary
{
    bool HasPersonalBest=false;
    bool HasLocalBest=false;
    AttemptRecord PersonalBest{};
    AttemptRecord LocalBest{};
    // Candidate finish time minus each best time. A positive value means the
    // candidate was slower; zero also covers an equal-time later attempt.
    double PersonalDeltaS=0.0;
    double LocalDeltaS=0.0;
};

inline bool IsRfc4122V4Uuid(const std::string& Value)
{
    if (Value.size()!=36U || Value[8U]!='-' || Value[13U]!='-' || Value[18U]!='-' || Value[23U]!='-') return false;
    for (std::size_t Index=0U;Index<Value.size();++Index)
    {
        if (Index==8U || Index==13U || Index==18U || Index==23U) continue;
        const char Character=Value[Index];
        const bool IsDigit=Character>='0' && Character<='9';
        const bool IsLower=Character>='a' && Character<='f';
        const bool IsUpper=Character>='A' && Character<='F';
        if (!IsDigit && !IsLower && !IsUpper) return false;
    }
    const char Version=Value[14U];
    const char Variant=Value[19U];
    return Version=='4' && (Variant=='8' || Variant=='9' || Variant=='a' || Variant=='b' || Variant=='A' || Variant=='B');
}

inline std::string FormatUuidV4(const std::array<std::uint8_t,16U>& CallerRandomBits)
{
    std::array<std::uint8_t,16U> Bytes=CallerRandomBits;
    Bytes[6U]=static_cast<std::uint8_t>((Bytes[6U]&0x0FU)|0x40U);
    Bytes[8U]=static_cast<std::uint8_t>((Bytes[8U]&0x3FU)|0x80U);
    static constexpr char Hex[]="0123456789abcdef";
    std::string Result;
    Result.reserve(36U);
    for (std::size_t Index=0U;Index<Bytes.size();++Index)
    {
        if (Index==4U || Index==6U || Index==8U || Index==10U) Result.push_back('-');
        const std::uint8_t Byte=Bytes[Index];
        Result.push_back(Hex[static_cast<std::size_t>((Byte>>4U)&0x0FU)]);
        Result.push_back(Hex[static_cast<std::size_t>(Byte&0x0FU)]);
    }
    return Result;
}

class LocalRecordBook
{
public:
    static constexpr std::size_t MaximumProfiles=128U;
    static constexpr std::size_t MaximumAttempts=4096U;
    static constexpr unsigned int SerializationVersion=1U;

    static ProfileValidation ValidateProfile(const LocalProfile& Profile)
    {
        if (!Profile.LocalUnverified) return ProfileValidation::LocalVerificationClaimed;
        if (!IsRfc4122V4Uuid(Profile.Id)) return ProfileValidation::InvalidUuidV4;
        const ProfileValidation NicknameResult=ValidateNickname(Profile.Nickname);
        if (NicknameResult!=ProfileValidation::None) return NicknameResult;
        if (!Profile.NationalityAlpha2.empty() && !IsIso3166Alpha2(Profile.NationalityAlpha2))
            return ProfileValidation::NationalityInvalid;
        return ProfileValidation::None;
    }

    static ProfileValidation ValidateNickname(const std::string& Nickname)
    {
        if (Nickname.empty()) return ProfileValidation::NicknameEmpty;
        std::size_t Index=0U;
        std::size_t CodePointCount=0U;
        bool HasNonSpace=false;
        while (Index<Nickname.size())
        {
            std::uint32_t CodePoint=0U;
            if (!DecodeUtf8Scalar(Nickname,Index,CodePoint)) return ProfileValidation::NicknameInvalidUtf8OrControl;
            ++CodePointCount;
            if (CodePointCount>24U) return ProfileValidation::NicknameTooLong;
            if (CodePoint<=0x1FU || (CodePoint>=0x7FU && CodePoint<=0x9FU) || CodePoint==0x7FU)
                return ProfileValidation::NicknameInvalidUtf8OrControl;
            if (CodePoint!=0x20U && CodePoint!=0x3000U) HasNonSpace=true;
        }
        return HasNonSpace ? ProfileValidation::None : ProfileValidation::NicknameEmpty;
    }

    RecordError AddProfile(const LocalProfile& Profile)
    {
        if (ValidateProfile(Profile)!=ProfileValidation::None) return RecordError::InvalidProfile;
        if (FindProfile(Profile.Id)!=nullptr) return RecordError::DuplicateProfileId;
        if (Profiles.size()>=MaximumProfiles) return RecordError::ProfileCapacityExceeded;
        Profiles.push_back(Profile);
        return RecordError::None;
    }

    RecordError RenameProfile(const std::string& ProfileId,const std::string& NewNickname)
    {
        LocalProfile* const Profile=FindProfileMutable(ProfileId);
        if (Profile==nullptr) return RecordError::ProfileNotFound;
        const ProfileValidation Validation=ValidateNickname(NewNickname);
        if (Validation!=ProfileValidation::None) return RecordError::InvalidNickname;
        // Keep ProfileId unchanged so past and future attempts remain one player.
        Profile->Nickname=NewNickname;
        return RecordError::None;
    }

    RecordError AddAttempt(const AttemptRecord& Attempt)
    {
        if (!IsAttemptStructValid(Attempt)) return RecordError::InvalidAttempt;
        if (FindProfile(Attempt.ProfileId)==nullptr) return RecordError::ProfileNotFound;
        if (FindAttempt(Attempt.AttemptId)!=nullptr) return RecordError::DuplicateAttemptId;
        if (Attempts.size()>=MaximumAttempts) return RecordError::AttemptCapacityExceeded;
        Attempts.push_back(Attempt);
        return RecordError::None;
    }

    bool FindPersonalBest(const std::string& ProfileId,const RecordBucket& Bucket,AttemptRecord& OutBest) const
    {
        if (!IsBucketValid(Bucket)) return false;
        bool Found=false;
        std::size_t BestIndex=0U;
        for (std::size_t Index=0U;Index<Attempts.size();++Index)
        {
            const AttemptRecord& Candidate=Attempts[Index];
            if (Candidate.ProfileId!=ProfileId || !Candidate.Valid || !MatchesBucket(Candidate,Bucket)) continue;
            if (!Found || IsBetter(Candidate,Index,Attempts[BestIndex],BestIndex))
            {
                Found=true;
                BestIndex=Index;
            }
        }
        if (Found) OutBest=Attempts[BestIndex];
        return Found;
    }

    bool FindLocalBest(const RecordBucket& Bucket,AttemptRecord& OutBest) const
    {
        if (!IsBucketValid(Bucket)) return false;
        bool Found=false;
        std::size_t BestIndex=0U;
        for (std::size_t Index=0U;Index<Attempts.size();++Index)
        {
            const AttemptRecord& Candidate=Attempts[Index];
            if (!Candidate.Valid || !MatchesBucket(Candidate,Bucket)) continue;
            if (!Found || IsBetter(Candidate,Index,Attempts[BestIndex],BestIndex))
            {
                Found=true;
                BestIndex=Index;
            }
        }
        if (Found) OutBest=Attempts[BestIndex];
        return Found;
    }

    bool GetBestSummaryForAttempt(const std::string& AttemptId,BestSummary& OutSummary) const
    {
        const AttemptRecord* const Candidate=FindAttempt(AttemptId);
        if (Candidate==nullptr || !Candidate->Valid) return false;
        const RecordBucket Bucket{Candidate->CourseVersion,Candidate->RuleVersion,Candidate->Conditions};
        BestSummary Summary{};
        Summary.HasPersonalBest=FindPersonalBest(Candidate->ProfileId,Bucket,Summary.PersonalBest);
        Summary.HasLocalBest=FindLocalBest(Bucket,Summary.LocalBest);
        if (!Summary.HasPersonalBest || !Summary.HasLocalBest) return false;
        Summary.PersonalDeltaS=Candidate->FinishTimeS-Summary.PersonalBest.FinishTimeS;
        Summary.LocalDeltaS=Candidate->FinishTimeS-Summary.LocalBest.FinishTimeS;
        OutSummary=Summary;
        return true;
    }

    const std::vector<LocalProfile>& GetProfiles() const { return Profiles; }
    const std::vector<AttemptRecord>& GetAttempts() const { return Attempts; }
    bool IsOnlineVerified() const { return false; }

    std::string Serialize() const
    {
        std::string Payload="TOPGUN_RECORDS|1\n";
        for (const LocalProfile& Profile:Profiles)
        {
            Payload+="PROFILE|"+Escape(Profile.Id)+"|"+Escape(Profile.Nickname)+"|"+Escape(Profile.NationalityAlpha2)+"\n";
        }
        for (const AttemptRecord& Attempt:Attempts)
        {
            Payload+="ATTEMPT|"+Escape(Attempt.AttemptId)+"|"+Escape(Attempt.ProfileId)+"|"+
                Escape(Attempt.CourseVersion)+"|"+Escape(Attempt.RuleVersion)+"|"+
                Escape(Attempt.Conditions.Aircraft)+"|"+Escape(Attempt.Conditions.Physics)+"|"+
                Escape(Attempt.Conditions.Gravity)+"|"+Escape(Attempt.Conditions.Assist)+"|"+
                Escape(Attempt.Conditions.Build)+"|"+Hex64(DoubleBits(Attempt.FinishTimeS))+"|"+
                (Attempt.Valid ? "1" : "0")+"|"+std::to_string(static_cast<unsigned int>(Attempt.InvalidReason))+"|"+
                std::to_string(Attempt.CollisionCount)+"|"+(Attempt.OfficialRecordEligible ? "1" : "0")+"\n";
        }
        return Payload+"CHECKSUM|"+Hex64(Fnv1a64(Payload))+"\n";
    }

    // On every parser error InOut remains unchanged, so a caller can preserve
    // its previously loaded local records and show the specific failure.
    static ParseError Deserialize(const std::string& Text,LocalRecordBook& InOut)
    {
        if (Text.empty()) return ParseError::EmptyInput;
        if (Text.back()!='\n') return ParseError::TruncatedInput;
        if (Text.size()<2U) return ParseError::TruncatedInput;

        const std::size_t PreviousNewline=Text.rfind('\n',Text.size()-2U);
        const std::size_t ChecksumLineStart=PreviousNewline==std::string::npos ? 0U : PreviousNewline+1U;
        const std::string ChecksumLine=Text.substr(ChecksumLineStart,Text.size()-ChecksumLineStart-1U);
        static constexpr char Prefix[]="CHECKSUM|";
        const std::size_t PrefixLength=sizeof(Prefix)-1U;
        if (ChecksumLine.size()!=PrefixLength+16U || ChecksumLine.compare(0U,PrefixLength,Prefix)!=0)
            return ParseError::TruncatedInput;
        std::uint64_t ExpectedChecksum=0U;
        if (!ParseHex64(ChecksumLine.substr(PrefixLength),ExpectedChecksum)) return ParseError::BadChecksum;
        const std::string Payload=Text.substr(0U,ChecksumLineStart);
        const std::size_t HeaderNewline=Payload.find('\n');
        if (HeaderNewline==std::string::npos) return ParseError::TruncatedInput;
        const ParseError HeaderError=ParseHeader(Payload.substr(0U,HeaderNewline));
        if (HeaderError!=ParseError::None) return HeaderError;
        if (Fnv1a64(Payload)!=ExpectedChecksum) return ParseError::BadChecksum;

        LocalRecordBook Candidate{};
        bool HeaderSeen=false;
        std::size_t Position=0U;
        while (Position<Payload.size())
        {
            const std::size_t Newline=Payload.find('\n',Position);
            if (Newline==std::string::npos) return ParseError::TruncatedInput;
            const std::string Line=Payload.substr(Position,Newline-Position);
            Position=Newline+1U;
            if (!HeaderSeen)
            {
                const ParseError LoopHeaderError=ParseHeader(Line);
                if (LoopHeaderError!=ParseError::None) return LoopHeaderError;
                HeaderSeen=true;
                continue;
            }
            if (Line.empty()) return ParseError::MalformedLine;
            const ParseError LineError=ParseDataLine(Line,Candidate);
            if (LineError!=ParseError::None) return LineError;
        }
        if (!HeaderSeen) return ParseError::TruncatedInput;
        InOut=Candidate;
        return ParseError::None;
    }

private:
    std::vector<LocalProfile> Profiles{};
    std::vector<AttemptRecord> Attempts{};

    static bool DecodeUtf8Scalar(const std::string& Text,std::size_t& InOutIndex,std::uint32_t& OutCodePoint)
    {
        if (InOutIndex>=Text.size()) return false;
        const unsigned char First=static_cast<unsigned char>(Text[InOutIndex]);
        if (First<0x80U)
        {
            OutCodePoint=static_cast<std::uint32_t>(First);
            ++InOutIndex;
            return true;
        }
        std::size_t Extra=0U;
        std::uint32_t CodePoint=0U;
        if (First>=0xC2U && First<=0xDFU)
        {
            Extra=1U;
            CodePoint=static_cast<std::uint32_t>(First&0x1FU);
        }
        else if (First>=0xE0U && First<=0xEFU)
        {
            Extra=2U;
            CodePoint=static_cast<std::uint32_t>(First&0x0FU);
        }
        else if (First>=0xF0U && First<=0xF4U)
        {
            Extra=3U;
            CodePoint=static_cast<std::uint32_t>(First&0x07U);
        }
        else return false;
        if (Text.size()-InOutIndex<=Extra) return false;
        for (std::size_t Offset=1U;Offset<=Extra;++Offset)
        {
            const unsigned char Continuation=static_cast<unsigned char>(Text[InOutIndex+Offset]);
            if ((Continuation&0xC0U)!=0x80U) return false;
            CodePoint=(CodePoint<<6U)|static_cast<std::uint32_t>(Continuation&0x3FU);
        }
        if ((Extra==1U && CodePoint<0x80U) || (Extra==2U && CodePoint<0x800U) ||
            (Extra==3U && CodePoint<0x10000U) || CodePoint>0x10FFFFU ||
            (CodePoint>=0xD800U && CodePoint<=0xDFFFU)) return false;
        InOutIndex+=Extra+1U;
        OutCodePoint=CodePoint;
        return true;
    }

    static bool IsIso3166Alpha2(const std::string& Value)
    {
        if (Value.size()!=2U || Value[0U]<'A' || Value[0U]>'Z' || Value[1U]<'A' || Value[1U]>'Z') return false;
        // ISO 3166-1 alpha-2 assigned codes; a code is never inferred from IP,
        // locale, nickname, or system settings.
        static constexpr char Codes[]=
            "AD AE AF AG AI AL AM AO AQ AR AS AT AU AW AX AZ "
            "BA BB BD BE BF BG BH BI BJ BL BM BN BO BQ BR BS BT BV BW BY BZ "
            "CA CC CD CF CG CH CI CK CL CM CN CO CR CU CV CW CX CY CZ "
            "DE DJ DK DM DO DZ EC EE EG EH ER ES ET FI FJ FK FM FO FR "
            "GA GB GD GE GF GG GH GI GL GM GN GP GQ GR GS GT GU GW GY "
            "HK HM HN HR HT HU ID IE IL IM IN IO IQ IR IS IT JE JM JO JP "
            "KE KG KH KI KM KN KP KR KW KY KZ LA LB LC LI LK LR LS LT LU LV LY "
            "MA MC MD ME MF MG MH MK ML MM MN MO MP MQ MR MS MT MU MV MW MX MY MZ "
            "NA NC NE NF NG NI NL NO NP NR NU NZ OM PA PE PF PG PH PK PL PM PN PR PS PT PW PY "
            "QA RE RO RS RU RW SA SB SC SD SE SG SH SI SJ SK SL SM SN SO SR SS ST SV SX SY SZ "
            "TC TD TF TG TH TJ TK TL TM TN TO TR TT TV TW TZ UA UG UM US UY UZ VA VC VE VG VI VN VU "
            "WF WS YE YT ZA ZM ZW";
        for (std::size_t Index=0U;Index+1U<sizeof(Codes);Index+=3U)
        {
            if (Codes[Index]==Value[0U] && Codes[Index+1U]==Value[1U]) return true;
        }
        return false;
    }

    static bool IsMetadataTextValid(const std::string& Value,const std::size_t MaximumLength)
    {
        if (Value.empty() || Value.size()>MaximumLength) return false;
        for (const char Character:Value)
        {
            const bool IsLetter=(Character>='A' && Character<='Z') || (Character>='a' && Character<='z');
            const bool IsDigit=Character>='0' && Character<='9';
            const bool IsPunctuation=Character=='-' || Character=='_' || Character=='.' || Character==':' ||
                Character=='@' || Character=='+' || Character=='=' || Character=='/';
            if (!IsLetter && !IsDigit && !IsPunctuation) return false;
        }
        return true;
    }

    static bool IsConditionKeyValid(const ConditionKey& Conditions)
    {
        return IsMetadataTextValid(Conditions.Aircraft,96U) && IsMetadataTextValid(Conditions.Physics,96U) &&
            IsMetadataTextValid(Conditions.Gravity,96U) && IsMetadataTextValid(Conditions.Assist,96U) &&
            IsMetadataTextValid(Conditions.Build,96U);
    }

    static bool IsBucketValid(const RecordBucket& Bucket)
    {
        return IsMetadataTextValid(Bucket.CourseVersion,64U) && IsMetadataTextValid(Bucket.RuleVersion,64U) &&
            IsConditionKeyValid(Bucket.Conditions);
    }

    static bool IsKnownRejection(const CourseRejection Reason)
    {
        switch (Reason)
        {
        case CourseRejection::None:case CourseRejection::NotArmed:case CourseRejection::CourseDefinitionInvalid:
        case CourseRejection::NonFiniteInput:case CourseRejection::ExplicitReset:case CourseRejection::AircraftSwap:
        case CourseRejection::PhysicsSettingChanged:case CourseRejection::GravitySettingChanged:
        case CourseRejection::AssistSettingChanged:case CourseRejection::Paused:case CourseRejection::AbnormalTimeScale:
        case CourseRejection::NonMonotonicClock:case CourseRejection::PhysicsTimeGap:case CourseRejection::ReverseDirection:
        case CourseRejection::SkippedZone:case CourseRejection::WrongSide:return true;
        }
        return false;
    }

    static bool IsAttemptStructValid(const AttemptRecord& Attempt)
    {
        if (!Attempt.LocalUnverified || !IsRfc4122V4Uuid(Attempt.AttemptId) || !IsRfc4122V4Uuid(Attempt.ProfileId) ||
            !IsMetadataTextValid(Attempt.CourseVersion,64U) || !IsMetadataTextValid(Attempt.RuleVersion,64U) ||
            !IsConditionKeyValid(Attempt.Conditions) || !std::isfinite(Attempt.FinishTimeS) || Attempt.FinishTimeS<0.0 ||
            !IsKnownRejection(Attempt.InvalidReason)) return false;
        if (Attempt.Valid && (Attempt.FinishTimeS<=0.0 || Attempt.InvalidReason!=CourseRejection::None)) return false;
        if (!Attempt.Valid && (Attempt.InvalidReason==CourseRejection::None || Attempt.OfficialRecordEligible)) return false;
        return true;
    }

    static bool EqualConditions(const ConditionKey& Left,const ConditionKey& Right)
    {
        return Left.Aircraft==Right.Aircraft && Left.Physics==Right.Physics && Left.Gravity==Right.Gravity &&
            Left.Assist==Right.Assist && Left.Build==Right.Build;
    }

    static bool MatchesBucket(const AttemptRecord& Attempt,const RecordBucket& Bucket)
    {
        return Attempt.CourseVersion==Bucket.CourseVersion && Attempt.RuleVersion==Bucket.RuleVersion &&
            EqualConditions(Attempt.Conditions,Bucket.Conditions);
    }

    static bool IsBetter(const AttemptRecord& Candidate,const std::size_t CandidateIndex,
                         const AttemptRecord& Best,const std::size_t BestIndex)
    {
        return Candidate.FinishTimeS<Best.FinishTimeS ||
            (Candidate.FinishTimeS==Best.FinishTimeS && CandidateIndex<BestIndex);
    }

    const LocalProfile* FindProfile(const std::string& ProfileId) const
    {
        for (const LocalProfile& Profile:Profiles) if (Profile.Id==ProfileId) return &Profile;
        return nullptr;
    }

    LocalProfile* FindProfileMutable(const std::string& ProfileId)
    {
        for (LocalProfile& Profile:Profiles) if (Profile.Id==ProfileId) return &Profile;
        return nullptr;
    }

    const AttemptRecord* FindAttempt(const std::string& AttemptId) const
    {
        for (const AttemptRecord& Attempt:Attempts) if (Attempt.AttemptId==AttemptId) return &Attempt;
        return nullptr;
    }

    static std::string Escape(const std::string& Value)
    {
        std::string Result;
        Result.reserve(Value.size());
        for (const char Character:Value)
        {
            if (Character=='\\') Result+="\\\\";
            else if (Character=='|') Result+="\\p";
            else if (Character=='\n') Result+="\\n";
            else if (Character=='\r') Result+="\\r";
            else Result.push_back(Character);
        }
        return Result;
    }

    static bool SplitEscaped(const std::string& Line,std::vector<std::string>& OutFields)
    {
        OutFields.clear();
        std::string Field;
        for (std::size_t Index=0U;Index<Line.size();++Index)
        {
            const char Character=Line[Index];
            if (Character=='|')
            {
                OutFields.push_back(Field);
                Field.clear();
                continue;
            }
            if (Character!='\\')
            {
                Field.push_back(Character);
                continue;
            }
            if (Index+1U>=Line.size()) return false;
            ++Index;
            switch (Line[Index])
            {
            case '\\':Field.push_back('\\');break;
            case 'p':Field.push_back('|');break;
            case 'n':Field.push_back('\n');break;
            case 'r':Field.push_back('\r');break;
            default:return false;
            }
        }
        OutFields.push_back(Field);
        return true;
    }

    static std::uint64_t Fnv1a64(const std::string& Text)
    {
        std::uint64_t Hash=14695981039346656037ULL;
        for (const char Character:Text)
        {
            Hash^=static_cast<std::uint64_t>(static_cast<unsigned char>(Character));
            Hash*=1099511628211ULL;
        }
        return Hash;
    }

    static std::string Hex64(const std::uint64_t Value)
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

    static bool ParseHex64(const std::string& Text,std::uint64_t& OutValue)
    {
        if (Text.size()!=16U) return false;
        std::uint64_t Value=0U;
        for (const char Character:Text)
        {
            unsigned int Digit=0U;
            if (Character>='0' && Character<='9') Digit=static_cast<unsigned int>(Character-'0');
            else if (Character>='a' && Character<='f') Digit=static_cast<unsigned int>(Character-'a')+10U;
            else if (Character>='A' && Character<='F') Digit=static_cast<unsigned int>(Character-'A')+10U;
            else return false;
            Value=(Value<<4U)|static_cast<std::uint64_t>(Digit);
        }
        OutValue=Value;
        return true;
    }

    static std::uint64_t DoubleBits(const double Value)
    {
        std::uint64_t Bits=0U;
        static_assert(sizeof(Bits)==sizeof(Value),"double must be 64-bit for this local text schema");
        std::memcpy(&Bits,&Value,sizeof(Bits));
        return Bits;
    }

    static double DoubleFromBits(const std::uint64_t Bits)
    {
        double Value=0.0;
        std::memcpy(&Value,&Bits,sizeof(Value));
        return Value;
    }

    static bool ParseUnsigned(const std::string& Text,std::uint64_t& OutValue)
    {
        if (Text.empty()) return false;
        std::uint64_t Value=0U;
        for (const char Character:Text)
        {
            if (Character<'0' || Character>'9') return false;
            const std::uint64_t Digit=static_cast<std::uint64_t>(Character-'0');
            if (Value>(std::numeric_limits<std::uint64_t>::max()-Digit)/10U) return false;
            Value=Value*10U+Digit;
        }
        OutValue=Value;
        return true;
    }

    static bool ParseBoolean(const std::string& Text,bool& OutValue)
    {
        if (Text=="0") { OutValue=false; return true; }
        if (Text=="1") { OutValue=true; return true; }
        return false;
    }

    static ParseError ParseHeader(const std::string& Line)
    {
        static constexpr char Prefix[]="TOPGUN_RECORDS|";
        const std::size_t PrefixLength=sizeof(Prefix)-1U;
        if (Line.size()<=PrefixLength || Line.compare(0U,PrefixLength,Prefix)!=0) return ParseError::BadHeader;
        std::uint64_t Version=0U;
        if (!ParseUnsigned(Line.substr(PrefixLength),Version)) return ParseError::BadHeader;
        if (Version>static_cast<std::uint64_t>(SerializationVersion)) return ParseError::FutureVersion;
        if (Version!=static_cast<std::uint64_t>(SerializationVersion)) return ParseError::UnsupportedVersion;
        return ParseError::None;
    }

    static ParseError ParseDataLine(const std::string& Line,LocalRecordBook& InOutBook)
    {
        std::vector<std::string> Fields;
        if (!SplitEscaped(Line,Fields)) return ParseError::MalformedEscape;
        if (Fields.empty()) return ParseError::MalformedLine;
        if (Fields[0U]=="PROFILE")
        {
            if (Fields.size()!=4U) return ParseError::MalformedLine;
            const LocalProfile Profile{Fields[1U],Fields[2U],Fields[3U],true};
            const RecordError Result=InOutBook.AddProfile(Profile);
            if (Result==RecordError::DuplicateProfileId) return ParseError::DuplicateIdentifier;
            if (Result==RecordError::ProfileCapacityExceeded) return ParseError::CapacityExceeded;
            return Result==RecordError::None ? ParseError::None : ParseError::InvalidProfile;
        }
        if (Fields[0U]!="ATTEMPT" || Fields.size()!=15U) return ParseError::MalformedLine;

        std::uint64_t FinishBits=0U;
        std::uint64_t RejectionCode=0U;
        std::uint64_t CollisionCount=0U;
        bool Valid=false;
        bool OfficialEligible=false;
        if (!ParseHex64(Fields[10U],FinishBits) || !ParseBoolean(Fields[11U],Valid) ||
            !ParseUnsigned(Fields[12U],RejectionCode) || !ParseUnsigned(Fields[13U],CollisionCount) ||
            !ParseBoolean(Fields[14U],OfficialEligible) || CollisionCount>std::numeric_limits<std::uint32_t>::max() ||
            RejectionCode>static_cast<std::uint64_t>(CourseRejection::WrongSide)) return ParseError::InvalidAttempt;
        const CourseRejection Reason=static_cast<CourseRejection>(RejectionCode);
        if (!IsKnownRejection(Reason)) return ParseError::InvalidAttempt;
        AttemptRecord Attempt{};
        Attempt.AttemptId=Fields[1U];
        Attempt.ProfileId=Fields[2U];
        Attempt.CourseVersion=Fields[3U];
        Attempt.RuleVersion=Fields[4U];
        Attempt.Conditions={Fields[5U],Fields[6U],Fields[7U],Fields[8U],Fields[9U]};
        Attempt.FinishTimeS=DoubleFromBits(FinishBits);
        Attempt.Valid=Valid;
        Attempt.InvalidReason=Reason;
        Attempt.CollisionCount=static_cast<std::uint32_t>(CollisionCount);
        Attempt.OfficialRecordEligible=OfficialEligible;
        Attempt.LocalUnverified=true;
        const RecordError Result=InOutBook.AddAttempt(Attempt);
        if (Result==RecordError::DuplicateAttemptId) return ParseError::DuplicateIdentifier;
        if (Result==RecordError::ProfileNotFound) return ParseError::InvalidAttempt;
        if (Result==RecordError::AttemptCapacityExceeded) return ParseError::CapacityExceeded;
        return Result==RecordError::None ? ParseError::None : ParseError::InvalidAttempt;
    }
};
}
