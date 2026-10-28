#include "ImperialPlayerController.h"
#include "ImperialMatchManager.h"
#include "SoccerDronePawn.h"
#include "ImperialHUD.h"
#include "ImperialSoccerLayout.h"
#include "ImperialRadioSubsystem.h"
#include "Engine/GameInstance.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "HAL/PlatformTime.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"

namespace
{
void ConfigureSpectatorCamera(UCameraComponent* Camera,float FieldOfView)
{
    Camera->bConstrainAspectRatio=false;Camera->SetFieldOfView(FieldOfView);
    auto& Settings=Camera->PostProcessSettings;
    Settings.bOverride_AutoExposureMethod=true;Settings.AutoExposureMethod=AEM_Manual;
    Settings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Settings.AutoExposureApplyPhysicalCameraExposure=false;
    Settings.bOverride_AutoExposureBias=true;Settings.AutoExposureBias=-4.f;
    Settings.bOverride_MotionBlurAmount=true;Settings.MotionBlurAmount=0;
}
}
void AImperialPlayerController::OnPossess(APawn* InPawn)
{
    TGuardValue<bool> PreserveRadio(bPreserveRadioWhileFlushing,true);
    Super::OnPossess(InPawn);FlushPressedKeys();
}
void AImperialPlayerController::FlushPilotContextKeys()
{
    TGuardValue<bool> PreserveRadio(bPreserveRadioWhileFlushing,true);FlushPressedKeys();
}
void AImperialPlayerController::FlushPressedKeys()
{
    RadioArmGate.Reset();
    InputResetAtCycles=FPlatformTime::Cycles64();InputResetFrame=GFrameCounter;++InputContextGeneration;FreshFlightKeys.Empty();bHeadDragging=false;
    Super::FlushPressedKeys();
    if(auto* Drone=Cast<ASoccerDronePawn>(GetPawn()))Drone->ClearManualPilotInput(TEXT("INPUT RESET / DISARMED. RELEASE KEYS, THEN ARM AGAIN."));
    if(!bPreserveRadioWhileFlushing&&GetGameInstance())
        if(auto* Radio=GetGameInstance()->GetSubsystem<UImperialRadioSubsystem>())Radio->DisableRadio();
}
bool AImperialPlayerController::IsInputResetThisFrame() const{return InputResetFrame==GFrameCounter;}
bool AImperialPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
    // Discard any queued command from before the interrupted input context.
    if(Params.EventTimestamp&&Params.EventTimestamp<InputResetAtCycles)return true;
    const FKey K=Params.Key;
    const bool FlightKey=K==EKeys::Up||K==EKeys::Down||K==EKeys::A||K==EKeys::D||K==EKeys::W||K==EKeys::S||K==EKeys::Q||K==EKeys::E||K==EKeys::SpaceBar;
    if(FlightKey)
    {
        if(HasRadioModal()||IsReadOnlyView()||IsEquipmentInspection()){FreshFlightKeys.Remove(K);return true;}
        if(Params.Event==IE_Repeat&&!FreshFlightKeys.Contains(K))return true;
        if(Params.Event==IE_Pressed)FreshFlightKeys.Add(K);
        else if(Params.Event==IE_Released)FreshFlightKeys.Remove(K);
    }
    return Super::InputKey(Params);
}
void AImperialPlayerController::BeginPlay()
{
    Super::BeginPlay(); bShowMouseCursor=true; bEnableClickEvents=false;bAutoManageActiveCameraTarget=false;
    Match=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));
    ArenaCamera=GetWorld()->SpawnActor<ACameraActor>();
    FVector Location(1750,-2250,1180),Target(0,30,180);
    ArenaCamera->SetActorLocationAndRotation(Location,(Target-Location).Rotation());
    ConfigureSpectatorCamera(ArenaCamera->GetCameraComponent(),60);
    PilotCamera=GetWorld()->SpawnActor<ACameraActor>();
    ConfigureSpectatorCamera(PilotCamera->GetCameraComponent(),PilotFieldOfView);
    ObserverCamera=GetWorld()->SpawnActor<ACameraActor>();ConfigureSpectatorCamera(ObserverCamera->GetCameraComponent(),65);
    AudienceCamera=GetWorld()->SpawnActor<ACameraActor>();ConfigureSpectatorCamera(AudienceCamera->GetCameraComponent(),AudienceFov);
    RefereeCamera=GetWorld()->SpawnActor<ACameraActor>();ConfigureSpectatorCamera(RefereeCamera->GetCameraComponent(),RefereeFov);
    UpdatePilotCamera();ConfigureInput();SetView(Match?Match->GetView():1);
}
void AImperialPlayerController::ConfigureInput()
{
    bShowMouseCursor=true;bEnableClickEvents=false;
    SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
    // NoCapture suppresses mouse-down InputKey in a standalone game viewport.
    // Capture only for the press, keep the visible cursor unlocked and unhidden.
    if(auto* V=GetWorld()->GetGameViewport()){V->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringMouseDown);V->SetMouseLockMode(EMouseLockMode::DoNotLock);V->SetHideCursorDuringCapture(false);}
}
void AImperialPlayerController::PlayerTick(float Dt)
{
    Super::PlayerTick(Dt);
    if(Match&&!bPossessed&&Match->GetSelectedDrone()) {Possess(Match->GetSelectedDrone());bPossessed=true;SetView(Match->GetView());}
    if(bPossessed&&!bCheckedEquipmentShowcase)
    {bCheckedEquipmentShowcase=true;if(FParse::Param(FCommandLine::Get(),TEXT("ImperialEquipmentShowcase")))ToggleEquipmentInspection();}
    if(InspectedDrone.IsValid())
    {
        if(!Match||Match->GetSelectedDrone()!=InspectedDrone.Get()||!InspectedDrone->IsEquipmentInspection())
        {InspectedDrone->SetEquipmentInspection(false);InspectedDrone.Reset();if(Match)Match->ChangeView(1);}
        else
        {
            const FVector Centre=InspectedDrone->GetActorLocation();
            const bool Macro=FParse::Param(FCommandLine::Get(),TEXT("ImperialEquipmentMacro"));
            const FVector Position=Centre+(Macro?FVector(17,-21,17):FVector(32,-42,30));
            ArenaCamera->SetActorLocationAndRotation(Position,(Centre+FVector(0,0,1)-Position).Rotation());
            ArenaCamera->GetCameraComponent()->SetFieldOfView(45);
        }
    }
    const auto* HUD=Cast<AImperialHUD>(GetHUD());
    const bool RadioOpen=HUD&&HUD->IsPilotMenuOpen();
    UpdateRadioArming();
    float X=0,Y=0;
    if((ActiveView==1||ActiveView==2||ActiveView==4||ActiveView==5)&&!RadioOpen&&IsInputKeyDown(EKeys::RightMouseButton)&&GetMousePosition(X,Y))
    {
        const FVector2D Pointer(X,Y);
        // Use visible cursor movement rather than high-precision captured mouse axes.
        if(bHeadDragging)
        {
            const FVector2D Delta=Pointer-LastHeadPointer;
            if(ActiveView==1){PilotHeadYaw=FMath::Clamp(PilotHeadYaw+static_cast<float>(Delta.X)*.18f,-60.f,60.f);PilotHeadPitch=FMath::Clamp(PilotHeadPitch-static_cast<float>(Delta.Y)*.14f,-25.f,40.f);}
            else if(ActiveView==2){ObserverYaw+=Delta.X*.18f;ObserverPitch=FMath::Clamp(ObserverPitch-static_cast<float>(Delta.Y)*.14f,10.f,70.f);}
            else {auto& Head=ActiveView==4?AudienceHead:RefereeHead;Head.Yaw=FMath::Clamp(Head.Yaw+Delta.X*.18f,-65.,65.);Head.Pitch=FMath::Clamp(Head.Pitch-Delta.Y*.14f,-25.,40.);}
        }
        else if(!HUD||!HUD->IsPointerOverUI(Pointer))bHeadDragging=true;
        LastHeadPointer=Pointer;
    }
    else bHeadDragging=false;
    if(ActiveView==1)UpdatePilotCamera();
    UpdateRoleCameras();
    if(ActiveView==3&&ArenaCamera&&Match&&Match->GetSelectedDrone()&&FParse::Param(FCommandLine::Get(),TEXT("ImperialShowcase")))
    {
        const FVector Centre=Match->GetSelectedDrone()->GetActorLocation();
        const FVector Location=Centre+FVector(80,-90,45);
        ArenaCamera->SetActorLocationAndRotation(Location,(Centre+FVector(0,0,1)-Location).Rotation());
        ArenaCamera->GetCameraComponent()->SetFieldOfView(45);
    }
    // View 2 is an external training observer, even if legacy pawn camera input is pressed.
    if(ActiveView==2&&Match&&Match->GetSelectedDrone())Match->GetSelectedDrone()->SetChaseView(true);
}
void AImperialPlayerController::UpdateRadioArming()
{
    auto* R=GetGameInstance()?GetGameInstance()->GetSubsystem<UImperialRadioSubsystem>():nullptr;
    auto* D=Cast<ASoccerDronePawn>(GetPawn());const auto* Device=R?R->GetSelectedDevice():nullptr;
    const int32 Button=R?R->GetArmButton():INDEX_NONE;
    const bool Valid=R&&D&&Device&&R->IsEnabled()&&Button>=0&&Button<FMath::Min(32,Device->ButtonCount)&&
        R->GetArmDevice()==Device->Identity&&!HasRadioModal()&&!IsReadOnlyView()&&!IsEquipmentInspection()&&
        !IsInputResetThisFrame()&&!D->IsAutonomousEnabled();
    const bool Pressed=Valid&&(R->GetButtons()&(uint32(1)<<Button))!=0;
    const auto Request=RadioArmGate.Step(Valid,Pressed,R?R->IsArmToggle():true,D&&D->IsArmed(),R?R->GetThrottle():1);
    if(Request==ImperialInput::ArmRequest::Disarm&&D)D->SetArmed(false);
    else if(Request==ImperialInput::ArmRequest::Arm&&D)D->SetArmed(true); // Existing low-throttle/match/view gates still apply.
}

