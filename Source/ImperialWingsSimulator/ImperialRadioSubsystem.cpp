#include "ImperialRadioSubsystem.h"
#include "ImperialUserData.h"
#include "ImperialBuildInfo.h"
#include "RadioProfileJson.h"
#include "ManualThrottleResponse.h"
#include "SoccerDronePawn.h"
#include "EngineUtils.h"

#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include <mmsystem.h>
#endif

namespace
{
constexpr double FreshInputSeconds = 0.25;
constexpr double PollInterval = 1.0 / 120.0;

bool ReadJsonFile(const FString& Path, TSharedPtr<FJsonObject>& Object)
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Path)) return false;
    return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object) && Object.IsValid();
}

bool ReadFinite(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, double& Value)
{
    return Object.IsValid() && Object->TryGetNumberField(Key, Value) && FMath::IsFinite(Value);
}

TArray<FImperialRadioDevice> EnumerateWinMmDevices()
{
    TArray<FImperialRadioDevice> Result;
#if PLATFORM_WINDOWS
    // joyGetNumDevs is a driver slot count, not the number physically connected.
    const UINT Slots = FMath::Min<UINT>(16, joyGetNumDevs());
    for (UINT Id = 0; Id < Slots; ++Id)
    {
        JOYCAPSW Caps{};
        JOYINFOEX Position{};
        Position.dwSize = sizeof(Position);
        Position.dwFlags = JOY_RETURNALL;
        if (joyGetDevCapsW(Id, &Caps, sizeof(Caps)) != JOYERR_NOERROR ||
            joyGetPosEx(Id, &Position) != JOYERR_NOERROR) continue;

        FImperialRadioDevice Device;
        Device.Id = static_cast<int32>(Id);
        Device.Name = FString(Caps.szPname);
        Device.RegistryKey = FString(Caps.szRegKey);
        Device.OemName = FString(Caps.szOEMVxD);
        Device.ManufacturerId = Caps.wMid;
        Device.ProductId = Caps.wPid;
        Device.AxisCount = static_cast<int32>(Caps.wNumAxes);
        Device.ButtonCount = static_cast<int32>(Caps.wNumButtons);
        Device.AxisAvailable = {Caps.wNumAxes >= 1, Caps.wNumAxes >= 2,
            (Caps.wCaps & JOYCAPS_HASZ) != 0, (Caps.wCaps & JOYCAPS_HASR) != 0,
            (Caps.wCaps & JOYCAPS_HASU) != 0, (Caps.wCaps & JOYCAPS_HASV) != 0};
        Device.AxisMinimum = {Caps.wXmin, Caps.wYmin, Caps.wZmin, Caps.wRmin, Caps.wUmin, Caps.wVmin};
        Device.AxisMaximum = {Caps.wXmax, Caps.wYmax, Caps.wZmax, Caps.wRmax, Caps.wUmax, Caps.wVmax};
        // WinMM does not expose a serial number. Include slot and complete reported
        // capabilities to avoid silently sharing calibration between similar devices.
        // A slot change requires selection/calibration again; no brand is inferred.
        Device.Identity = FString::Printf(TEXT("winmm-v1|slot=%u|mid=%04X|pid=%04X|name=%s|reg=%s|oem=%s|axes=%u|buttons=%u|caps=%u"),
            Id, Caps.wMid, Caps.wPid, *Device.Name, *Device.RegistryKey, *Device.OemName,
            Caps.wNumAxes, Caps.wNumButtons, Caps.wCaps);
        for (int32 Axis = 0; Axis < 6; ++Axis)
        {
            if (Device.AxisMaximum[Axis] <= Device.AxisMinimum[Axis]) Device.AxisAvailable[Axis] = false;
            Device.Identity += FString::Printf(TEXT("|a%d=%u,%u"), Axis, Device.AxisMinimum[Axis], Device.AxisMaximum[Axis]);
        }
        Result.Add(MoveTemp(Device));
    }
#endif
    return Result;
}
}

void UImperialRadioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    RawAxes.Init(0.0f, 6);
    EmptyAxes.Init(0.0f, 6);
    bInitialized = true;
    bEnabled = false;
    RefreshDevices();
    LoadPilotControls();
    LoadPilotResponse();
    UE_LOG(LogTemp, Display, TEXT("IMPERIAL_RADIO_READY devices=%d mode=KEYBOARD native=WinMM"), Devices.Num());
}

void UImperialRadioSubsystem::Deinitialize()
{
    bInitialized = false;
    ClearLiveInput(TEXT("USB input stopped."));
    Devices.Empty();
    Super::Deinitialize();
}

bool UImperialRadioSubsystem::IsTickable() const
{
    return bInitialized && !HasAnyFlags(RF_ClassDefaultObject) && GetGameInstance() && GetWorld() && GetWorld()->IsGameWorld();
}

TStatId UImperialRadioSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UImperialRadioSubsystem, STATGROUP_Tickables);
}

void UImperialRadioSubsystem::Tick(float DeltaTime)
{
    if(!bStorageSmokeDone&&FParse::Param(FCommandLine::Get(),TEXT("ImperialStorageSmoke")))
    {
        bStorageSmokeDone=true;
        // Explicit disk/HID integration check. Never creates calibration values or arms a body.
        const bool ColdKeyboard=!IsEnabled()&&SelectedId==INDEX_NONE;
        const bool ExpectedSaved=FParse::Param(FCommandLine::Get(),TEXT("ImperialStorageExpectSaved"));
        const bool SavedBefore=HasSavedProfiles();
        const bool Activated=EnableSavedRadio();
        const bool GenuineRoundTrip=ExpectedSaved&&Activated&&IsCalibrated()&&WriteProfile()&&LoadProfile()&&IsCalibrated()&&!IsEnabled();
        DisableRadio();
        const bool ReportSaved=ExportDeviceReport();
        auto Result=MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("source_sha256"),ImperialBuildInfo::SourceHash());
        Result->SetStringField(TEXT("data_directory"),ImperialUserData::Directory());
        Result->SetStringField(TEXT("profile_path"),ProfilePath());
        Result->SetNumberField(TEXT("actual_device_count"),Devices.Num());
        Result->SetBoolField(TEXT("cold_start_keyboard"),ColdKeyboard);
        Result->SetBoolField(TEXT("saved_profiles_found"),SavedBefore);
        Result->SetBoolField(TEXT("saved_profile_activated"),Activated);
        Result->SetBoolField(TEXT("genuine_profile_round_trip"),GenuineRoundTrip);
        Result->SetBoolField(TEXT("diagnostic_saved"),ReportSaved);
        Result->SetBoolField(TEXT("ended_disabled"),!IsEnabled());
        Result->SetBoolField(TEXT("ok"),ColdKeyboard&&ReportSaved&&!IsEnabled()&&(ExpectedSaved?GenuineRoundTrip:!SavedBefore&&!Activated));
        FString Path=ImperialUserData::Path(TEXT("storage-smoke.json"));FParse::Value(FCommandLine::Get(),TEXT("ImperialStorageResult="),Path);
        FString Json;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        FPlatformMisc::RequestExit(false);return;
    }
    const double Now = FPlatformTime::Seconds();
    // A long paused/stalled game frame may recover with a successful poll. It still
    // invalidates the previous enable command, so reconnect never resumes throttle.
    if (bConnected && Now - LastSampleTime > FreshInputSeconds)
        ClearLiveInput(TEXT("Joystick input timed out. USB disabled; lower throttle and enable again."));
    if (Now - LastEnumerationTime >= 1.0) RefreshDevices();
    if (SelectedId != INDEX_NONE && Now - LastPollTime >= PollInterval) PollSelectedDevice();
}

