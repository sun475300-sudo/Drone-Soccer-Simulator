#include "ImperialRadioUXVerifier.h"
#include "ImperialLocalization.h"
#include "ImperialProfileStorage.h"
#include "ImperialUserData.h"
#include "ImperialRadioSubsystem.h"
#include "ImperialHUD.h"
#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "ImperialBuildInfo.h"
#include "SoccerDronePawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
bool ContainsHangul(const FString& Text)
{
    for(const TCHAR C:Text)
        if((C>=0xAC00&&C<=0xD7A3)||(C>=0x1100&&C<=0x11FF)||(C>=0x3130&&C<=0x318F))return true;
    return false;
}
bool ReadEquals(const FString& File,const FString& Expected)
{
    FString Actual;
    return FFileHelper::LoadFileToString(Actual,*File)&&Actual.TrimStartAndEnd()==Expected;
}
bool AllDisarmed(AImperialMatchManager& Match)
{
    if(Match.GetDrones().Num()!=10)return false;
    for(const auto& Drone:Match.GetDrones())if(!Drone||Drone->IsArmed())return false;
    return true;
}
}
AImperialRadioUXVerifier::AImperialRadioUXVerifier()
{PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AImperialRadioUXVerifier::Check(const TCHAR* Name,bool Passed)
{
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("check"),Name);Row->SetBoolField(TEXT("ok"),Passed);
    Checks.Add(MakeShared<FJsonValueObject>(Row));bOk&=Passed;
}
void AImperialRadioUXVerifier::CheckStorage()
{
    const FString Folder=ImperialUserData::Path(TEXT("Verification/ProfileStorage"));
    const FString Target=FPaths::Combine(Folder,TEXT("storage-test.json"));
    Check(TEXT("atomic storage creates a new isolated QA file"),ImperialProfileStorage::Save(Target,TEXT("original"))&&ReadEquals(Target,TEXT("original")));
    Check(TEXT("atomic storage replaces an existing isolated QA file"),ImperialProfileStorage::Save(Target,TEXT("replacement"))&&ReadEquals(Target,TEXT("replacement")));
    Check(TEXT("empty destination is rejected without touching existing contents"),!ImperialProfileStorage::Save(FString(),TEXT("invalid"))&&ReadEquals(Target,TEXT("replacement")));
    const FString UnicodePath=FPaths::Combine(Folder,TEXT("한글-저장-검사.json"));
    Check(TEXT("Unicode path and UTF-8 Korean contents round trip"),ImperialProfileStorage::Save(UnicodePath,TEXT("한국어 저장 검사"))&&ReadEquals(UnicodePath,TEXT("한국어 저장 검사")));
#if PLATFORM_WINDOWS
    // Denying delete sharing exercises the real Windows rename/replace failure.
    // This QA file is unrelated to RadioProfile.json and contains no calibration.
    HANDLE Lock=CreateFileW(*Target,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    Check(TEXT("destination lock fixture opened"),Lock!=INVALID_HANDLE_VALUE);
    bool Rejected=false;
    if(Lock!=INVALID_HANDLE_VALUE)
    {
        Rejected=!ImperialProfileStorage::Save(Target,TEXT("must never replace locked destination"));
        CloseHandle(Lock);
    }
    Check(TEXT("locked destination rejects replacement and preserves its previous contents"),Rejected&&ReadEquals(Target,TEXT("replacement")));
#endif
    const FString Blocker=FPaths::Combine(Folder,TEXT("parent-is-a-file"));
    const bool Blocked=FFileHelper::SaveStringToFile(TEXT("retain parent"),*Blocker,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    Check(TEXT("non-directory parent rejects storage and retains parent file"),Blocked&&!ImperialProfileStorage::Save(FPaths::Combine(Blocker,TEXT("child.json")),TEXT("not writable"))&&ReadEquals(Blocker,TEXT("retain parent")));
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files,*FPaths::Combine(Folder,TEXT("*")),true,false);
    bool NoTemps=true;
    for(const auto& File:Files)if(File.Contains(TEXT(".tmp-")))NoTemps=false;
    Check(TEXT("successful and failed writes leave no temporary siblings"),NoTemps&&Files.Num()==3);

    // Isolated schema fixtures: validation must never alter rejected source bytes.
    const FString ProfileFolder=ImperialUserData::Path(TEXT("Verification/RadioProfileStructure"));
    IFileManager::Get().MakeDirectory(*ProfileFolder,true);
    struct FBadRadioStore { const TCHAR* Name; const TCHAR* Contents; };
    const FBadRadioStore BadStores[]={
        {TEXT("missing_profiles"),TEXT("{\"version\":1}")},
        {TEXT("null_profiles"),TEXT("{\"version\":1,\"profiles\":null}")},
        {TEXT("string_profiles"),TEXT("{\"version\":1,\"profiles\":\"damaged\"}")},
        {TEXT("array_profiles"),TEXT("{\"version\":1,\"profiles\":[]}")},
        {TEXT("wrong_version"),TEXT("{\"version\":2,\"profiles\":{}}")},
        {TEXT("missing_version"),TEXT("{\"profiles\":{}}")},
        {TEXT("invalid_json"),TEXT("{broken")}
    };
    for (const auto& Case:BadStores)
    {
        const FString File=FPaths::Combine(ProfileFolder,FString(Case.Name)+TEXT(".json"));
        const bool Written=FFileHelper::SaveStringToFile(Case.Contents,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        TArray<uint8> Before,After;
        const bool ReadBefore=FFileHelper::LoadFileToArray(Before,*File);
        TSharedPtr<FJsonObject> Store,Profiles;
        const bool Accepted=ImperialProfileStorage::LoadRadioProfilesForUpdate(File,Store,Profiles);
        const bool ReadAfter=FFileHelper::LoadFileToArray(After,*File);
        Check(*FString::Printf(TEXT("invalid radio store %s is rejected with bytes preserved"),Case.Name),
              Written&&ReadBefore&&!Accepted&&!Store.IsValid()&&!Profiles.IsValid()&&ReadAfter&&Before==After);
    }
    const FString Fresh=FPaths::Combine(ProfileFolder,TEXT("fresh.json"));
    TSharedPtr<FJsonObject> FreshRoot,FreshProfiles;
    Check(TEXT("missing radio store creates in-memory objects without writing"),
          !IFileManager::Get().FileExists(*Fresh)&&ImperialProfileStorage::LoadRadioProfilesForUpdate(Fresh,FreshRoot,FreshProfiles)&&
          FreshRoot.IsValid()&&FreshProfiles.IsValid()&&!IFileManager::Get().FileExists(*Fresh));
    const FString ValidFile=FPaths::Combine(ProfileFolder,TEXT("valid.json"));
    const bool ValidWritten=FFileHelper::SaveStringToFile(TEXT("{\"version\":1,\"profiles\":{\"existing\":{\"confirmed\":true}},\"unknown\":\"keep\"}"),
                                                           *ValidFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    TSharedPtr<FJsonObject> ValidRoot,ValidProfiles;
    const bool ValidLoaded=ImperialProfileStorage::LoadRadioProfilesForUpdate(ValidFile,ValidRoot,ValidProfiles);
    bool Preserved=false;
    if(ValidLoaded&&ValidRoot.IsValid()&&ValidProfiles.IsValid())
    {
        ValidProfiles->SetObjectField(TEXT("new"),MakeShared<FJsonObject>());
        ValidRoot->SetObjectField(TEXT("profiles"),ValidProfiles);
        FString Json;
        const bool Serialized=FJsonSerializer::Serialize(ValidRoot.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
        const bool Saved=Serialized&&ImperialProfileStorage::Save(ValidFile,Json);
        TSharedPtr<FJsonObject> Reread,Reprofiles;
        FString Unknown;
        Preserved=Saved&&ImperialProfileStorage::LoadRadioProfilesForUpdate(ValidFile,Reread,Reprofiles)&&
            Reread->TryGetStringField(TEXT("unknown"),Unknown)&&Unknown==TEXT("keep")&&
            Reprofiles->HasField(TEXT("existing"))&&Reprofiles->HasField(TEXT("new"));
    }
    Check(TEXT("valid multi-device store preserves existing profile and unknown field"),ValidWritten&&Preserved);
#if PLATFORM_WINDOWS
    // These are synthetic files beneath the explicit isolated data directory.
    // Two independent handles exercise the same nonblocking lock used by WriteProfile.
    TArray<uint8> LockBefore,LockAfter;
    const bool LockRead=FFileHelper::LoadFileToArray(LockBefore,*ValidFile);
    bool FirstHeld=false,SecondRejected=false;
    {
        ImperialProfileStorage::FRadioProfileWriteLock First(ValidFile);
        ImperialProfileStorage::FRadioProfileWriteLock Second(ValidFile);
        FirstHeld=First.IsHeld();SecondRejected=!Second.IsHeld();
    }
    const bool LockReadAfter=FFileHelper::LoadFileToArray(LockAfter,*ValidFile);
    Check(TEXT("cooperative radio lock rejects a second handle and preserves original bytes"),
          LockRead&&FirstHeld&&SecondRejected&&LockReadAfter&&LockBefore==LockAfter);
    bool Retried=false;
    {
        ImperialProfileStorage::FRadioProfileWriteLock Retry(ValidFile);
        TSharedPtr<FJsonObject> Current,CurrentProfiles;
        if(Retry.IsHeld()&&ImperialProfileStorage::LoadRadioProfilesForUpdate(ValidFile,Current,CurrentProfiles))
        {
            CurrentProfiles->SetObjectField(TEXT("retry"),MakeShared<FJsonObject>());
            Current->SetObjectField(TEXT("profiles"),CurrentProfiles);
            FString Json;Retried=FJsonSerializer::Serialize(Current.ToSharedRef(),TJsonWriterFactory<>::Create(&Json))&&
                ImperialProfileStorage::Save(ValidFile,Json);
        }
    }
    TSharedPtr<FJsonObject> RetriedRoot,RetriedProfiles;FString RetriedUnknown;
    Check(TEXT("explicit retry reloads and preserves both existing profiles and metadata"),
          Retried&&ImperialProfileStorage::LoadRadioProfilesForUpdate(ValidFile,RetriedRoot,RetriedProfiles)&&
          RetriedRoot->TryGetStringField(TEXT("unknown"),RetriedUnknown)&&RetriedUnknown==TEXT("keep")&&
          RetriedProfiles->HasField(TEXT("existing"))&&RetriedProfiles->HasField(TEXT("new"))&&RetriedProfiles->HasField(TEXT("retry")));
    const FString LockBlocked=FPaths::Combine(Blocker,TEXT("lock.json"));
    ImperialProfileStorage::FRadioProfileWriteLock Unavailable(LockBlocked);
    Check(TEXT("unavailable cooperative lock fails closed without changing parent file"),!Unavailable.IsHeld()&&ReadEquals(Blocker,TEXT("retain parent")));
    Check(TEXT("radio profile lock rejection has Korean guidance"),
          ContainsHangul(ImperialLocalization::Translate(TEXT("Radio profile is busy or cannot be locked. Previous calibration remains unchanged; try again."))));
    const bool ExternalBusy=FParse::Param(FCommandLine::Get(),TEXT("ImperialRadioLockExternalBusy"));
    const bool ExternalAvailable=FParse::Param(FCommandLine::Get(),TEXT("ImperialRadioLockExternalAvailable"));
    if (ExternalBusy||ExternalAvailable)
    {
        // Test target is fixed underneath the explicitly isolated test data directory.
        const FString ExternalTarget=FPaths::Combine(ProfileFolder,TEXT("external.json"));
        ImperialProfileStorage::FRadioProfileWriteLock External(ExternalTarget);
        Check(TEXT("cross-process radio lock matches external holder state"),
              ExternalBusy!=ExternalAvailable&&(ExternalBusy?!External.IsHeld():External.IsHeld()));
    }
#endif
}
void AImperialRadioUXVerifier::CheckResponse(AImperialHUD& H,UImperialRadioSubsystem& R,AImperialMatchManager& M)
{
    const auto Open=[&](){H.NotifyHitBoxClick(TEXT("Radio_Response"));};
    const auto Click=[&](const TCHAR* Button){H.NotifyHitBoxClick(Button);};
    Open();
    Check(TEXT("response panel starts basic without changing saved response"),!H.IsResponseAdvanced()&&R.GetPilotResponse().RateDegrees[0]==180&&R.GetPilotResponse().Expo[0]==0);
    Click(TEXT("Response_RateUp_0"));Click(TEXT("Response_ExpoUp_0"));Click(TEXT("Response_Save"));
    Check(TEXT("hidden advanced controls cannot edit basic response"),R.GetPilotResponse().RateDegrees[0]==180&&R.GetPilotResponse().Expo[0]==0);
    Open();Click(TEXT("Response_Gentle"));
    Check(TEXT("basic preset is a draft until explicit save"),R.GetPilotResponse().RateDegrees[0]==180&&R.GetPilotResponse().Expo[0]==0);
    Click(TEXT("Response_Cancel"));
    Check(TEXT("cancelling gentle selection retains existing response"),R.GetPilotResponse().RateDegrees[0]==180&&R.GetPilotResponse().Expo[0]==0);
    Open();Click(TEXT("Response_Advanced"));
    Check(TEXT("advanced editor opens only on explicit request"),H.IsResponseAdvanced());
    Click(TEXT("Response_RateUp_0"));Click(TEXT("Response_ExpoUp_1"));Click(TEXT("Response_ArmMode"));
    Click(TEXT("Response_Advanced"));
    Check(TEXT("switching to basic does not apply a preset to live response"),!H.IsResponseAdvanced()&&R.GetPilotResponse().RateDegrees[0]==180);
    Click(TEXT("Response_Save"));
    Check(TEXT("advanced draft survives switching back to basic and saves custom values"),R.GetPilotResponse().RateDegrees[0]==195&&FMath::IsNearlyEqual(R.GetPilotResponse().Expo[1],.05)&&!R.IsArmToggle());
    Open();Click(TEXT("Response_Save"));
    Check(TEXT("opening and saving basic preserves custom response and ARM mode"),R.GetPilotResponse().RateDegrees[0]==195&&FMath::IsNearlyEqual(R.GetPilotResponse().Expo[1],.05)&&!R.IsArmToggle());
    Open();Click(TEXT("Response_Gentle"));Click(TEXT("Response_Save"));
    bool Gentle=true;
    for(int I=0;I<3;++I)Gentle&=R.GetPilotResponse().RateDegrees[I]==(I==2?90:120)&&FMath::IsNearlyEqual(R.GetPilotResponse().Expo[I],.35);
    Check(TEXT("explicit basic gentle save applies all three documented responses"),Gentle);
    Check(TEXT("response presets preserve separate ARM binding and mode"),R.GetArmButton()==INDEX_NONE&&!R.IsArmToggle());
    Open();Click(TEXT("Response_Defaults"));Click(TEXT("Response_Save"));
    Check(TEXT("basic standard response restores original response only"),R.GetPilotResponse().RateDegrees==ImperialInput::PilotResponse{}.RateDegrees&&R.GetPilotResponse().Expo==ImperialInput::PilotResponse{}.Expo&&!R.IsArmToggle());
    Check(TEXT("all response edits leave USB disabled and ten aircraft disarmed"),!R.IsEnabled()&&AllDisarmed(M));
    Open();Click(TEXT("Response_Gentle"));
}
void AImperialRadioUXVerifier::Finish()
{
    auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("ok"),bOk);Result->SetArrayField(TEXT("checks"),Checks);
    Result->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());
    Result->SetStringField(TEXT("scope"),TEXT("isolated new user data, explicit HUD language/layout button APIs and real Windows file-replacement failures; no device selection, HID injection, physical USB flight, cable-loss or other-PC verification"));
    FString Json,File;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Json));
    FParse::Value(FCommandLine::Get(),TEXT("ImperialRadioUXResult="),File);
    const bool Saved=!File.IsEmpty()&&FFileHelper::SaveStringToFile(Json,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FPlatformMisc::RequestExitWithStatus(false,bOk&&Saved?0:1);SetActorTickEnabled(false);
}
void AImperialRadioUXVerifier::Tick(float Dt)
{
    Super::Tick(Dt);Age+=Dt;
    auto* PC=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    auto* M=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));
    auto* H=PC?Cast<AImperialHUD>(PC->GetHUD()):nullptr;
    auto* R=GetGameInstance()->GetSubsystem<UImperialRadioSubsystem>();
    if(!M||!H||!R||!M->GetSelectedDrone())return;
    FString CaptureDir;FParse::Value(FCommandLine::Get(),TEXT("ImperialRadioUXCapture="),CaptureDir);
    const FString LanguagePath=ImperialUserData::Path(TEXT("Config/Language.txt"));
    if(Stage==0&&Age>.75)
    {
        FString DataDir;
        const bool Isolated=FParse::Value(FCommandLine::Get(),TEXT("ImperialDataDir="),DataDir)&&!DataDir.IsEmpty()&&!FPaths::IsRelative(DataDir)&&FPaths::IsSamePath(DataDir,ImperialUserData::Directory());
        Check(TEXT("explicit absolute isolated user data is required"),Isolated);
        if(!Isolated){Finish();return;}
        Check(TEXT("no device is selected and no USB input enabled"),!R->GetSelectedDevice()&&!R->IsEnabled());
        Check(TEXT("no real or synthetic calibration is present"),!IFileManager::Get().FileExists(*R->GetProfilePath()));
        if(FParse::Param(FCommandLine::Get(),TEXT("ImperialRadioUXRestoreOnly")))
        {
            FString Expected;FParse::Value(FCommandLine::Get(),TEXT("ImperialRadioUXExpectedLanguage="),Expected);
            Check(TEXT("fresh process restores expected language or explicit CLI override"),ImperialLocalization::IsKorean()==(Expected==TEXT("ko")));
            Check(TEXT("restoring or overriding display leaves persisted English selection unchanged"),ReadEquals(LanguagePath,TEXT("en")));
            Check(TEXT("language restoration leaves all aircraft disarmed"),AllDisarmed(*M));
            Finish();return;
        }
        Check(TEXT("new user starts in Korean without a prewritten language setting"),ImperialLocalization::IsKorean()&&!IFileManager::Get().FileExists(*LanguagePath));
        Check(TEXT("new user retains default Mode 2"),R->GetStickMode()==ImperialRadio::StickMode::Mode2);
        if(!H->IsRadioPanelOpen())H->ToggleRadioPanel();
        Check(TEXT("opening controller panel leaves all ten aircraft disarmed"),H->IsRadioPanelOpen()&&AllDisarmed(*M));
        for(const TCHAR* Label:{TEXT("ENABLE RADIO"),TEXT("RADIO / USB CONTROLLER"),TEXT("THROTTLE"),TEXT("YAW")})
            Check(*FString::Printf(TEXT("Korean controller label translates: %s"),Label),ContainsHangul(ImperialLocalization::Translate(Label)));
        Check(TEXT("first flight measured percentage is translated without losing its numeric value"),ImperialLocalization::Translate(TEXT("MEASURED 42%"))==TEXT("측정 42%"));
        Check(TEXT("first flight height and speed translate with both measured decimals preserved"),ImperialLocalization::Translate(TEXT("ALT 0.73 m / SPEED 0.21 m/s"))==TEXT("높이 0.73m / 속도 0.21m/s"));

        Check(TEXT("formatted leading count translates and preserves its value"),ImperialLocalization::Translate(TEXT("1 device(s) found. Choose your radio below, or use NEXT DEVICE."))==TEXT("장치 1개를 찾았습니다. 사용할 조종기를 아래에서 선택하세요."));
        Check(TEXT("formatted saved mode message translates and preserves Mode 2"),ContainsHangul(ImperialLocalization::Translate(TEXT("Mode 2 device calibration loaded. Keyboard active; minimum throttle and explicit ENABLE USB required.")))&&ImperialLocalization::Translate(TEXT("Mode 2 device calibration loaded. Keyboard active; minimum throttle and explicit ENABLE USB required.")).Contains(TEXT("2")));
        Check(TEXT("physical-controller ARM failure is translated"),ContainsHangul(ImperialLocalization::Translate(TEXT("USB INPUT LOST / DISARMED"))));
        Check(TEXT("unrecognized device name remains unchanged"),ImperialLocalization::Translate(TEXT("USER USB DEVICE 123"))==TEXT("USER USB DEVICE 123"));
        const FString* StartupEvent=M->GetEvents().FindByPredicate([](const FString& Event){return Event.StartsWith(TEXT("10 aircraft ready."));});
        Check(TEXT("startup notice is localized without changing the manager's stored event"),StartupEvent&&ContainsHangul(ImperialLocalization::Translate(*StartupEvent)));
        Check(TEXT("score notice preserves unequal team scores and return instruction"),ImperialLocalization::Translate(TEXT("GOAL! Blue 12 : 3 Red. Scoring team must all retreat."))==TEXT("골! 파랑 12 : 3 빨강 · 득점 팀은 모두 자기 진영으로 돌아오세요."));
        Check(TEXT("guard event preserves aircraft ID, percent and decimal energy"),ImperialLocalization::Translate(TEXT("IW-03 guard contact: 78% integrity, 2.5 J estimated dissipated work."))==TEXT("IW-03 가드 충돌 · 내구도 78% · 추정 흡수 에너지 2.5 J"));
        Check(TEXT("calibration ARM rejection uses generic controller wording"),ImperialLocalization::Translate(TEXT("ARM BLOCKED: CALIBRATE BOXER"))==TEXT("시동 불가: 조종기 스틱을 먼저 보정하세요"));
        Check(TEXT("set-ended notice preserves manual landing and no new ARM distinction"),ImperialLocalization::Translate(TEXT("Set ended. Manual landing control retained; new ARM and autonomous aids are locked."))==TEXT("세트 종료 · 직접 조종해 착륙하세요. 새 시동과 자율비행은 잠겼습니다."));
        Check(TEXT("reset failure notice retains unsaved-result warning"),ImperialLocalization::Translate(TEXT("Training reset. AI result NOT SAVED: open LAST RESULT to retry or discard.")).Contains(TEXT("저장되지")));
        Check(TEXT("formatted set number is preserved"),ImperialLocalization::Translate(TEXT("SET 3 / FIDA CLASS 40"))==TEXT("3세트 / FIDA Class 40"));
        R->DisableRadio();
        Check(TEXT("controller status is translated in Korean"),ContainsHangul(R->GetStatus()));
        H->NotifyHitBoxClick(TEXT("Radio_Mode1"));
        Check(TEXT("Korean UI preserves explicit Mode 1 selection without enabling USB"),R->GetStickMode()==ImperialRadio::StickMode::Mode1&&!R->IsEnabled()&&AllDisarmed(*M));
        CheckStorage();Stage=1;Age=0;return;
    }
    if(Stage==1&&Age>.75)
    {
        if(!CaptureDir.IsEmpty())FScreenshotRequest::RequestScreenshot(FPaths::Combine(CaptureDir,TEXT("controller-ko.png")),true,false);
        Stage=2;Age=0;return;
    }
    if(Stage==2&&Age>.75)
    {
        H->NotifyHitBoxClick(TEXT("Radio_Language"));
        Check(TEXT("language button switches to English and saves only language preference"),!ImperialLocalization::IsKorean()&&ReadEquals(LanguagePath,TEXT("en")));
        Check(TEXT("English controller label is unchanged"),ImperialLocalization::Translate(TEXT("ENABLE RADIO"))==TEXT("ENABLE RADIO"));
        Check(TEXT("English score notice stays unchanged after language switch"),ImperialLocalization::Translate(TEXT("GOAL! Blue 12 : 3 Red. Scoring team must all retreat."))==TEXT("GOAL! Blue 12 : 3 Red. Scoring team must all retreat."));
        R->DisableRadio();
        Check(TEXT("existing status renders English after changing language"),!ContainsHangul(R->GetStatus())&&R->GetStatus().Contains(TEXT("Keyboard input selected")));
#if PLATFORM_WINDOWS
        HANDLE LanguageLock=CreateFileW(*LanguagePath,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        Check(TEXT("language preference lock fixture opened"),LanguageLock!=INVALID_HANDLE_VALUE);
        bool LanguageRejected=false;
        if(LanguageLock!=INVALID_HANDLE_VALUE)
        {
            LanguageRejected=!ImperialLocalization::SetKorean(true);
            CloseHandle(LanguageLock);
        }
        Check(TEXT("failed language save retains both active English and persisted English"),LanguageRejected&&!ImperialLocalization::IsKorean()&&ReadEquals(LanguagePath,TEXT("en")));
        TArray<FString> LanguageTemps;
        IFileManager::Get().FindFiles(LanguageTemps,*FPaths::Combine(FPaths::GetPath(LanguagePath),TEXT("Language.txt.tmp*")),true,false);
        Check(TEXT("failed language save leaves no temporary preference file"),LanguageTemps.IsEmpty());
#endif
        Check(TEXT("language switch preserves chosen layout and motor/input gates"),R->GetStickMode()==ImperialRadio::StickMode::Mode1&&!R->IsEnabled()&&AllDisarmed(*M));
        Stage=3;Age=0;return;
    }
    if(Stage==3&&Age>.75)
    {
        if(!CaptureDir.IsEmpty())FScreenshotRequest::RequestScreenshot(FPaths::Combine(CaptureDir,TEXT("controller-en.png")),true,false);
        Stage=4;Age=0;return;
    }
    if(Stage==5&&Age>.75)
    {
        if(!CaptureDir.IsEmpty())FScreenshotRequest::RequestScreenshot(FPaths::Combine(CaptureDir,TEXT("response-basic-ko.png")),true,false);
        Stage=6;Age=0;return;
    }
    if(Stage==6&&Age>.75)
    {H->NotifyHitBoxClick(TEXT("Response_Advanced"));Stage=7;Age=0;return;}
    if(Stage==7&&Age>.75)
    {
        if(!CaptureDir.IsEmpty())FScreenshotRequest::RequestScreenshot(FPaths::Combine(CaptureDir,TEXT("response-advanced-ko.png")),true,false);
        Stage=8;Age=0;return;
    }
    if(Stage==8&&Age>.75)
    {
        H->NotifyHitBoxClick(TEXT("Response_Cancel"));
        H->NotifyHitBoxClick(TEXT("Radio_Language"));
        Check(TEXT("fixture leaves English persisted for independent process reload"),!ImperialLocalization::IsKorean()&&ReadEquals(LanguagePath,TEXT("en")));
        Check(TEXT("UI and storage tests never create a calibration file"),!IFileManager::Get().FileExists(*R->GetProfilePath()));
        Finish();return;
    }
    if(Stage!=4||Age<.75)return;
    H->NotifyHitBoxClick(TEXT("Radio_Language"));
    Check(TEXT("switching back to Korean persists ko"),ImperialLocalization::IsKorean()&&ReadEquals(LanguagePath,TEXT("ko")));
    H->NotifyHitBoxClick(TEXT("Radio_Mode2"));
    Check(TEXT("Korean UI preserves Mode 2 selection without changing motor/input gates"),R->GetStickMode()==ImperialRadio::StickMode::Mode2&&!R->IsEnabled()&&AllDisarmed(*M));
    CheckResponse(*H,*R,*M);Stage=5;Age=0;
}