void AImperialPlayerController::UpdateRoleCameras()
{
    if(ObserverCamera&&Match&&Match->GetSelectedDrone())
    {const FVector Target=Match->GetSelectedDrone()->GetActorLocation();const FVector Position=Target+FRotator(ObserverPitch,ObserverYaw,0).Vector()*ObserverDistance;ObserverCamera->SetActorLocationAndRotation(Position,(Target-Position).Rotation());}
    if(AudienceCamera){const FVector Position(300,-1050,250);const FRotator Base=(FVector(0,0,180)-Position).Rotation();AudienceCamera->SetActorLocationAndRotation(Position,Base+AudienceHead);AudienceCamera->GetCameraComponent()->SetFieldOfView(AudienceFov);}
    if(RefereeCamera){const FVector Position(220,-550,170);const FRotator Base=(FVector(0,0,190)-Position).Rotation();RefereeCamera->SetActorLocationAndRotation(Position,Base+RefereeHead);RefereeCamera->GetCameraComponent()->SetFieldOfView(RefereeFov);}
}
bool AImperialPlayerController::CanSetView(int32 View) const
{const auto* Drone=Match?Match->GetSelectedDrone():nullptr;return View>=1&&View<=6&&(View!=6||InspectedDrone.IsValid())&&!((View==4||View==5)&&Drone&&Drone->IsArmed()&&!Drone->IsAutonomousEnabled());}
FString AImperialPlayerController::GetCameraRoleName() const
{if(InspectedDrone.IsValid())return TEXT("EQUIPMENT");switch(ActiveView){case 1:return TEXT("PILOT LOS");case 2:return TEXT("OBSERVER TRACK");case 3:return TEXT("ARENA OVERVIEW");case 4:return TEXT("AUDIENCE STANDS");case 5:return TEXT("REFEREE SIDELINE");default:return TEXT("UNKNOWN");}}
FVector AImperialPlayerController::GetRoleCameraPosition() const
{const ACameraActor* Camera=ActiveView==1?PilotCamera.Get():ActiveView==2?ObserverCamera.Get():ActiveView==4?AudienceCamera.Get():ActiveView==5?RefereeCamera.Get():ArenaCamera.Get();return Camera?Camera->GetActorLocation():FVector::ZeroVector;}
void AImperialPlayerController::UpdatePilotCamera()
{
    if(!PilotCamera)return;
    const int32 Team=Match&&Match->GetSelected()>=5?1:0;
    if(Team!=PilotTeam){PilotTeam=Team;PilotHeadYaw=0;PilotHeadPitch=0;bHeadDragging=false;}
    const int32 Slot=Match?Match->GetSelected()%5:0;
    const FVector Station=ImperialSoccerLayout::PilotPosition(Team,Slot);
    const FVector OpponentGoal(Team==0?650.f:-650.f,0,170);
    const FRotator Facing=(OpponentGoal-Station).Rotation();
    PilotCamera->SetActorLocationAndRotation(Station,FRotator(Facing.Pitch+PilotHeadPitch,Facing.Yaw+PilotHeadYaw,0));
    PilotCamera->GetCameraComponent()->SetFieldOfView(PilotFieldOfView);
}
FVector AImperialPlayerController::GetPilotStationPosition() const {return PilotCamera?PilotCamera->GetActorLocation():FVector::ZeroVector;}
void AImperialPlayerController::SetView(int32 View)
{
    if(!Match||!CanSetView(View)) return;
    if(ActiveView==6||View==6||IsReadOnlyView()||View==4||View==5)FlushPilotContextKeys();
    if(View!=6&&InspectedDrone.IsValid()){InspectedDrone->SetEquipmentInspection(false);InspectedDrone.Reset();}
    if(View==3&&!InspectedDrone.IsValid()&&ArenaCamera)
    {const FVector Position(1750,-2250,1180),Target(0,30,180);ArenaCamera->SetActorLocationAndRotation(Position,(Target-Position).Rotation());ArenaCamera->GetCameraComponent()->SetFieldOfView(60);}
    ActiveView=FMath::Clamp(View,1,6);bHeadDragging=false;ConfigureInput();UpdateRoleCameras();
    if(IsReadOnlyView()){if(auto* Radio=GetGameInstance()->GetSubsystem<UImperialRadioSubsystem>())Radio->DisableRadio();}
    if(ActiveView==1&&PilotCamera){UpdatePilotCamera();SetViewTargetWithBlend(PilotCamera,.25f);}
    else if((ActiveView==3||ActiveView==6)&&ArenaCamera)SetViewTargetWithBlend(ArenaCamera,.3f);
    else if(ActiveView==2&&ObserverCamera)SetViewTargetWithBlend(ObserverCamera,.25f);
    else if(ActiveView==4&&AudienceCamera)SetViewTargetWithBlend(AudienceCamera,.25f);
    else if(ActiveView==5&&RefereeCamera)SetViewTargetWithBlend(RefereeCamera,.25f);
}
void AImperialPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::One,IE_Pressed,this,&AImperialPlayerController::Fpv);
    InputComponent->BindKey(EKeys::Two,IE_Pressed,this,&AImperialPlayerController::Chase);
    InputComponent->BindKey(EKeys::Three,IE_Pressed,this,&AImperialPlayerController::Arena);
    InputComponent->BindKey(EKeys::Four,IE_Pressed,this,&AImperialPlayerController::Audience);
    InputComponent->BindKey(EKeys::Five,IE_Pressed,this,&AImperialPlayerController::Referee);
    InputComponent->BindKey(EKeys::Tab,IE_Pressed,this,&AImperialPlayerController::Next);
    InputComponent->BindKey(EKeys::Enter,IE_Pressed,this,&AImperialPlayerController::Auto);
    InputComponent->BindKey(EKeys::H,IE_Pressed,this,&AImperialPlayerController::Hold);
    InputComponent->BindKey(EKeys::R,IE_Pressed,this,&AImperialPlayerController::Return);
    InputComponent->BindKey(EKeys::Home,IE_Pressed,this,&AImperialPlayerController::Reset);
    InputComponent->BindKey(EKeys::F1,IE_Pressed,this,&AImperialPlayerController::FirstFlightGuide);
    InputComponent->BindKey(EKeys::F5,IE_Pressed,this,&AImperialPlayerController::Export);
    InputComponent->BindKey(EKeys::F6,IE_Pressed,this,&AImperialPlayerController::RecordFlight);
    InputComponent->BindKey(EKeys::F8,IE_Pressed,this,&AImperialPlayerController::SavedRadio);
    InputComponent->BindKey(EKeys::F9,IE_Pressed,this,&AImperialPlayerController::ToggleEquipmentInspection);
    InputComponent->BindKey(EKeys::C,IE_Pressed,this,&AImperialPlayerController::Radio);
    InputComponent->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&AImperialPlayerController::ClickHUD);
    InputComponent->BindKey(EKeys::F10,IE_Pressed,this,&AImperialPlayerController::CompactHUD);
    InputComponent->BindKey(EKeys::MouseScrollUp,IE_Pressed,this,&AImperialPlayerController::ZoomIn);
    InputComponent->BindKey(EKeys::MouseScrollDown,IE_Pressed,this,&AImperialPlayerController::ZoomOut);
}
void AImperialPlayerController::ZoomIn(){if(HasRadioModal())return;if(ActiveView==1){PilotFieldOfView=FMath::Clamp(PilotFieldOfView-5.f,40.f,85.f);UpdatePilotCamera();}else if(ActiveView==2)ObserverDistance=FMath::Clamp(ObserverDistance-35.f,100.f,900.f);else if(ActiveView==4)AudienceFov=FMath::Clamp(AudienceFov-5.f,40.f,85.f);else if(ActiveView==5)RefereeFov=FMath::Clamp(RefereeFov-5.f,60.f,110.f);}
void AImperialPlayerController::ZoomOut(){if(HasRadioModal())return;if(ActiveView==1){PilotFieldOfView=FMath::Clamp(PilotFieldOfView+5.f,40.f,85.f);UpdatePilotCamera();}else if(ActiveView==2)ObserverDistance=FMath::Clamp(ObserverDistance+35.f,100.f,900.f);else if(ActiveView==4)AudienceFov=FMath::Clamp(AudienceFov+5.f,40.f,85.f);else if(ActiveView==5)RefereeFov=FMath::Clamp(RefereeFov+5.f,60.f,110.f);}
bool AImperialPlayerController::HasRadioModal() const {const auto* H=Cast<AImperialHUD>(GetHUD());return H&&H->IsPilotMenuOpen();}
void AImperialPlayerController::Fpv(){if(Match&&!HasRadioModal())Match->ChangeView(1);}
void AImperialPlayerController::Chase(){if(Match&&!HasRadioModal())Match->ChangeView(2);}
void AImperialPlayerController::Arena(){if(Match&&!HasRadioModal())Match->ChangeView(3);}
void AImperialPlayerController::Audience(){if(Match&&!HasRadioModal())Match->ChangeView(4);}
void AImperialPlayerController::Referee(){if(Match&&!HasRadioModal())Match->ChangeView(5);}
void AImperialPlayerController::Next(){if(Match&&!HasRadioModal())Match->SelectNext();}
void AImperialPlayerController::Auto(){if(HasRadioModal()){if(auto* H=Cast<AImperialHUD>(GetHUD()))H->AdvanceRadioSetup();return;}if(Match&&!IsReadOnlyView())Match->LaunchSwarm();}
void AImperialPlayerController::Hold(){if(Match&&!HasRadioModal()&&!IsReadOnlyView())Match->HoldFleet();}
void AImperialPlayerController::Return(){if(Match&&!HasRadioModal()&&!IsReadOnlyView())Match->ReturnFleet();}
void AImperialPlayerController::Reset(){if(Match&&!HasRadioModal()&&!IsReadOnlyView())Match->ResetMatch();}
void AImperialPlayerController::Export(){if(Match)Match->ExportTelemetry();}
void AImperialPlayerController::RecordFlight(){if(Match)Match->ToggleFlightRecording();}
void AImperialPlayerController::ToggleEquipmentInspection()
{
    if(!Match||HasRadioModal())return;
    if(InspectedDrone.IsValid()){InspectedDrone->SetEquipmentInspection(false);InspectedDrone.Reset();Match->ChangeView(1);return;}
    auto* Drone=Match->GetSelectedDrone();if(!Drone||Drone->IsArmed())return;
    Match->ManualSelected();InspectedDrone=Drone;Drone->SetEquipmentInspection(true);Match->ChangeView(6);
}
void AImperialPlayerController::SavedRadio()
{
    if(HasRadioModal()||!Match||IsReadOnlyView())return;
    auto* R=GetGameInstance()->GetSubsystem<UImperialRadioSubsystem>();if(!R)return;
    const bool WasEnabled=R->IsEnabled();Match->ManualSelected();
    if(WasEnabled)R->DisableRadio();else R->EnableSavedRadio();
}
void AImperialPlayerController::Radio(){if(auto* H=Cast<AImperialHUD>(GetHUD()))H->ToggleRadioPanel();}
void AImperialPlayerController::ClickHUD(){if(auto* H=Cast<AImperialHUD>(GetHUD()))H->ClickPointer();}
void AImperialPlayerController::CompactHUD(){if(auto* H=Cast<AImperialHUD>(GetHUD()))H->ToggleCompactView();}

void AImperialPlayerController::FirstFlightGuide(){if(auto* H=Cast<AImperialHUD>(GetHUD()))H->ToggleFirstFlightGuide();}