const FImperialRadioDevice* UImperialRadioSubsystem::GetSelectedDevice() const
{
    return Devices.FindByPredicate([this](const FImperialRadioDevice& Device)
        { return Device.Id == SelectedId && Device.Identity == SelectedIdentity; });
}

void UImperialRadioSubsystem::RefreshDevices()
{
    LastEnumerationTime = FPlatformTime::Seconds();
    Devices = EnumerateWinMmDevices();
    if (SelectedId != INDEX_NONE)
    {
        const FImperialRadioDevice* Slot = Devices.FindByPredicate([this](const FImperialRadioDevice& Device) { return Device.Id == SelectedId; });
        if (!Slot)
        {
            ClearLiveInput(TEXT("Selected joystick disconnected. USB disabled; reconnect then enable explicitly."));
        }
        else if (Slot->Identity != SelectedIdentity)
        {
            ClearLiveInput(TEXT("Joystick identity changed. Select the device and calibrate again."));
            SelectedId = INDEX_NONE;
            SelectedIdentity.Empty();
            Calibration = ImperialRadio::Profile{};
        }
    }
    else
    {
        Status = Devices.IsEmpty() ? TEXT("No connected USB joystick. Keyboard input remains active.")
            : TEXT("USB joystick found. Select the actual device; keyboard input remains active.");
    }
}

bool UImperialRadioSubsystem::SelectDevice(int32 Id)
{
    RefreshDevices();
    const FImperialRadioDevice* Device = Devices.FindByPredicate([Id](const FImperialRadioDevice& Candidate) { return Candidate.Id == Id; });
    if (!Device)
    {
        ClearLiveInput(TEXT("The requested joystick is not connected. Keyboard input remains active."));
        return false;
    }
    const FString Identity = Device->Identity;
    ClearLiveInput(TEXT("Selecting joystick. Keyboard input remains active."));
    SelectedId = Id;
    SelectedIdentity = Identity;
    Calibration = ImperialRadio::Profile{};
    RangeCapture = ImperialRadio::Capture{};
    PollSelectedDevice();
    if (!IsConnected()) return false;
    LoadProfile(); // Strictly loads calibration; never enables radio on its own.
    return true;
}

void UImperialRadioSubsystem::CycleDevice()
{
    RefreshDevices();
    if (Devices.IsEmpty()) return;
    const int32 Current = Devices.IndexOfByPredicate([this](const FImperialRadioDevice& Device) { return Device.Id == SelectedId; });
    SelectDevice(Devices[(Current + 1) % Devices.Num()].Id);
}

void UImperialRadioSubsystem::ClearLiveInput(const FString& Reason)
{
    if (Guided.Active()) Guided.Observe(CurrentSample(), AvailableAxes(), false, FPlatformTime::Seconds());
    bConnected = false;
    bEnabled = false;
    Buttons = 0;
    LastSampleTime = 0.0;
    RawAxes.Init(0.0f, 6);
    if (bCalibrationActive)
    {
        RangeCapture.Interrupted = true;
        RangeCapture.Active = false;
        Calibration.Confirmed = false;
        bCalibrationActive = false;
    }
    Status = Reason;
}

void UImperialRadioSubsystem::PollSelectedDevice()
{
    LastPollTime = FPlatformTime::Seconds();
    const FImperialRadioDevice* Device = GetSelectedDevice();
    if (!Device)
    {
        ClearLiveInput(TEXT("Selected joystick unavailable. USB disabled."));
        return;
    }
#if PLATFORM_WINDOWS
    JOYINFOEX Position{};
    Position.dwSize = sizeof(Position);
    Position.dwFlags = JOY_RETURNALL;
    const MMRESULT Result = joyGetPosEx(static_cast<UINT>(SelectedId), &Position);
    if (Result != JOYERR_NOERROR)
    {
        ClearLiveInput(FString::Printf(TEXT("Joystick read failed (%u). USB disabled; reconnect and enable explicitly."), Result));
        return;
    }
    const DWORD Values[6] = {Position.dwXpos, Position.dwYpos, Position.dwZpos, Position.dwRpos, Position.dwUpos, Position.dwVpos};
    for (int32 Axis = 0; Axis < 6; ++Axis)
    {
        // Driver coordinates, normalized using JOYCAPS; these are not raw HID packets.
        RawAxes[Axis] = Device->AxisAvailable[Axis]
            ? static_cast<float>(ImperialRadio::NormalizeDriverAxis(Values[Axis], Device->AxisMinimum[Axis], Device->AxisMaximum[Axis])) : 0.0f;
    }
    const bool bWasConnected = bConnected;
    Buttons = static_cast<uint32>(Position.dwButtons);
    LastSampleTime = FPlatformTime::Seconds();
    bConnected = true;
    if (!bWasConnected) Status = Guided.Phase()==ImperialRadio::GuidedPhase::Interrupted
        ? TEXT("Stick setup interrupted by input loss. Previous saved profile retained; restart STICK SETUP.")
        : TEXT("Joystick live. USB disabled; calibrate or lower throttle and enable explicitly.");
    if (bCalibrationActive && RangeCapture.Active)
    {
        const auto Error = RangeCapture.Observe(CurrentSample(), AvailableAxes(), true);
        if (Error != ImperialRadio::Error::None)
        {
            bCalibrationActive = false;
            bEnabled = false;
            SetCalibrationStatus(Error, FString());
        }
    }
    if (Guided.Active())
    {
        const auto Error = Guided.Observe(CurrentSample(), AvailableAxes(), true, LastSampleTime);
        if (Error != ImperialRadio::Error::None)
        {
            bEnabled = false;
            SetCalibrationStatus(Error, FString());
        }
    }
#else
    ClearLiveInput(TEXT("Native USB joystick input is available in the Windows build."));
#endif
}

