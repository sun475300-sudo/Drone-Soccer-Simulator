#include "ImperialMatchManager.h"
#include "ImperialBuildInfo.h"
#include "ImperialUserData.h"
#include "SoccerDronePawn.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
bool Int(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,double Low,double High,double& Value)
{
    const auto F=O->TryGetField(Key);return F.IsValid()&&F->Type==EJson::Number&&O->TryGetNumberField(Key,Value)&&FMath::IsFinite(Value)&&Value>=Low&&Value<=High&&Value==FMath::FloorToDouble(Value);
}
bool Number(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,double Low,double High,double& Value)
{
    const auto F=O->TryGetField(Key);return F.IsValid()&&F->Type==EJson::Number&&O->TryGetNumberField(Key,Value)&&FMath::IsFinite(Value)&&Value>=Low&&Value<=High;
}
bool ParseResult(const FString& Json,FImperialAIResult& R)
{
    TSharedPtr<FJsonObject> O;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),O)||!O.IsValid())return false;
    for(const TCHAR* Key:{TEXT("session_id"),TEXT("started_utc"),TEXT("ended_utc"),TEXT("stop_reason"),TEXT("completion_reason"),TEXT("source_sha256"),TEXT("build_version")})
    {const auto F=O->TryGetField(Key);if(!F.IsValid()||F->Type!=EJson::String)return false;}
    const auto Test=O->TryGetField(TEXT("test_session"));if(!Test.IsValid()||Test->Type!=EJson::Boolean)return false;
    double Version=0,Count=0,Level=0,Seed=0,Goals=0;FGuid Id;FDateTime Started,Ended;
    if(!Int(O,TEXT("schema"),1,2,Version)||!Int(O,Version==1?TEXT("defenders"):TEXT("opponents"),3,4,Count)||!Int(O,TEXT("difficulty"),0,2,Level)||
       !Int(O,TEXT("seed"),1,4294967295.,Seed)||!Int(O,TEXT("goals"),0,100000,Goals)||
       !Number(O,TEXT("practice_seconds"),0,604800,R.PracticeSeconds)||!Number(O,TEXT("landing_seconds"),0,604800,R.LandingSeconds)||
       !O->TryGetStringField(TEXT("session_id"),R.SessionId)||!FGuid::ParseExact(R.SessionId,EGuidFormats::Digits,Id)||
       !O->TryGetStringField(TEXT("started_utc"),R.StartedUtc)||!FDateTime::ParseIso8601(*R.StartedUtc,Started)||
       !O->TryGetStringField(TEXT("ended_utc"),R.EndedUtc)||!FDateTime::ParseIso8601(*R.EndedUtc,Ended)||
       !O->TryGetStringField(TEXT("stop_reason"),R.StopReason)||!O->TryGetStringField(TEXT("completion_reason"),R.CompletionReason)||
       !O->TryGetStringField(TEXT("source_sha256"),R.SourceHash)||R.SourceHash.Len()!=64||
       !O->TryGetStringField(TEXT("build_version"),R.BuildVersion)||R.BuildVersion.IsEmpty()||R.BuildVersion.Len()>64||
       !O->TryGetBoolField(TEXT("test_session"),R.bTestSession))return false;
    for(TCHAR C:R.SourceHash)if(!FChar::IsHexDigit(C))return false;
    if(R.StopReason!=TEXT("manual_stop")&&R.StopReason!=TEXT("time_limit")&&R.StopReason!=TEXT("reset")&&R.StopReason!=TEXT("application_exit")&&R.StopReason!=TEXT("mode_changed")&&R.StopReason!=TEXT("defender_grounded")&&R.StopReason!=TEXT("defender_unavailable")&&R.StopReason!=TEXT("recovery_failed"))return false;
    if(R.CompletionReason!=TEXT("landed")&&R.CompletionReason!=TEXT("reset")&&R.CompletionReason!=TEXT("application_exit")&&R.CompletionReason!=TEXT("mode_changed"))return false;
    R.Settings={static_cast<int>(Count),static_cast<int>(Level),static_cast<uint32>(Seed)};R.Goals=static_cast<int32>(Goals);
    if(Version==2){FString Mode;double Red=0;if(!O->TryGetStringField(TEXT("session_mode"),Mode)||(Mode!=TEXT("defence")&&Mode!=TEXT("duel"))||!Int(O,TEXT("opponent_goals"),0,100000,Red))return false;R.Settings.Duel=Mode==TEXT("duel");R.OpponentGoals=static_cast<int32>(Red);}
    if(O->HasField(TEXT("recovery_attempts"))){double Attempts=0;if(!Int(O,TEXT("recovery_attempts"),0,100000,Attempts))return false;R.RecoveryAttempts=static_cast<int32>(Attempts);}
    const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;if(!O->TryGetArrayField(TEXT("aircraft"),Rows)||Rows->Num()!=Count+1)return false;
    TSet<int32> Seen;
    for(const auto& Row:*Rows)
    {
        if(!Row.IsValid()||Row->Type!=EJson::Object)return false;const auto A=Row->AsObject();double AircraftId=0,Impacts=0,Remaining=0;
        if(!A.IsValid()||!Int(A,TEXT("id"),0,9,AircraftId)||!Int(A,TEXT("guard_impacts"),0,1000000,Impacts)||!Number(A,TEXT("guard_remaining_percent"),0,100,Remaining))return false;
        const int32 I=static_cast<int32>(AircraftId);if((I!=0&&(I<R.Settings.FirstId()||I>=R.Settings.FirstId()+Count))||Seen.Contains(I))return false;
        Seen.Add(I);R.Aircraft.Add({I,static_cast<int32>(Impacts),Remaining});
    }
    R.bSnapshot=R.bSaved=true;return true;
}
FString SerializeResult(const FImperialAIResult& R)
{
    auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("schema"),2);O->SetStringField(TEXT("session_mode"),R.Settings.Duel?TEXT("duel"):TEXT("defence"));O->SetNumberField(TEXT("opponent_goals"),R.OpponentGoals);O->SetStringField(TEXT("session_id"),R.SessionId);
    O->SetStringField(TEXT("started_utc"),R.StartedUtc);O->SetStringField(TEXT("ended_utc"),R.EndedUtc);
    O->SetStringField(TEXT("stop_reason"),R.StopReason);O->SetStringField(TEXT("completion_reason"),R.CompletionReason);
    O->SetStringField(TEXT("source_sha256"),R.SourceHash);O->SetStringField(TEXT("build_version"),R.BuildVersion);O->SetBoolField(TEXT("test_session"),R.bTestSession);
    O->SetNumberField(TEXT("opponents"),R.Settings.Count);O->SetNumberField(TEXT("difficulty"),R.Settings.Difficulty);O->SetNumberField(TEXT("seed"),R.Settings.Seed);
    O->SetNumberField(TEXT("goals"),R.Goals);O->SetNumberField(TEXT("practice_seconds"),R.PracticeSeconds);O->SetNumberField(TEXT("landing_seconds"),R.LandingSeconds);
    O->SetNumberField(TEXT("recovery_attempts"),R.RecoveryAttempts);
    TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& A:R.Aircraft){auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("id"),A.Id);Row->SetNumberField(TEXT("guard_impacts"),A.GuardImpacts);Row->SetNumberField(TEXT("guard_remaining_percent"),A.GuardRemaining);Rows.Add(MakeShared<FJsonValueObject>(Row));}
    O->SetArrayField(TEXT("aircraft"),Rows);FString Json;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Json));return Json;
}
}
void AImperialMatchManager::SnapshotAIResult(const FString& Reason)
{
    if(!CurrentAIResult.Exists()||CurrentAIResult.bSnapshot)return;
    CurrentAIResult.bSnapshot=true;CurrentAIResult.StopReason=Reason;CurrentAIResult.PracticeSeconds=AIAge;CurrentAIResult.Goals=Rules.Score(0);CurrentAIResult.OpponentGoals=Rules.Score(1);
    if(CurrentAIResult.Settings.Duel)bAIScoringClosed=true;
    CurrentAIResult.RecoveryAttempts=GetAIRecoveryAttempts();
    for(int32 Id=0;Id<Drones.Num();++Id)if(Id==0||(Id>=AISettings.FirstId()&&Id<AISettings.FirstId()+AISettings.Count))
    {const auto* D=GetDrone(Id);CurrentAIResult.Aircraft.Add({Id,D->GetGuardImpactCount(),D->GetGuardIntegrityPercent()});}
}
void AImperialMatchManager::FinishAIResult(const FString& Reason)
{
    if(!CurrentAIResult.Exists())return;
    SnapshotAIResult(Reason);CurrentAIResult.CompletionReason=Reason;CurrentAIResult.LandingSeconds=AILandingAge;
    const auto Now=FDateTime::UtcNow();CurrentAIResult.EndedUtc=Now.ToIso8601();
    CurrentAIResult.FilePath=FPaths::Combine(ImperialUserData::Path(TEXT("TrainingResults/AI")),FString::Printf(TEXT("%019lld-"),Now.GetTicks())+CurrentAIResult.SessionId+TEXT(".json"));
    LastAIResult=CurrentAIResult;CurrentAIResult={};SaveAIResult();
}
bool AImperialMatchManager::SaveAIResult()
{
    if(!LastAIResult.Exists())return false;if(LastAIResult.bSaved)return true;
    const FString Json=SerializeResult(LastAIResult),Path=LastAIResult.FilePath,Temp=Path+TEXT(".tmp");
    bool Saved=false;FString Existing;
    if(FPaths::FileExists(Path))Saved=FFileHelper::LoadFileToString(Existing,*Path)&&Existing==Json;
    else if(IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true)&&FFileHelper::SaveStringToFile(Json,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
#if PLATFORM_WINDOWS
        // Publish a complete new report atomically. Never replace a previous session.
        Saved=::MoveFileExW(*FPaths::ConvertRelativePathToFull(Temp),*FPaths::ConvertRelativePathToFull(Path),MOVEFILE_WRITE_THROUGH)!=0;
#endif
    }
    LastAIResult.bSaved=Saved;
    Log(Saved?TEXT("AI result saved. Open AI PRACTICE > LAST RESULT to review."):TEXT("AI result could not be saved. LAST RESULT keeps it in memory: RETRY SAVE or explicitly DISCARD UNSAVED."));return Saved;
}
void AImperialMatchManager::DiscardUnsavedAIResult()
{
    if(LastAIResult.Exists()&&!LastAIResult.bSaved){LastAIResult={};Log(TEXT("Unsaved result discarded by request. Saved sessions remain unchanged."));}
}
bool AImperialMatchManager::PrepareToExit()
{
    // Leave the player's aircraft untouched while the AI follows its normal
    // landing path. Never drop an in-memory result on a failed save.
    if(IsAIPracticeActive())
    {
        if(!IsAILanding())StopAIPractice();
        Log(TEXT("AI is landing. Land the blue striker manually; exit again after AI has landed and its result is saved."));
        return false;
    }
    if(LastAIResult.Exists()&&!LastAIResult.bSaved)
    {
        Log(TEXT("Exit paused: AI result is unsaved. Land and disarm, then open AI PRACTICE > LAST RESULT to retry saving or discard it."));
        return false;
    }
    ResetMatch();return true;
}
void AImperialMatchManager::LoadAIResult()
{
    const FString Dir=ImperialUserData::Path(TEXT("TrainingResults/AI"));TArray<FString> Files;
    IFileManager::Get().FindFiles(Files,*FPaths::Combine(Dir,TEXT("*.json")),true,false);Files.Sort([](const FString& A,const FString& B){return A>B;});
    // A bad report never turns into an invented zero-score success or deletes history.
    for(int32 I=0;I<FMath::Min(Files.Num(),100);++I)
    {
        const FString Path=FPaths::Combine(Dir,Files[I]);if(IFileManager::Get().FileSize(*Path)>65536)continue;
        FString Json;FImperialAIResult Candidate;
        if(FFileHelper::LoadFileToString(Json,*Path)&&ParseResult(Json,Candidate)){Candidate.FilePath=Path;LastAIResult=Candidate;return;}
    }
}
