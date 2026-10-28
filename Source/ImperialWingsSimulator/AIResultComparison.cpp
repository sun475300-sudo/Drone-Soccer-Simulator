#include "AIResultComparison.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/Archive.h"
#include "Containers/StringConv.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
bool ValidUtf8(const TArray<uint8>& Bytes,int32& Start)
{
    Start=Bytes.Num()>=3&&Bytes[0]==0xef&&Bytes[1]==0xbb&&Bytes[2]==0xbf?3:0;
    for(int32 I=Start;I<Bytes.Num();)
    {
        const uint8 A=Bytes[I++];if(A<0x80){if(A==0)return false;continue;}
        int32 N=0;uint32 Code=0,Min=0;
        if(A>=0xc2&&A<=0xdf){N=1;Code=A&31;Min=0x80;}
        else if(A>=0xe0&&A<=0xef){N=2;Code=A&15;Min=0x800;}
        else if(A>=0xf0&&A<=0xf4){N=3;Code=A&7;Min=0x10000;}
        else return false;
        if(I+N>Bytes.Num())return false;
        for(int32 J=0;J<N;++J){const uint8 B=Bytes[I++];if((B&0xc0)!=0x80)return false;Code=(Code<<6)|(B&63);}
        if(Code<Min||Code>0x10ffff||(Code>=0xd800&&Code<=0xdfff))return false;
    }
    return true;
}
bool ValidUtf16(const FString& Value)
{
    for(int32 I=0;I<Value.Len();++I)
    {
        const uint32 C=static_cast<uint32>(Value[I]);
        if(C>=0xd800&&C<=0xdbff){if(++I>=Value.Len())return false;const uint32 D=static_cast<uint32>(Value[I]);if(D<0xdc00||D>0xdfff)return false;}
        else if(C>=0xdc00&&C<=0xdfff)return false;
    }
    return true;
}
bool UniqueJson(const FString& Json,FString& Error)
{
    struct FScope { bool bObject=false;TSet<FString> Keys; };
    TArray<FScope> Scopes;int32 Roots=0;EJsonNotation Token;
    const auto Reader=TJsonReaderFactory<>::Create(Json);
    while(Reader->ReadNext(Token))
    {
        if(Token==EJsonNotation::Error){Error=TEXT("Invalid JSON syntax");return false;}
        if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd)
        {
            if(Scopes.IsEmpty()){Error=TEXT("Invalid JSON nesting");return false;}Scopes.Pop();continue;
        }
        if(Scopes.IsEmpty())++Roots;
        else if(Scopes.Last().bObject)
        {
            const FString Key=Reader->GetIdentifier();
            if(!ValidUtf16(Key)||Scopes.Last().Keys.Contains(Key)){Error=TEXT("Duplicate or invalid JSON key");return false;}
            Scopes.Last().Keys.Add(Key);
        }
        if(Token==EJsonNotation::String&&!ValidUtf16(FString(Reader->GetValueAsString()))){Error=TEXT("Invalid Unicode in JSON string");return false;}
        if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart)
        {
            if(Scopes.Num()>=64){Error=TEXT("JSON nesting exceeds the safety limit");return false;}
            FScope Scope;Scope.bObject=Token==EJsonNotation::ObjectStart;Scopes.Add(MoveTemp(Scope));
        }
    }
    if(!Reader->GetErrorMessage().IsEmpty()||Roots!=1||!Scopes.IsEmpty()){Error=TEXT("Invalid JSON syntax");return false;}
    return true;
}
bool Number(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,double Low,double High,double& Out,bool bInteger=false)
{
    const auto F=O->TryGetField(Key);
    return F.IsValid()&&F->Type==EJson::Number&&O->TryGetNumberField(Key,Out)&&FMath::IsFinite(Out)&&Out>=Low&&Out<=High&&(!bInteger||Out==FMath::FloorToDouble(Out));
}
bool Text(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,FString& Out,int32 Limit=128)
{
    const auto F=O->TryGetField(Key);
    return F.IsValid()&&F->Type==EJson::String&&O->TryGetStringField(Key,Out)&&!Out.TrimStartAndEnd().IsEmpty()&&Out.Len()<=Limit&&ValidUtf16(Out);
}
bool Timestamp(const FString& Value,FDateTime& Out)
{
    if(!Value.Contains(TEXT("T")))return false;
    FString Normalized=Value;
    if(!Normalized.EndsWith(TEXT("Z"),ESearchCase::CaseSensitive))
    {
        bool bUtc=false;
        for(const TCHAR* Suffix:{TEXT("+00:00"),TEXT("-00:00"),TEXT("+0000"),TEXT("-0000"),TEXT("+00"),TEXT("-00")})
        {
            if(Normalized.EndsWith(Suffix,ESearchCase::CaseSensitive))
            {Normalized.LeftChopInline(FCString::Strlen(Suffix));Normalized+=TEXT("Z");bUtc=true;break;}
        }
        if(!bUtc)return false;
    }
    return FDateTime::ParseIso8601(*Normalized,Out);
}
bool Hex(const FString& Value,int32 Length)
{
    if(Value.Len()!=Length)return false;for(TCHAR C:Value)if(!FChar::IsHexDigit(C))return false;return true;
}
bool Parse(const FString& Json,FImperialAIComparisonRecord& Out)
{
    if(!UniqueJson(Json,Out.Error))return false;
    TSharedPtr<FJsonObject> O;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),O)||!O.IsValid()){Out.Error=TEXT("Result must be a JSON object");return false;}
    auto& R=Out.Result;double Schema=0,Count=0,Difficulty=0,Seed=0,Goals=0,Opponent=0,Recovery=0;
    FDateTime Started,Ended;
    Out.Error=TEXT("Invalid or missing result fields");
    if(!Number(O,TEXT("schema"),1,2,Schema,true))return false;Out.Schema=static_cast<int32>(Schema);
    if(!Number(O,Schema==1?TEXT("defenders"):TEXT("opponents"),3,4,Count,true)||
       !Number(O,TEXT("difficulty"),0,2,Difficulty,true)||!Number(O,TEXT("seed"),1,4294967295.,Seed,true)||
       !Number(O,TEXT("goals"),0,100000,Goals,true)||!Number(O,TEXT("practice_seconds"),0,604800,R.PracticeSeconds)||
       !Number(O,TEXT("landing_seconds"),0,604800,R.LandingSeconds)||!Text(O,TEXT("session_id"),R.SessionId)||!Hex(R.SessionId,32)||
       !Text(O,TEXT("source_sha256"),R.SourceHash)||!Hex(R.SourceHash,64)||!Text(O,TEXT("build_version"),R.BuildVersion,64)||
       !Text(O,TEXT("started_utc"),R.StartedUtc)||!Text(O,TEXT("ended_utc"),R.EndedUtc)||
       !Timestamp(R.StartedUtc,Started)||!Timestamp(R.EndedUtc,Ended)||Ended<Started||
       !Text(O,TEXT("stop_reason"),R.StopReason)||!Text(O,TEXT("completion_reason"),R.CompletionReason))return false;
    const auto Test=O->TryGetField(TEXT("test_session"));if(!Test.IsValid()||Test->Type!=EJson::Boolean||!O->TryGetBoolField(TEXT("test_session"),R.bTestSession))return false;
    if(R.StopReason!=TEXT("manual_stop")&&R.StopReason!=TEXT("time_limit")&&R.StopReason!=TEXT("reset")&&R.StopReason!=TEXT("application_exit")&&R.StopReason!=TEXT("mode_changed")&&R.StopReason!=TEXT("defender_grounded")&&R.StopReason!=TEXT("defender_unavailable")&&R.StopReason!=TEXT("recovery_failed"))return false;
    if(R.CompletionReason!=TEXT("landed")&&R.CompletionReason!=TEXT("reset")&&R.CompletionReason!=TEXT("application_exit")&&R.CompletionReason!=TEXT("mode_changed"))return false;
    R.Settings={static_cast<int>(Count),static_cast<int>(Difficulty),static_cast<uint32>(Seed)};R.Goals=static_cast<int32>(Goals);
    if(Schema==2)
    {
        FString Mode;if(!Text(O,TEXT("session_mode"),Mode)||(Mode!=TEXT("defence")&&Mode!=TEXT("duel"))||!Number(O,TEXT("opponent_goals"),0,100000,Opponent,true))return false;
        R.Settings.Duel=Mode==TEXT("duel");R.OpponentGoals=static_cast<int32>(Opponent);
    }
    Out.bHasRecovery=O->HasField(TEXT("recovery_attempts"));
    if(Out.bHasRecovery){if(!Number(O,TEXT("recovery_attempts"),0,100000,Recovery,true))return false;R.RecoveryAttempts=static_cast<int32>(Recovery);}
    const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;if(!O->TryGetArrayField(TEXT("aircraft"),Rows)||Rows->Num()!=static_cast<int32>(Count)+1)return false;
    TSet<int32> Seen;
    for(const auto& Row:*Rows)
    {
        if(!Row.IsValid()||Row->Type!=EJson::Object)return false;
        const auto A=Row->AsObject();double Id=0,Impacts=0,Remaining=0;
        if(!A.IsValid()||!Number(A,TEXT("id"),0,9,Id,true)||!Number(A,TEXT("guard_impacts"),0,1000000,Impacts,true)||!Number(A,TEXT("guard_remaining_percent"),0,100,Remaining))return false;
        const int32 I=static_cast<int32>(Id);
        if(Seen.Contains(I)||(I!=0&&(I<R.Settings.FirstId()||I>=R.Settings.FirstId()+Count)))return false;
        Seen.Add(I);R.Aircraft.Add({I,static_cast<int32>(Impacts),Remaining});
    }
    if(!Seen.Contains(0))return false;
    R.SessionId=R.SessionId.ToLower();R.SourceHash=R.SourceHash.ToLower();R.bSnapshot=R.bSaved=true;
    Out.Error.Empty();return true;
}
}