bool UImperialRadioSubsystem::IsConnected() const
{
    return bConnected && GetSelectedDevice() && LastSampleTime > 0.0 &&
           FPlatformTime::Seconds() - LastSampleTime <= FreshInputSeconds;
}

bool UImperialRadioSubsystem::IsCalibrated() const
{
    return !Guided.Active() && Calibration.Mode == SelectedMode && IsConnected() &&
        ImperialRadio::ValidateProfile(Calibration, AvailableAxes()) == ImperialRadio::Error::None;
}

bool UImperialRadioSubsystem::IsEnabled() const
{
    return bEnabled && IsCalibrated();
}

float UImperialRadioSubsystem::ApplyManualThrottle(float Input) const
{
    return static_cast<float>(ImperialInput::ManualThrottleResponse(Input, bPrecisionThrottle));
}

void UImperialRadioSubsystem::LoadPilotControls()
{
    bPrecisionThrottle = false;
    const FString Path = ImperialUserData::Path(TEXT("Config/PilotControls.json"));
    if (!IFileManager::Get().FileExists(*Path)) return;
    TSharedPtr<FJsonObject> Root;
    double Version = 0;
    bool Precision = false;
    bool Valid = ReadJsonFile(Path, Root) && ReadFinite(Root, TEXT("version"), Version) && Version == 1.0;
    if (Valid)
    {
        const TSharedPtr<FJsonValue>* VersionField = Root->Values.Find(TEXT("version"));
        const TSharedPtr<FJsonValue>* Field = Root->Values.Find(TEXT("precision_throttle"));
        Valid = VersionField && VersionField->IsValid() && (*VersionField)->Type == EJson::Number &&
            Field && Field->IsValid() && (*Field)->Type == EJson::Boolean && (*Field)->TryGetBool(Precision);
    }
    if (!Valid)
    {
        Status = TEXT("Pilot controls file invalid or unsupported. THROTTLE: DIRECT remains active.");
        UE_LOG(LogTemp, Warning, TEXT("IMPERIAL_PILOT_CONTROLS_INVALID precision=0"));
        return;
    }
    bPrecisionThrottle = Precision;
    UE_LOG(LogTemp, Display, TEXT("IMPERIAL_PILOT_CONTROLS_LOADED precision=%d keyboard=1"), bPrecisionThrottle ? 1 : 0);
}

bool UImperialRadioSubsystem::SetPrecisionThrottleEnabled(bool bEnable)
{
    if (IsCalibrating())
    {
        Status = TEXT("Finish or cancel calibration before changing throttle response.");
        return false;
    }
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<ASoccerDronePawn> It(World); It; ++It)
        {
            if (It->IsArmed() && !It->IsAutonomousEnabled())
            {
                Status = TEXT("DISARM every manually controlled aircraft before changing throttle response.");
                return false;
            }
        }
    }
    auto Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("version"), 1);
    Root->SetBoolField(TEXT("precision_throttle"), bEnable);
    FString Json;
    const FString Path = ImperialUserData::Path(TEXT("Config/PilotControls.json"));
    const FString TempPath = Path + TEXT(".tmp");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    const bool Saved = FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json)) &&
        FFileHelper::SaveStringToFile(Json, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) &&
        IFileManager::Get().Move(*Path, *TempPath, true, true, false, true);
    if (!Saved)
    {
        Status = TEXT("Could not save pilot controls. Previous throttle response remains unchanged.");
        return false;
    }
    // Commit only after the atomic file replacement succeeds. Never rewrite the
    // device calibration file or reinterpret its raw input/low-throttle gate.
    bPrecisionThrottle = bEnable;
    bEnabled = false;
    Status = bEnable
        ? TEXT("Precision throttle saved: softer near 55% input; height stays manual. USB disabled; reconnect explicitly.")
        : TEXT("Direct throttle saved. USB disabled; reconnect explicitly at low throttle.");
    UE_LOG(LogTemp, Display, TEXT("IMPERIAL_PILOT_CONTROLS_SAVED precision=%d keyboard=1"), bEnable ? 1 : 0);
    return true;
}

void UImperialRadioSubsystem::LoadPilotResponse()
{
    Response={};ArmingButton=INDEX_NONE;ArmingDevice.Empty();bArmingToggle=true;
    const FString Path=ImperialUserData::Path(TEXT("Config/PilotResponse.json"));
    if(!IFileManager::Get().FileExists(*Path))return;
    TSharedPtr<FJsonObject> Root;ImperialInput::PilotResponse Candidate;
    double Version=0,Button=-1;FString Device;bool Toggle=true;
    bool Valid=ReadJsonFile(Path,Root);
    const auto Number=[&](const TCHAR* Key,double& Value)
    {
        if(!Root)return false;const auto* Field=Root->Values.Find(Key);
        return Field&&Field->IsValid()&&(*Field)->Type==EJson::Number&&(*Field)->TryGetNumber(Value)&&FMath::IsFinite(Value);
    };
    Valid=Valid&&Number(TEXT("version"),Version)&&Version==1&&Number(TEXT("arm_button"),Button)&&
        Button>=-1&&Button<=31&&Button==FMath::FloorToDouble(Button)&&
        Root->HasTypedField<EJson::String>(TEXT("arm_device"))&&Root->HasTypedField<EJson::Boolean>(TEXT("arm_toggle"))&&
        Root->TryGetStringField(TEXT("arm_device"),Device)&&Root->TryGetBoolField(TEXT("arm_toggle"),Toggle);
    for(int I=0;I<3&&Valid;++I)
        Valid=Number(*FString::Printf(TEXT("expo_%d"),I),Candidate.Expo[I])&&
              Number(*FString::Printf(TEXT("rate_%d"),I),Candidate.RateDegrees[I]);
    Valid=Valid&&Candidate.Valid()&&((Button==-1&&Device.IsEmpty())||(Button>=0&&!Device.IsEmpty()&&Device.Len()<2048));
    if(!Valid){Status=TEXT("Response settings invalid. Default rates/Expo and unassigned ARM button retained.");return;}
    Response=Candidate;ArmingButton=static_cast<int32>(Button);ArmingDevice=Device;bArmingToggle=Toggle;
}

