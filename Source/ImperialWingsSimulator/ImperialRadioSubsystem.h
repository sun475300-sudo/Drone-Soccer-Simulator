#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "RadioCalibration.h"
#include "PilotResponse.h"
#include "ImperialRadioSubsystem.generated.h"

/** The WinMM-reported identity is retained verbatim. It does not certify a brand or USB serial. */
struct FImperialRadioDevice
{
    int32 Id = INDEX_NONE;
    FString Name;
    FString Identity;
    FString RegistryKey;
    FString OemName;
    uint16 ManufacturerId = 0;
    uint16 ProductId = 0;
    int32 AxisCount = 0;
    int32 ButtonCount = 0;
    TArray<bool> AxisAvailable;
    TArray<uint32> AxisMinimum;
    TArray<uint32> AxisMaximum;
};

/** Native, explicitly selected USB joystick input. Every launch starts in keyboard mode. */
UCLASS()
class IMPERIALWINGSSIMULATOR_API UImperialRadioSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override;
    virtual bool IsTickableWhenPaused() const override { return true; }
    virtual TStatId GetStatId() const override;
    virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }

    const TArray<FImperialRadioDevice>& GetDevices() const { return Devices; }
    const FImperialRadioDevice* GetSelectedDevice() const;
    const TArray<float>& GetRawAxes() const;
    uint32 GetButtons() const;
    float GetNormalizedRoll() const;
    float GetNormalizedPitch() const;
    float GetNormalizedYaw() const;
    float GetThrottle() const;
    bool IsConnected() const;
    bool IsCalibrated() const;
    bool IsEnabled() const;
    bool IsPrecisionThrottleEnabled() const { return bPrecisionThrottle; }
    bool SetPrecisionThrottleEnabled(bool bEnable);
    float ApplyManualThrottle(float Input) const;
    const ImperialInput::PilotResponse& GetPilotResponse() const { return Response; }
    bool SavePilotResponse(const ImperialInput::PilotResponse& Candidate,int32 ArmButton,const FString& ArmDevice,bool bToggle);
    int32 GetArmButton() const { return ArmingButton; }
    const FString& GetArmDevice() const { return ArmingDevice; }
    bool IsArmToggle() const { return bArmingToggle; }
    ImperialRadio::StickMode GetStickMode() const { return SelectedMode; }
    bool SetStickMode(ImperialRadio::StickMode Mode);
    bool IsCalibrating() const { return bCalibrationActive || Guided.Active(); }
    FString GetStatus() const;
    bool HasSavedProfiles() const;
    FString GetProfilePath() const { return ProfilePath(); }
    bool ExportDeviceReport();
    int32 GetMappedAxis(int32 Channel) const;
    bool IsInverted(int32 Channel) const;
    float GetDeadzone() const { return static_cast<float>(Calibration.Deadzone); }
    float GetCapturedMinimum(int32 Axis) const;
    float GetCapturedMaximum(int32 Axis) const;
    int32 GetCaptureSampleCount() const { return static_cast<int32>(Guided.Active() ? Guided.SampleCount() : RangeCapture.Samples); }
    ImperialRadio::GuidedPhase GetGuidedPhase() const { return Guided.Phase(); }
    int32 GetGuidedChannel() const { return Guided.Channel(); }
    bool IsGuidedActive() const { return Guided.Active(); }
    bool BeginGuidedCalibration();
    bool StartGuidedStep();
    bool FinishGuidedStep();
    bool FinishGuidedNeutral();
    bool SaveGuidedCalibration();
    void CancelGuidedCalibration();

    void RefreshDevices();
    bool SelectDevice(int32 Id);
    void CycleDevice();
    bool SetChannelMapping(int32 Channel, int32 Axis, bool bInvert);
    void CycleAxis(int32 Channel);
    void ToggleInvert(int32 Channel);
    bool SetDeadzone(float Deadzone);
    bool BeginCalibration();
    bool BeginRangeCapture() { return BeginCalibration(); }
    bool FinishRangeCapture();
    bool CaptureNeutral();
    bool FinishCalibration();
    bool SaveProfile();
    bool LoadProfile();
    bool EnableRadio();
    bool EnableSavedRadio();
    void DisableRadio();

private:
    TArray<FImperialRadioDevice> Devices;
    int32 SelectedId = INDEX_NONE;
    FString SelectedIdentity;
    TArray<float> RawAxes;
    TArray<float> EmptyAxes;
    uint32 Buttons = 0;
    ImperialRadio::Profile Calibration;
    ImperialRadio::StickMode SelectedMode = ImperialRadio::StickMode::Mode2;
    bool bStickModeChosen = false;
    bool bPrecisionThrottle = false;
    ImperialInput::PilotResponse Response;
    int32 ArmingButton=INDEX_NONE;
    FString ArmingDevice;
    bool bArmingToggle=true;
    ImperialRadio::Capture RangeCapture;
    ImperialRadio::GuidedCapture Guided;
    bool bConnected = false;
    bool bEnabled = false;
    bool bCalibrationActive = false;
    bool bInitialized = false;
    bool bStorageSmokeDone = false;
    double LastSampleTime = 0.0;
    double LastPollTime = 0.0;
    double LastEnumerationTime = 0.0;
    FString Status = TEXT("Keyboard input. Select a connected joystick to configure USB.");

    void PollSelectedDevice();
    void ClearLiveInput(const FString& Reason);
    void InvalidateCalibration();
    bool SetCalibrationStatus(ImperialRadio::Error Result, const FString& Success);
    ImperialRadio::Axes CurrentSample() const;
    ImperialRadio::AxisMask AvailableAxes() const;
    ImperialRadio::Controls CurrentControls() const;
    bool WriteProfile();
    FString ProfilePath() const;
    void LoadPilotControls();
    void LoadPilotResponse();
};