bool ImperialAIComparison::Load(const FString& Path,FImperialAIComparisonRecord& Out)
{
    Out={};Out.FileName=FPaths::GetCleanFilename(Path);
#if PLATFORM_WINDOWS
    const DWORD Attributes=::GetFileAttributesW(*FPaths::ConvertRelativePathToFull(Path));
    if(Attributes!=INVALID_FILE_ATTRIBUTES&&(Attributes&FILE_ATTRIBUTE_REPARSE_POINT)){Out.Error=TEXT("Linked report files are not supported");return false;}
#endif
    TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*Path,FILEREAD_Silent));
    if(!Reader){Out.Error=TEXT("Could not read report");return false;}
    const int64 Size=Reader->TotalSize();
    if(Size<1||Size>MaxReportBytes){Out.Error=TEXT("Report is empty or exceeds 65536 bytes");return false;}
    TArray<uint8> Bytes;Bytes.SetNumUninitialized(static_cast<int32>(Size));
    Reader->Serialize(Bytes.GetData(),Size);
    if(Reader->IsError()||Reader->TotalSize()!=Size||!Reader->Close()){Out.Error=TEXT("Report changed or could not be read");return false;}
    int32 Start=0;if(!ValidUtf8(Bytes,Start)){Out.Error=TEXT("Report must be valid UTF-8 JSON");return false;}
    const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()+Start),Bytes.Num()-Start);
    const FString Json(Converted.Length(),Converted.Get());
    if(!Parse(Json,Out)){Out.Result={};return false;}
    Out.Result.FilePath=Path;Out.bValid=true;return true;
}
FImperialAIComparisonOutcome ImperialAIComparison::Compare(const FImperialAIComparisonRecord& Reference,const FImperialAIComparisonRecord& Candidate)
{
    FImperialAIComparisonOutcome Out;Out.bHasPair=true;
    if(!Reference.bValid||!Candidate.bValid){Out.Reasons.Add(TEXT("Both selections must be readable valid AI results"));return Out;}
    const auto& A=Reference.Result;const auto& B=Candidate.Result;
    const auto Condition=[&](const TCHAR* Name,bool Same){if(!Same)Out.Reasons.Add(FString(TEXT("Recorded condition differs: "))+Name);};
    Condition(TEXT("schema"),Reference.Schema==Candidate.Schema);Condition(TEXT("source_sha256"),A.SourceHash==B.SourceHash);
    Condition(TEXT("build_version"),A.BuildVersion==B.BuildVersion);Condition(TEXT("session_mode"),A.Settings.Duel==B.Settings.Duel);
    Condition(TEXT("opponents"),A.Settings.Count==B.Settings.Count);Condition(TEXT("difficulty"),A.Settings.Difficulty==B.Settings.Difficulty);
    Condition(TEXT("seed"),A.Settings.Seed==B.Settings.Seed);Condition(TEXT("test_session"),A.bTestSession==B.bTestSession);
    Out.bRecordedConditionsMatch=Out.Reasons.IsEmpty();
    if(A.SessionId==B.SessionId)Out.Reasons.Add(TEXT("Both files identify the same session (including a save retry or copy)"));
    const auto Qualified=[&](const FImperialAIResult& R,const TCHAR* Label)
    {
        const auto Reason=[&](const TCHAR* S){Out.Reasons.Add(FString(Label)+TEXT(": ")+S);};
        if(R.bTestSession)Reason(TEXT("automated test session"));
        if(R.StopReason!=TEXT("time_limit"))Reason(TEXT("session did not reach the time limit"));
        if(R.CompletionReason!=TEXT("landed"))Reason(TEXT("AI landing did not complete"));
        if(R.PracticeSeconds<179||R.PracticeSeconds>181)Reason(TEXT("duration is outside the three-minute window (179..181 s)"));
    };
    Qualified(A,TEXT("reference"));Qualified(B,TEXT("candidate"));
    Out.bEligible=Out.Reasons.IsEmpty();
    if(Out.bEligible)
    {
        const auto Delta=[&](const TCHAR* Key,double X,double Y){Out.Deltas.Add({Key,Y-X});};
        Delta(TEXT("goals"),A.Goals,B.Goals);Delta(TEXT("practice_seconds"),A.PracticeSeconds,B.PracticeSeconds);Delta(TEXT("landing_seconds"),A.LandingSeconds,B.LandingSeconds);
        if(Reference.Schema==2&&Candidate.Schema==2)Delta(TEXT("opponent_goals"),A.OpponentGoals,B.OpponentGoals);
        if(Reference.bHasRecovery&&Candidate.bHasRecovery)Delta(TEXT("recovery_attempts"),A.RecoveryAttempts,B.RecoveryAttempts);
        const FImperialAIAircraftResult* AP=A.Aircraft.FindByPredicate([](const auto& R){return R.Id==0;});
        const FImperialAIAircraftResult* BP=B.Aircraft.FindByPredicate([](const auto& R){return R.Id==0;});
        if(AP&&BP){Delta(TEXT("player_guard_impacts"),AP->GuardImpacts,BP->GuardImpacts);Delta(TEXT("player_guard_remaining_percent"),AP->GuardRemaining,BP->GuardRemaining);}
    }
    return Out;
}