bool UImperialRadioSubsystem::SavePilotResponse(const ImperialInput::PilotResponse& Candidate,int32 ArmButton,const FString& ArmDevice,bool bToggle)
{
    if(!Candidate.Valid()||ArmButton < -1||ArmButton>31||
        (ArmButton==-1&&!ArmDevice.IsEmpty())||(ArmButton>=0&&(ArmDevice.IsEmpty()||ArmDevice.Len()>=2048)))
    {Status=TEXT("Invalid rate, Expo or ARM binding. Previous settings retained.");return false;}
    if(IsCalibrating()){Status=TEXT("Finish or cancel stick setup before editing response.");return false;}
    if(UWorld* World=GetWorld())for(TActorIterator<ASoccerDronePawn> It(World);It;++It)
        if(It->IsArmed()&&!It->IsAutonomousEnabled())
        {Status=TEXT("DISARM every manual aircraft before saving response or ARM binding.");return false;}
    if(ArmButton>=0&&(ArmButton!=ArmingButton||ArmDevice!=ArmingDevice))
    {
        const auto* Device=GetSelectedDevice();
        if(!IsConnected()||!Device||Device->Identity!=ArmDevice||ArmButton>=FMath::Min(32,Device->ButtonCount))
        {Status=TEXT("Select the connected controller and learn a button it reports.");return false;}
    }
    auto Root=MakeShared<FJsonObject>();Root->SetNumberField(TEXT("version"),1);
    for(int I=0;I<3;++I)
    {Root->SetNumberField(*FString::Printf(TEXT("expo_%d"),I),Candidate.Expo[I]);Root->SetNumberField(*FString::Printf(TEXT("rate_%d"),I),Candidate.RateDegrees[I]);}
    Root->SetNumberField(TEXT("arm_button"),ArmButton);Root->SetStringField(TEXT("arm_device"),ArmDevice);Root->SetBoolField(TEXT("arm_toggle"),bToggle);
    const FString Path=ImperialUserData::Path(TEXT("Config/PilotResponse.json")),Temp=Path+TEXT(".tmp");FString Json;
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
    bool Saved=FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json))&&
       FFileHelper::SaveStringToFile(Json,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
#if PLATFORM_WINDOWS
    // UE FileManager::Move deletes the destination first. Replace on the same
    // volume without pre-deleting so a sharing/permission failure keeps it.
    Saved=Saved&&::MoveFileExW(*FPaths::ConvertRelativePathToFull(Temp),*FPaths::ConvertRelativePathToFull(Path),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    Saved=false; // Windows-only controller backend; never risk a destructive fallback.
#endif
    if(!Saved)
    {Status=TEXT("Could not save response. Previous settings retained.");return false;}
    Response=Candidate;ArmingButton=ArmButton;ArmingDevice=ArmDevice;bArmingToggle=bToggle;bEnabled=false;
    Status=TEXT("Rates / Expo / ARM saved. USB disabled. Release ARM, lower throttle, then enable again.");return true;
}

bool UImperialRadioSubsystem::SetStickMode(ImperialRadio::StickMode Mode)
{
    if (!ImperialRadio::IsValidStickMode(Mode))
        return SetCalibrationStatus(ImperialRadio::Error::InvalidStickMode, FString());
    if (Guided.Active() || bCalibrationActive)
    {
        Status = TEXT("Finish or cancel calibration before changing the stick layout.");
        return false;
    }
    bEnabled = false;
    SelectedMode = Mode;
    bStickModeChosen = true;
    Status = Calibration.Mode == SelectedMode
        ? FString::Printf(TEXT("Mode %d selected. USB disabled; verify low throttle before enabling."), static_cast<int>(Mode))
        : FString::Printf(TEXT("Mode %d selected. Run STICK SETUP and save before enabling. Previous saved profile is unchanged."), static_cast<int>(Mode));
    return true;
}

const TArray<float>& UImperialRadioSubsystem::GetRawAxes() const
{
    return IsConnected() ? RawAxes : EmptyAxes;
}

uint32 UImperialRadioSubsystem::GetButtons() const { return IsConnected() ? Buttons : 0; }

ImperialRadio::Axes UImperialRadioSubsystem::CurrentSample() const
{
    ImperialRadio::Axes Sample{};
    for (int32 Axis = 0; Axis < FMath::Min(6, RawAxes.Num()); ++Axis) Sample[Axis] = RawAxes[Axis];
    return Sample;
}

ImperialRadio::AxisMask UImperialRadioSubsystem::AvailableAxes() const
{
    ImperialRadio::AxisMask Mask{};
    if (const auto* Device = GetSelectedDevice())
        for (int32 Axis = 0; Axis < 6; ++Axis) Mask[Axis] = Device->AxisAvailable.IsValidIndex(Axis) && Device->AxisAvailable[Axis];
    return Mask;
}

ImperialRadio::Controls UImperialRadioSubsystem::CurrentControls() const
{
    return Guided.Active() || Calibration.Mode != SelectedMode ? ImperialRadio::Controls{}
        : ImperialRadio::MapControls(Calibration, CurrentSample(), AvailableAxes(), IsConnected());
}

float UImperialRadioSubsystem::GetNormalizedRoll() const { return static_cast<float>(CurrentControls().Roll); }
float UImperialRadioSubsystem::GetNormalizedPitch() const { return static_cast<float>(CurrentControls().Pitch); }
float UImperialRadioSubsystem::GetNormalizedYaw() const { return static_cast<float>(CurrentControls().Yaw); }
float UImperialRadioSubsystem::GetThrottle() const { return static_cast<float>(CurrentControls().Throttle); }

FString UImperialRadioSubsystem::GetStatus() const
{
    if (SelectedId != INDEX_NONE && !IsConnected()) return TEXT("Joystick disconnected or stale. USB disabled; keyboard input required.");
    return Status;
}

int32 UImperialRadioSubsystem::GetMappedAxis(int32 Channel) const
{
    return Channel >= 0 && Channel < 4 ? (Guided.Active() ? Guided.Candidate() : Calibration).Mapping[Channel] : INDEX_NONE;
}

bool UImperialRadioSubsystem::IsInverted(int32 Channel) const
{
    return Channel >= 0 && Channel < 4 && (Guided.Active() ? Guided.Candidate() : Calibration).Inverted[Channel];
}

float UImperialRadioSubsystem::GetCapturedMinimum(int32 Axis) const
{
    if (Axis < 0 || Axis >= 6) return 0;
    if (Guided.Active()) return static_cast<float>(Guided.ObservedMinimum(Axis));
    if (RangeCapture.Samples > 0 && FMath::IsFinite(RangeCapture.Minimum[Axis])) return static_cast<float>(RangeCapture.Minimum[Axis]);
    return Calibration.RangeCaptured ? static_cast<float>(Calibration.Ranges[Axis].Minimum) : 0.0f;
}

float UImperialRadioSubsystem::GetCapturedMaximum(int32 Axis) const
{
    if (Axis < 0 || Axis >= 6) return 0;
    if (Guided.Active()) return static_cast<float>(Guided.ObservedMaximum(Axis));
    if (RangeCapture.Samples > 0 && FMath::IsFinite(RangeCapture.Maximum[Axis])) return static_cast<float>(RangeCapture.Maximum[Axis]);
    return Calibration.RangeCaptured ? static_cast<float>(Calibration.Ranges[Axis].Maximum) : 0.0f;
}

void UImperialRadioSubsystem::InvalidateCalibration()
{
    Guided.Cancel();
    bEnabled = false;
    bCalibrationActive = false;
    RangeCapture = ImperialRadio::Capture{};
    Calibration.RangeCaptured = false;
    Calibration.NeutralCaptured = false;
    Calibration.ThrottleMinimumConfirmed = false;
    Calibration.Confirmed = false;
}

bool UImperialRadioSubsystem::SetChannelMapping(int32 Channel, int32 Axis, bool bInvert)
{
    if (Guided.Active()) { Status = TEXT("Finish or cancel stick setup before editing channels."); return false; }
    if (Channel < 0 || Channel >= 4 || Axis < 0 || Axis >= 6)
        return SetCalibrationStatus(ImperialRadio::Error::InvalidAxis, FString());
    if (!AvailableAxes()[Axis]) return SetCalibrationStatus(ImperialRadio::Error::UnavailableAxis, FString());
    if (Calibration.Mapping[Channel] != Axis || Calibration.Inverted[Channel] != bInvert)
    {
        InvalidateCalibration();
        Calibration.Mapping[Channel] = Axis;
        Calibration.Inverted[Channel] = bInvert;
        Status = TEXT("Mapping changed. USB disabled; capture full ranges and neutral again.");
    }
    // Temporary duplicates are allowed while editing; BeginCalibration rejects them.
    return true;
}

void UImperialRadioSubsystem::CycleAxis(int32 Channel)
{
    if (Channel < 0 || Channel >= 4) return;
    const auto Available = AvailableAxes();
    for (int32 Offset = 1; Offset <= 6; ++Offset)
    {
        const int32 Axis = (Calibration.Mapping[Channel] + Offset) % 6;
        if (Available[Axis]) { SetChannelMapping(Channel, Axis, Calibration.Inverted[Channel]); return; }
    }
}

void UImperialRadioSubsystem::ToggleInvert(int32 Channel)
{
    if (Channel >= 0 && Channel < 4) SetChannelMapping(Channel, Calibration.Mapping[Channel], !Calibration.Inverted[Channel]);
}

bool UImperialRadioSubsystem::SetDeadzone(float Deadzone)
{
    if (Guided.Active()) { Status = TEXT("Finish or cancel stick setup before changing deadzone."); return false; }
    if (!FMath::IsFinite(Deadzone) || Deadzone < 0.0f || Deadzone > 0.25f)
        return SetCalibrationStatus(ImperialRadio::Error::InvalidDeadzone, FString());
    bEnabled = false;
    Calibration.Deadzone = Deadzone;
    Status = TEXT("Deadzone updated. USB disabled; save the profile and enable explicitly.");
    return true;
}

bool UImperialRadioSubsystem::SetCalibrationStatus(ImperialRadio::Error Result, const FString& Success)
{
    Status = Result == ImperialRadio::Error::None ? Success : FString(UTF8_TO_TCHAR(ImperialRadio::ErrorText(Result)));
    return Result == ImperialRadio::Error::None;
}

bool UImperialRadioSubsystem::BeginGuidedCalibration()
{
    bEnabled = false;
    bCalibrationActive = false;
    RangeCapture = ImperialRadio::Capture{};
    PollSelectedDevice();
    return SetCalibrationStatus(Guided.Begin(AvailableAxes(), IsConnected(), Calibration.Deadzone, SelectedMode),
        FString::Printf(TEXT("Mode %d setup ready. Center roll/pitch/yaw, lower %s throttle, then START THIS STEP."),
            static_cast<int>(SelectedMode), SelectedMode == ImperialRadio::StickMode::Mode1 ? TEXT("RIGHT") : TEXT("LEFT")));
}

bool UImperialRadioSubsystem::StartGuidedStep()
{
    PollSelectedDevice();
    bEnabled = false;
    return SetCalibrationStatus(Guided.StartStep(IsConnected()),
        TEXT("Move only the indicated direction. Hold both ends 1 second; hold the final end and confirm."));
}

bool UImperialRadioSubsystem::FinishGuidedStep()
{
    PollSelectedDevice();
    const auto Result = Guided.FinishStep(IsConnected());
    return SetCalibrationStatus(Result,
        Guided.Phase() == ImperialRadio::GuidedPhase::Neutral ? TEXT("Last axis captured. Center roll/pitch/yaw and hold throttle fully down.")
                            : TEXT("Axis captured. Return sticks to center and throttle low before starting the next step."));
}

bool UImperialRadioSubsystem::FinishGuidedNeutral()
{
    PollSelectedDevice();
    return SetCalibrationStatus(Guided.FinishNeutral(IsConnected()),
        TEXT("Neutral and low throttle captured. Check the four assignments above, then CONFIRM & SAVE."));
}

bool UImperialRadioSubsystem::SaveGuidedCalibration()
{
    PollSelectedDevice();
    bEnabled = false;
    ImperialRadio::Profile Candidate;
    if (!SetCalibrationStatus(Guided.ConfirmedProfile(Candidate, IsConnected()), FString())) return false;
    // Recheck the current throttle; the pilot may have moved it after neutral.
    if (!ImperialRadio::CanEnable(Candidate, CurrentSample(), AvailableAxes(), IsConnected()))
        return SetCalibrationStatus(ImperialRadio::Error::ThrottleNotLow, FString());
    const ImperialRadio::Profile Previous = Calibration;
    Calibration = Candidate;
    if (!WriteProfile()) { Calibration = Previous; return false; }
    Guided.MarkSaved();
    bStickModeChosen = false;
    UE_LOG(LogTemp, Display, TEXT("IMPERIAL_RADIO_GUIDED_SAVED mode=%d axes=%d,%d,%d,%d keyboard=1"),
        static_cast<int>(Calibration.Mode), Calibration.Mapping[0], Calibration.Mapping[1], Calibration.Mapping[2], Calibration.Mapping[3]);
    Status = FString::Printf(TEXT("Mode %d setup saved. Keyboard active; ENABLE RADIO is a separate action at low throttle."), static_cast<int>(Calibration.Mode));
    return true;
}

void UImperialRadioSubsystem::CancelGuidedCalibration()
{
    if (!Guided.Active() && Guided.Phase() != ImperialRadio::GuidedPhase::Interrupted) return;
    Guided.Cancel();
    bEnabled = false;
    SelectedMode = Calibration.Mode;
    bStickModeChosen = false;
    Status = TEXT("Stick setup cancelled. Previous calibration retained; keyboard active.");
}

bool UImperialRadioSubsystem::BeginCalibration()
{
    Guided.Cancel();
    PollSelectedDevice();
    if (!IsConnected()) return SetCalibrationStatus(ImperialRadio::Error::Disconnected, FString());
    const auto MappingError = ImperialRadio::ValidateMapping(Calibration, AvailableAxes());
    if (MappingError != ImperialRadio::Error::None) return SetCalibrationStatus(MappingError, FString());
    bEnabled = false;
    Calibration.Mode = SelectedMode;
    RangeCapture.Begin(Calibration);
    bCalibrationActive = true;
    RangeCapture.Observe(CurrentSample(), AvailableAxes(), true);
    Status = TEXT("Capturing ranges: sweep every mapped stick fully both ways, then CAPTURE RANGE.");
    return true;
}

bool UImperialRadioSubsystem::FinishRangeCapture()
{
    if (Guided.Active()) return false;
    PollSelectedDevice();
    if (!bCalibrationActive) return SetCalibrationStatus(IsConnected() ? ImperialRadio::Error::NotCapturing : ImperialRadio::Error::Disconnected, FString());
    return SetCalibrationStatus(RangeCapture.FinishRange(Calibration, AvailableAxes(), IsConnected()),
        TEXT("Ranges captured. Center roll/pitch/yaw, hold throttle minimum, then CAPTURE NEUTRAL."));
}

bool UImperialRadioSubsystem::CaptureNeutral()
{
    if (Guided.Active()) return false;
    PollSelectedDevice();
    if (!bCalibrationActive) return SetCalibrationStatus(IsConnected() ? ImperialRadio::Error::NotCapturing : ImperialRadio::Error::Disconnected, FString());
    if (RangeCapture.Active && !FinishRangeCapture()) return false;
    return SetCalibrationStatus(ImperialRadio::CaptureNeutral(Calibration, CurrentSample(), AvailableAxes(), IsConnected()),
        TEXT("Neutral and physical minimum throttle captured. FINISH & SAVE to confirm calibration."));
}

bool UImperialRadioSubsystem::FinishCalibration()
{
    if (Guided.Active()) return false;
    PollSelectedDevice();
    if (!bCalibrationActive)
    {
        if (IsCalibrated()) return SaveProfile();
        return SetCalibrationStatus(IsConnected() ? ImperialRadio::Error::NotCapturing : ImperialRadio::Error::Disconnected, FString());
    }
    if (!SetCalibrationStatus(ImperialRadio::Confirm(Calibration, AvailableAxes(), IsConnected()), FString())) return false;
    if (!SaveProfile()) { Calibration.Confirmed = false; return false; }
    bCalibrationActive = false;
    Status = TEXT("Calibration saved for this device. Keyboard active; lower throttle and ENABLE USB explicitly.");
    return true;
}

bool UImperialRadioSubsystem::EnableRadio()
{
    PollSelectedDevice();
    bEnabled = false;
    if (Guided.Active()) { Status = TEXT("Finish and save stick setup before enabling radio."); return false; }
    if (Calibration.Mode != SelectedMode)
    {
        Status = TEXT("Selected stick layout needs calibration. Complete STICK SETUP and save, or select the saved layout.");
        return false;
    }
    if (!IsConnected()) return SetCalibrationStatus(ImperialRadio::Error::Disconnected, FString());
    const auto Error = ImperialRadio::ValidateProfile(Calibration, AvailableAxes());
    if (Error != ImperialRadio::Error::None) return SetCalibrationStatus(Error, FString());
    if (!ImperialRadio::CanEnable(Calibration, CurrentSample(), AvailableAxes(), true))
        return SetCalibrationStatus(ImperialRadio::Error::ThrottleNotLow, FString());
    bEnabled = true;
    Status = TEXT("USB input enabled. Aircraft remains disarmed until the pilot explicitly arms at low throttle.");
    return true;
}
bool UImperialRadioSubsystem::EnableSavedRadio()
{
    bEnabled=false;
    if(Guided.Active()||bCalibrationActive){Status=TEXT("Finish or cancel calibration before connecting saved radio.");return false;}
    if (GetSelectedDevice() && Calibration.Mode != SelectedMode)
    {
        Status = TEXT("Stick layout changed. Complete STICK SETUP, or select the saved layout before CONNECT SAVED RADIO.");
        return false;
    }
    RefreshDevices();
    TSharedPtr<FJsonObject> Root;
    const TSharedPtr<FJsonObject>* Profiles=nullptr;
    if(!ReadJsonFile(ProfilePath(),Root)||!Root->TryGetObjectField(TEXT("profiles"),Profiles)||!Profiles||!Profiles->IsValid())
    {Status=TEXT("No saved radio profile. Open C / RADIO to select and calibrate the device.");return false;}
    TArray<int32> Matches;
    for(const auto& Device:Devices)
    {
        const TSharedPtr<FJsonObject>* Entry=nullptr;bool Confirmed=false;
        if((*Profiles)->TryGetObjectField(Device.Identity,Entry)&&Entry&&Entry->IsValid()&&(*Entry)->TryGetBoolField(TEXT("confirmed"),Confirmed)&&Confirmed)Matches.Add(Device.Id);
    }
    if(Matches.Num()!=1){Status=TEXT("Select the connected radio explicitly in C / RADIO; no unique saved device found.");return false;}
    if (bStickModeChosen)
    {
        // F8 must not silently undo an explicit choice made before selecting a
        // device. An untouched cold start can still restore either saved mode.
        const auto* Match = Devices.FindByPredicate([&](const FImperialRadioDevice& Device) { return Device.Id == Matches[0]; });
        const TSharedPtr<FJsonObject>* Entry = nullptr;
        ImperialRadio::Profile SavedMode;
        if (!Match || !(*Profiles)->TryGetObjectField(Match->Identity, Entry) || !Entry ||
            !ImperialRadio::ReadProfileMode(*Entry, SavedMode) || SavedMode.Mode != SelectedMode)
        {
            Status = TEXT("Saved layout differs from your choice. Select the device and calibrate its actual layout before enabling.");
            return false;
        }
    }
    return SelectDevice(Matches[0])&&IsCalibrated()&&EnableRadio();
}

void UImperialRadioSubsystem::DisableRadio()
{
    bEnabled = false;
    Status = TEXT("Keyboard input selected. USB calibration retained; enabling requires low throttle.");
}

FString UImperialRadioSubsystem::ProfilePath() const
{
    return FPaths::Combine(ImperialUserData::Directory(), TEXT("Config/RadioProfile.json"));
}

bool UImperialRadioSubsystem::HasSavedProfiles() const
{
    TSharedPtr<FJsonObject> Root;double Version=0;
    const TSharedPtr<FJsonObject>* Profiles=nullptr;
    return ReadJsonFile(ProfilePath(),Root)&&ReadFinite(Root,TEXT("version"),Version)&&Version==1.0&&
        Root->TryGetObjectField(TEXT("profiles"),Profiles)&&Profiles&&Profiles->IsValid()&&!(*Profiles)->Values.IsEmpty();
}

bool UImperialRadioSubsystem::ExportDeviceReport()
{
    RefreshDevices();
    auto Root=MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("utc"),FDateTime::UtcNow().ToIso8601());
    Root->SetStringField(TEXT("build"),ImperialBuildInfo::Label());
    Root->SetStringField(TEXT("source_sha256"),ImperialBuildInfo::SourceHash());
    Root->SetStringField(TEXT("input_api"),TEXT("Windows WinMM joyGetPosEx"));
    Root->SetNumberField(TEXT("stick_mode"), static_cast<int>(SelectedMode));
    Root->SetStringField(TEXT("profile_path"),ProfilePath());
    Root->SetBoolField(TEXT("connected"),IsConnected());Root->SetBoolField(TEXT("calibrated"),IsCalibrated());Root->SetBoolField(TEXT("enabled"),IsEnabled());
    TArray<TSharedPtr<FJsonValue>> Entries;
    for(const auto& Device:Devices)
    {
        auto Entry=MakeShared<FJsonObject>();
        Entry->SetNumberField(TEXT("slot"),Device.Id);Entry->SetStringField(TEXT("name"),Device.Name);
        Entry->SetStringField(TEXT("driver_id"),FString::Printf(TEXT("%04X:%04X"),Device.ManufacturerId,Device.ProductId));
        Entry->SetNumberField(TEXT("axes"),Device.AxisCount);Entry->SetNumberField(TEXT("buttons"),Device.ButtonCount);
        Entry->SetBoolField(TEXT("selected"),Device.Id==SelectedId&&Device.Identity==SelectedIdentity);
        Entries.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Root->SetArrayField(TEXT("devices"),Entries);
    const FString Dir=ImperialUserData::Path(TEXT("Diagnostics"));IFileManager::Get().MakeDirectory(*Dir,true);
    const FString Path=FPaths::Combine(Dir,TEXT("radio-")+FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"))+TEXT(".json"));
    FString Json;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));
    const bool Saved=FFileHelper::SaveStringToFile(Json,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    Status=Saved?TEXT("Device report saved in your user-data Diagnostics folder. No calibration was changed."):TEXT("Cannot write user data. Check your Windows user folder permissions.");
    return Saved;
}

bool UImperialRadioSubsystem::SaveProfile()
{
    if (Guided.Active()) { Status = TEXT("Use CONFIRM & SAVE after completing stick setup."); return false; }
    return WriteProfile();
}

bool UImperialRadioSubsystem::WriteProfile()
{
    if (Calibration.Mode != SelectedMode)
    {
        Status = TEXT("Cannot save another layout's calibration. Complete STICK SETUP for the selected mode.");
        return false;
    }
    const FImperialRadioDevice* Device = GetSelectedDevice();
    if (!Device || !IsConnected()) return SetCalibrationStatus(ImperialRadio::Error::Disconnected, FString());
    const auto Error = ImperialRadio::ValidateProfile(Calibration, AvailableAxes());
    if (Error != ImperialRadio::Error::None) return SetCalibrationStatus(Error, FString());
    const FString Path = ProfilePath();
    TSharedPtr<FJsonObject> Root;
    if (IFileManager::Get().FileExists(*Path))
    {
        double Version = 0;
        if (!ReadJsonFile(Path, Root) || !ReadFinite(Root, TEXT("version"), Version) || Version != 1.0)
        {
            Status = TEXT("Existing radio profile file is malformed or unsupported; it was left unchanged.");
            return false;
        }
    }
    else Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("version"), 1);
    const TSharedPtr<FJsonObject>* StoredProfiles = nullptr;
    TSharedPtr<FJsonObject> Profiles = Root->TryGetObjectField(TEXT("profiles"), StoredProfiles) && StoredProfiles && StoredProfiles->IsValid()
        ? *StoredProfiles : MakeShared<FJsonObject>();
    auto Entry = MakeShared<FJsonObject>();
    ImperialRadio::WriteProfileMode(Entry, Calibration);
    Entry->SetStringField(TEXT("identity"), Device->Identity);
    Entry->SetStringField(TEXT("device_name"), Device->Name);
    Entry->SetNumberField(TEXT("manufacturer_id"), Device->ManufacturerId);
    Entry->SetNumberField(TEXT("product_id"), Device->ProductId);
    Entry->SetNumberField(TEXT("deadzone"), Calibration.Deadzone);
    Entry->SetBoolField(TEXT("ranges_captured"), Calibration.RangeCaptured);
    Entry->SetBoolField(TEXT("neutral_captured"), Calibration.NeutralCaptured);
    Entry->SetBoolField(TEXT("throttle_min_confirmed"), Calibration.ThrottleMinimumConfirmed);
    Entry->SetBoolField(TEXT("confirmed"), Calibration.Confirmed);
    TArray<TSharedPtr<FJsonValue>> Mapping;
    for (int32 Channel = 0; Channel < 4; ++Channel)
    {
        auto Item = MakeShared<FJsonObject>();
        Item->SetNumberField(TEXT("axis"), Calibration.Mapping[Channel]);
        Item->SetBoolField(TEXT("invert"), Calibration.Inverted[Channel]);
        Mapping.Add(MakeShared<FJsonValueObject>(Item));
    }
    Entry->SetArrayField(TEXT("mapping"), Mapping);
    TArray<TSharedPtr<FJsonValue>> Ranges;
    for (const auto& Range : Calibration.Ranges)
    {
        auto Item = MakeShared<FJsonObject>();
        Item->SetNumberField(TEXT("min"), Range.Minimum);
        Item->SetNumberField(TEXT("max"), Range.Maximum);
        Item->SetNumberField(TEXT("center"), Range.Center);
        Ranges.Add(MakeShared<FJsonValueObject>(Item));
    }
    Entry->SetArrayField(TEXT("ranges"), Ranges);
    Profiles->SetObjectField(Device->Identity, Entry);
    Root->SetObjectField(TEXT("profiles"), Profiles);
    FString Json;
    if (!FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Json))) return false;
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    const FString TempPath = Path + TEXT(".tmp");
    const bool bSaved = FFileHelper::SaveStringToFile(Json, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) &&
        IFileManager::Get().Move(*Path, *TempPath, true, true, false, true);
    Status = bSaved ? TEXT("Device calibration saved for this Windows user. USB activation is never saved.") : TEXT("Could not save calibration. Check your Windows user-data folder permissions.");
    return bSaved;
}

