#include "ImperialFlightRecordingVerifier.h"
#include "ImperialMatchManager.h"
#include "ImperialBuildInfo.h"
#include "ImperialUserData.h"
#include "ImperialRadioSubsystem.h"
#include "SoccerDronePawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

AImperialFlightRecordingVerifier::AImperialFlightRecordingVerifier()
{PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AImperialFlightRecordingVerifier::Check(const TCHAR* Name,bool Passed)
{auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("check"),Name);R->SetBoolField(TEXT("ok"),Passed);Checks.Add(MakeShared<FJsonValueObject>(R));bOk&=Passed;}
void AImperialFlightRecordingVerifier::Next(int32 Value){Stage=Value;StageWall=FPlatformTime::Seconds();}
void AImperialFlightRecordingVerifier::BeginPlay()
{
    Super::BeginPlay();StartWall=StageWall=FPlatformTime::Seconds();
    FString RequestedData,Marker;
    FParse::Value(FCommandLine::Get(),TEXT("ImperialDataDir="),RequestedData);
    FParse::Value(FCommandLine::Get(),TEXT("ImperialFlightRecordingResult="),ResultFile);
    DataDir=ImperialUserData::Directory();
    const bool Paths=!RequestedData.IsEmpty()&&!ResultFile.IsEmpty()&&!FPaths::IsRelative(RequestedData)&&!FPaths::IsRelative(ResultFile)&&
        FPaths::IsSamePath(RequestedData,DataDir)&&FPaths::IsSamePath(FPaths::GetPath(DataDir),FPaths::GetPath(ResultFile))&&
        FPaths::GetCleanFilename(DataDir)==TEXT("user")&&FPaths::GetCleanFilename(ResultFile)==TEXT("result.json");
    const bool Fresh=Paths&&FFileHelper::LoadFileToString(Marker,*FPaths::Combine(DataDir,TEXT(".flight-recording-qa")))&&
        Marker==TEXT("imperial-flight-recording-qa-v1\n")&&
        !IFileManager::Get().FileExists(*FPaths::Combine(DataDir,TEXT("Config/RadioProfile.json")))&&
        !IFileManager::Get().FileExists(*FPaths::Combine(DataDir,TEXT("FlightLogs")))&&
        !IFileManager::Get().DirectoryExists(*FPaths::Combine(DataDir,TEXT("FlightLogs")));
    bGuarded=Paths&&Fresh;
    Check(TEXT("explicit fresh absolute QA user data and result sibling are required"),bGuarded);
    if(!bGuarded){Finish();return;}
    LogDir=FPaths::Combine(DataDir,TEXT("FlightLogs"));
    BlockedText=TEXT("intentional QA file blocks only this fresh FlightLogs directory\n");
    Sentinel=TEXT("keep unrelated QA data unchanged\n");
    FString Existing;Check(TEXT("unrelated QA sentinel exists before any fault injection"),
        FFileHelper::LoadFileToString(Existing,*FPaths::Combine(DataDir,TEXT("keep.txt")))&&Existing==Sentinel);
    int32 FPS=60;FParse::Value(FCommandLine::Get(),TEXT("FPS="),FPS);
    Check(TEXT("verification frame rate is an explicit supported value"),FPS==20||FPS==60||FPS==240);
    if(FPS!=20&&FPS!=60&&FPS!=240){Finish();return;}
    FApp::SetFixedDeltaTime(1./FPS);FApp::SetUseFixedTimeStep(true);
    if(GEngine)GEngine->SetMaxFPS(60);
}
bool AImperialFlightRecordingVerifier::Lock(const FString& Path)
{
    Unlock();
#if PLATFORM_WINDOWS
    HANDLE H=::CreateFileW(*Path,GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(H==INVALID_HANDLE_VALUE)return false;
    LockedFile=H;return true;
#else
    return false;
#endif
}
void AImperialFlightRecordingVerifier::Unlock()
{
#if PLATFORM_WINDOWS
    if(LockedFile)::CloseHandle(static_cast<HANDLE>(LockedFile));
#endif
    LockedFile=nullptr;
}
void AImperialFlightRecordingVerifier::EndPlay(const EEndPlayReason::Type Reason)
{Unlock();Super::EndPlay(Reason);}
TArray<FString> AImperialFlightRecordingVerifier::Files() const
{TArray<FString> Result;IFileManager::Get().FindFiles(Result,*FPaths::Combine(LogDir,TEXT("*.jsonl")),true,false);return Result;}
FString AImperialFlightRecordingVerifier::OnlyNewFile(const TArray<FString>& Before) const
{
    TArray<FString> Added;for(const auto& Name:Files())if(!Before.Contains(Name))Added.Add(Name);
    return Added.Num()==1?FPaths::Combine(LogDir,Added[0]):FString();
}
bool AImperialFlightRecordingVerifier::Read(const FString& Path,FString& Text) const
{return !Path.IsEmpty()&&FFileHelper::LoadFileToString(Text,*Path);}
bool AImperialFlightRecordingVerifier::HasEvents(const FString& Path,bool StopExpected) const
{
    FString Text;if(!Read(Path,Text))return false;
    TArray<FString> Lines;Text.ParseIntoArrayLines(Lines,false);if(!Lines.IsEmpty()&&Lines.Last().IsEmpty())Lines.Pop();if(Lines.IsEmpty())return false;
    FString First,Last;
    for(int32 I=0;I<Lines.Num();++I)
    {
        TSharedPtr<FJsonObject> R;
        if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Lines[I]),R)||!R.IsValid())return false;
        FString Event,Hash;const TArray<TSharedPtr<FJsonValue>>* Aircraft=nullptr;
        if(!R->TryGetStringField(TEXT("event"),Event)||!R->TryGetStringField(TEXT("source_sha256"),Hash)||
            Hash!=ImperialBuildInfo::SourceHash()||!R->TryGetArrayField(TEXT("aircraft"),Aircraft)||Aircraft->Num()!=10)return false;
        if(I==0)First=Event;Last=Event;
    }
    return First==TEXT("start")&&(StopExpected?Last==TEXT("stop"):Last!=TEXT("stop"));
}
void AImperialFlightRecordingVerifier::Finish()
{
    if(bFinished)return;bFinished=true;Unlock();SetActorTickEnabled(false);
    auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("ok"),bOk&&bGuarded);R->SetArrayField(TEXT("checks"),Checks);
    R->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());R->SetStringField(TEXT("data_directory"),DataDir);
    R->SetNumberField(TEXT("min_game_dt_s"),MinDt);R->SetNumberField(TEXT("max_game_dt_s"),MaxDt);
    R->SetStringField(TEXT("scope"),TEXT("Actual ToggleFlightRecording and periodic RecordFlightSample, actual file-as-directory start failure and Windows deny-share sample/stop failures, new isolated QA data, no OS input/GPU/USB or flight commands. This does not prove disk-full/power-loss atomicity."));
    R->SetStringField(TEXT("failed_sample_path"),SampleFile);R->SetStringField(TEXT("failed_stop_path"),StopFile);
    TArray<TSharedPtr<FJsonValue>> Logs;
    if(bGuarded)for(const auto& Name:Files())Logs.Add(MakeShared<FJsonValueString>(Name));
    R->SetArrayField(TEXT("log_files"),Logs);
    FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));
    const bool Saved=bGuarded&&FFileHelper::SaveStringToFile(Text,*ResultFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FPlatformMisc::RequestExitWithStatus(false,bOk&&Saved?0:1);
}
void AImperialFlightRecordingVerifier::Tick(float Dt)
{
    Super::Tick(Dt);if(bFinished||!bGuarded)return;
    MinDt=FMath::Min(MinDt,static_cast<double>(Dt));MaxDt=FMath::Max(MaxDt,static_cast<double>(Dt));
    const double Now=FPlatformTime::Seconds(),Age=Now-StageWall;
    if(Now-StartWall>12){Check(TEXT("filesystem fixture completes within bounded real time"),false);Finish();return;}
    auto* M=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));
    if(!M||M->GetDrones().Num()!=10)return;
    const auto Safe=[&](){bool Result=M->GetSelected()==0&&!M->IsAIPracticeActive()&&M->GetScore(0)==0&&M->GetScore(1)==0;
        for(const auto& D:M->GetDrones())Result&=D&&!D->IsArmed()&&!D->IsAutonomousEnabled()&&D->GetAltitudeMeters()<=.35f&&D->GetFlightSpeed()<=.25f;
        const auto* Radio=GetGameInstance()->GetSubsystem<UImperialRadioSubsystem>();return Result&&Radio&&!Radio->IsEnabled();};
    const auto FailedNotice=[&](){return M->GetNotice()==TEXT("Flight recording stopped: could not write to Saved/FlightLogs.");};
    const auto RecordingNotice=[&](){return M->GetNotice()==TEXT("Recording actual controls and physics. F6 stops recording.");};
    const auto SavedNotice=[&](){return M->GetNotice()==TEXT("Flight recording saved in Saved/FlightLogs.");};
    if(Stage==0&&Age>.5)
    {
        Check(TEXT("fixture starts with ten grounded disarmed manual aircraft and no score"),Safe());
        if(!Safe()){Finish();return;}
        Check(TEXT("actual start obstruction is a file at the fresh log-directory path"),FFileHelper::SaveStringToFile(BlockedText,*LogDir));
        M->ToggleFlightRecording();
        Check(TEXT("failed start leaves recording inactive"),!M->IsRecordingFlight());
        Check(TEXT("failed start preserves failure notice instead of announcing recording"),FailedNotice());
        M->ToggleFlightRecording();
        Check(TEXT("explicit retry while blocked fails again without success notice"),!M->IsRecordingFlight()&&FailedNotice());
        Next(1);return;
    }
    if(Stage==1&&Age>.2)
    {
        FString Before;const bool BlockerUnchanged=Read(LogDir,Before)&&Before==BlockedText;
        Check(TEXT("no automatic retry or blocker modification after failed start"),!M->IsRecordingFlight()&&BlockerUnchanged&&Files().IsEmpty());
        if(!BlockerUnchanged){Finish();return;}
        Check(TEXT("fixture removes only its exact known obstruction"),IFileManager::Get().Delete(*LogDir,false,true,true));
        Check(TEXT("fresh log directory can be created after resolving obstruction"),IFileManager::Get().MakeDirectory(*LogDir,true));
        const auto BeforeFiles=Files();M->ToggleFlightRecording();RetryFile=OnlyNewFile(BeforeFiles);
        Check(TEXT("explicit retry after start failure creates a unique real start record"),M->IsRecordingFlight()&&RecordingNotice()&&HasEvents(RetryFile,false));
        M->ToggleFlightRecording();
        Check(TEXT("normal stop after start retry appends stop and announces saved"),!M->IsRecordingFlight()&&SavedNotice()&&HasEvents(RetryFile,true));
        Next(2);return;
    }
    if(Stage==2&&Age>.1)
    {
        const auto Before=Files();M->ToggleFlightRecording();SampleFile=OnlyNewFile(Before);
        Check(TEXT("sample-failure scenario starts a new real recording"),M->IsRecordingFlight()&&Read(SampleFile,SamplePrefix));
        const bool Locked=Lock(SampleFile);Check(TEXT("actual deny-share handle prevents periodic sample append"),Locked);
        if(!Locked){Finish();return;}Next(3);return;
    }
    if(Stage==3)
    {
        if(M->IsRecordingFlight()&&Age<1)return;
        Check(TEXT("periodic sample failure stops recording"),!M->IsRecordingFlight());
        Check(TEXT("periodic sample failure notice remains visible"),FailedNotice());
        Unlock();FString After;
        Check(TEXT("sample lock failure preserves the complete previously written prefix"),Read(SampleFile,After)&&After==SamplePrefix&&HasEvents(SampleFile,false));
        Next(4);return;
    }
    if(Stage==4&&Age>.2)
    {
        Check(TEXT("recording does not silently resume after sample write failure"),!M->IsRecordingFlight()&&Files().Num()==2);
        const auto Before=Files();M->ToggleFlightRecording();RetryFile=OnlyNewFile(Before);
        Check(TEXT("explicit sample-failure retry creates another file and recording notice"),M->IsRecordingFlight()&&RecordingNotice()&&!RetryFile.IsEmpty()&&RetryFile!=SampleFile);
        Next(5);return;
    }
    if(Stage==5&&Age>.2)
    {
        M->ToggleFlightRecording();
        Check(TEXT("normal sample retry stop closes a valid start-sample-stop file"),!M->IsRecordingFlight()&&SavedNotice()&&HasEvents(RetryFile,true));
        const auto Before=Files();M->ToggleFlightRecording();StopFile=OnlyNewFile(Before);
        Check(TEXT("stop-failure scenario starts a new real recording"),M->IsRecordingFlight()&&Read(StopFile,StopPrefix));
        const bool Locked=Lock(StopFile);Check(TEXT("actual deny-share handle prevents explicit stop append"),Locked);
        if(!Locked){Finish();return;}
        M->ToggleFlightRecording();
        Check(TEXT("failed stop leaves recording inactive"),!M->IsRecordingFlight());
        Check(TEXT("failed stop preserves failure notice instead of announcing saved"),FailedNotice());
        Unlock();FString After;
        Check(TEXT("failed stop preserves prefix without adding a false stop record"),Read(StopFile,After)&&After==StopPrefix&&HasEvents(StopFile,false));
        Next(6);return;
    }
    if(Stage==6&&Age>.2)
    {
        Check(TEXT("failed stop never automatically retries or overwrites existing logs"),!M->IsRecordingFlight()&&Files().Num()==4);
        const auto Before=Files();M->ToggleFlightRecording();RetryFile=OnlyNewFile(Before);
        Check(TEXT("explicit stop-failure retry starts a separate unique file"),M->IsRecordingFlight()&&RecordingNotice()&&!RetryFile.IsEmpty()&&RetryFile!=StopFile&&RetryFile!=SampleFile);
        Next(7);return;
    }
    if(Stage==7&&Age>.2)
    {
        M->ToggleFlightRecording();
        Check(TEXT("explicit final retry can stop successfully"),!M->IsRecordingFlight()&&SavedNotice()&&HasEvents(RetryFile,true));
        FString After,Other;
        Check(TEXT("earlier failed sample prefix remains unchanged after all retries"),Read(SampleFile,After)&&After==SamplePrefix);
        Check(TEXT("earlier failed stop prefix remains unchanged after all retries"),Read(StopFile,Other)&&Other==StopPrefix);
        Check(TEXT("all five recordings remain separate with valid JSON and build identity"),Files().Num()==5);
        bool Valid=true;for(const auto& Name:Files()){const auto Path=FPaths::Combine(LogDir,Name);Valid&=HasEvents(Path,Path!=SampleFile&&Path!=StopFile);}
        Check(TEXT("real output records retain start-stop boundaries and aircraft schema"),Valid);
        FString Keep;Check(TEXT("unrelated QA sentinel and absent radio profile are preserved"),Read(FPaths::Combine(DataDir,TEXT("keep.txt")),Keep)&&Keep==Sentinel&&!IFileManager::Get().FileExists(*FPaths::Combine(DataDir,TEXT("Config/RadioProfile.json"))));
        Check(TEXT("recording faults and retries never arm, fly, select or score"),Safe());
        Finish();
    }
}
