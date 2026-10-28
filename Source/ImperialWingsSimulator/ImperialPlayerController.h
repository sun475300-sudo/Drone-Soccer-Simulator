#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PilotResponse.h"
#include "ImperialPlayerController.generated.h"
class ACameraActor;
class AImperialMatchManager;
class ASoccerDronePawn;
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float Dt) override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void FlushPressedKeys() override;
    virtual bool ShouldFlushKeysWhenViewportFocusChanges() const override { return true; }
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;
    /** Menu/aircraft changes clear flight keys without undoing explicit USB enable. */
    void FlushPilotContextKeys();
    bool IsInputResetThisFrame() const;
    uint64 GetInputContextGeneration() const { return InputContextGeneration; }
    void SetView(int32 View);
    FVector GetPilotStationPosition() const;
    void ToggleEquipmentInspection();
    bool IsEquipmentInspection() const { return InspectedDrone.IsValid(); }
    bool IsReadOnlyView() const { return ActiveView==4||ActiveView==5; }
    bool CanSetView(int32 View) const;
    FString GetCameraRoleName() const;
    FVector GetRoleCameraPosition() const;
private:
    UPROPERTY() TObjectPtr<ACameraActor> ArenaCamera;
    UPROPERTY() TObjectPtr<ACameraActor> PilotCamera;
    UPROPERTY() TObjectPtr<ACameraActor> ObserverCamera;
    UPROPERTY() TObjectPtr<ACameraActor> AudienceCamera;
    UPROPERTY() TObjectPtr<ACameraActor> RefereeCamera;
    UPROPERTY() TObjectPtr<AImperialMatchManager> Match;
    bool bPossessed=false;
    bool bCheckedEquipmentShowcase=false;
    UPROPERTY() TWeakObjectPtr<ASoccerDronePawn> InspectedDrone;
    int32 ActiveView=1;
    int32 PilotTeam=INDEX_NONE;
    float PilotHeadYaw=0;
    float PilotHeadPitch=0;
    float PilotFieldOfView=85;
    float ObserverYaw=-135,ObserverPitch=25,ObserverDistance=360;
    FRotator AudienceHead=FRotator::ZeroRotator,RefereeHead=FRotator::ZeroRotator;
    float AudienceFov=70,RefereeFov=100;
    bool bHeadDragging=false;
    bool bPreserveRadioWhileFlushing=false;
    uint64 InputResetAtCycles=0;
    uint64 InputResetFrame=MAX_uint64;
    uint64 InputContextGeneration=0;
    TSet<FKey> FreshFlightKeys;
    FVector2D LastHeadPointer=FVector2D::ZeroVector;
    void ConfigureInput();
    void UpdatePilotCamera();
    void UpdateRoleCameras();
    void UpdateRadioArming();
    ImperialInput::ArmingEdgeGate RadioArmGate;
    void Audience();void Referee();
    bool HasRadioModal() const;
    void ZoomIn(); void ZoomOut();
    void ClickHUD(); void CompactHUD();
    void FirstFlightGuide();
    void SavedRadio();void RecordFlight();
    void Fpv(); void Chase(); void Arena(); void Next(); void Auto(); void Hold(); void Return(); void Reset(); void Export(); void Radio();
};