bool UImperialRadioSubsystem::LoadProfile()
{
    Guided.Cancel();
    bEnabled = false;
    bCalibrationActive = false;
    Calibration = ImperialRadio::Profile{};
    RangeCapture = ImperialRadio::Capture{};
    const auto* Device = GetSelectedDevice();
    if (!Device || !IsConnected()) return SetCalibrationStatus(ImperialRadio::Error::Disconnected, FString());
    TSharedPtr<FJsonObject> Root;
    if (!IFileManager::Get().FileExists(*ProfilePath()))
    {
        Status = TEXT("No saved calibration for this device. Keyboard active; map channels and start range capture.");
        return false;
    }
    const auto Reject = [this]() { Status = TEXT("Saved profile invalid or belongs to another device. Keyboard active; calibrate this device."); return false; };
    double Version = 0;
    if (!ReadJsonFile(ProfilePath(), Root) || !ReadFinite(Root, TEXT("version"), Version) || Version != 1.0) return Reject();
    const TSharedPtr<FJsonObject>* Profiles = nullptr;
    const TSharedPtr<FJsonObject>* EntryPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("profiles"), Profiles) || !Profiles || !Profiles->IsValid() ||
        !(*Profiles)->TryGetObjectField(Device->Identity, EntryPtr) || !EntryPtr || !EntryPtr->IsValid()) return Reject();
    const TSharedPtr<FJsonObject> Entry = *EntryPtr;
    FString Identity;
    ImperialRadio::Profile Candidate;
    if (!ImperialRadio::ReadProfileMode(Entry, Candidate) ||
        !Entry->TryGetStringField(TEXT("identity"), Identity) || Identity != Device->Identity ||
        !ReadFinite(Entry, TEXT("deadzone"), Candidate.Deadzone) ||
        !Entry->TryGetBoolField(TEXT("ranges_captured"), Candidate.RangeCaptured) ||
        !Entry->TryGetBoolField(TEXT("neutral_captured"), Candidate.NeutralCaptured) ||
        !Entry->TryGetBoolField(TEXT("throttle_min_confirmed"), Candidate.ThrottleMinimumConfirmed) ||
        !Entry->TryGetBoolField(TEXT("confirmed"), Candidate.Confirmed)) return Reject();
    const TArray<TSharedPtr<FJsonValue>>* Mapping = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Ranges = nullptr;
    if (!Entry->TryGetArrayField(TEXT("mapping"), Mapping) || !Mapping || Mapping->Num() != 4 ||
        !Entry->TryGetArrayField(TEXT("ranges"), Ranges) || !Ranges || Ranges->Num() != 6) return Reject();
    for (int32 Channel = 0; Channel < 4; ++Channel)
    {
        const auto& Value = (*Mapping)[Channel];
        if (!Value.IsValid() || Value->Type != EJson::Object) return Reject();
        const auto Item = Value->AsObject();
        double Axis = 0;
        if (!ReadFinite(Item, TEXT("axis"), Axis) || Axis < 0 || Axis > 5 || Axis != FMath::FloorToDouble(Axis) ||
            !Item->TryGetBoolField(TEXT("invert"), Candidate.Inverted[Channel])) return Reject();
        Candidate.Mapping[Channel] = static_cast<int>(Axis);
    }
    for (int32 Axis = 0; Axis < 6; ++Axis)
    {
        const auto& Value = (*Ranges)[Axis];
        if (!Value.IsValid() || Value->Type != EJson::Object) return Reject();
        const auto Item = Value->AsObject();
        auto& Range = Candidate.Ranges[Axis];
        if (!ReadFinite(Item, TEXT("min"), Range.Minimum) || !ReadFinite(Item, TEXT("max"), Range.Maximum) ||
            !ReadFinite(Item, TEXT("center"), Range.Center) || Range.Minimum < 0 || Range.Maximum > 1 ||
            Range.Minimum > Range.Maximum || Range.Center < 0 || Range.Center > 1) return Reject();
    }
    if (ImperialRadio::ValidateProfile(Candidate, AvailableAxes()) != ImperialRadio::Error::None) return Reject();
    Calibration = Candidate;
    SelectedMode = Candidate.Mode;
    bStickModeChosen = false;
    Status = FString::Printf(TEXT("Mode %d device calibration loaded. Keyboard active; minimum throttle and explicit ENABLE USB required."), static_cast<int>(SelectedMode));
    return true;
}
