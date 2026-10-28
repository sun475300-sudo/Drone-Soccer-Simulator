#include "ImperialHUD.h"
#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "SoccerDronePawn.h"
#include "ImperialRadioSubsystem.h"
#include "ImperialSoccerLayout.h"
#include "ImperialBuildInfo.h"
#include "ImperialPresentationEffects.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
const FLinearColor Navy(.018f,.032f,.052f,.94f), Edge(.13f,.27f,.34f,.8f);
const FLinearColor White(.89f,.96f,1.f,1.f), Muted(.43f,.59f,.68f,1.f);
const FLinearColor Cyan(.10f,.75f,.96f,1.f), Orange(1.f,.16f,.11f,1.f);
const FLinearColor Green(.25f,.92f,.64f,1.f), Amber(1.f,.71f,.24f,1.f);
FString UnitName(int32 Id) { return FString::Printf(TEXT("%s-%02d"), Id < 5 ? TEXT("IW") : TEXT("WG"), Id % 5 + 1); }
FString TimeLabel(double Seconds) { int32 T=FMath::Max(0,FMath::FloorToInt(Seconds));return FString::Printf(TEXT("%02d:%02d"),T/60,T%60); }
}

AImperialMatchManager* AImperialHUD::FindMatch()
{
    if (!Match.IsValid()) Match = Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AImperialMatchManager::StaticClass()));
    return Match.Get();
}
FVector2D AImperialHUD::Screen(float X,float Y) const { return Origin+FVector2D(X,Y)*Scale; }
void AImperialHUD::Rect(float X,float Y,float W,float H,FLinearColor C)
{
    const auto P=Screen(X,Y);DrawRect(C,P.X,P.Y,W*Scale,H*Scale);
}
void AImperialHUD::Text(const FString& V,float X,float Y,float Size,FLinearColor C,bool Right)
{
    UFont* F=GEngine?GEngine->GetMediumFont():nullptr;if(!F||!Canvas)return;
    float W=0,H=0;Canvas->StrLen(F,TEXT("Ag"),W,H);const float S=Size/FMath::Max(1.f,H);
    W=0;if(Right)Canvas->TextSize(F,V,W,H,S,S);
    const auto P=Screen(X-W,Y);DrawText(V,C,P.X,P.Y,F,S*Scale);
}
FString AImperialHUD::Fit(const FString& V,float Size,float Width)const
{
    UFont* F=GEngine?GEngine->GetMediumFont():nullptr;if(!F||!Canvas)return V;
    float W=0,H=0;Canvas->StrLen(F,TEXT("Ag"),W,H);const float S=Size/FMath::Max(1.f,H);
    Canvas->TextSize(F,V,W,H,S,S);if(W<=Width)return V;FString R=V;
    do{R.LeftChopInline(1);Canvas->TextSize(F,R+TEXT("..."),W,H,S,S);}while(W>Width&&R.Len()>0);
    return R+TEXT("...");
}
void AImperialHUD::Panel(float X,float Y,float W,float H)
{
    Rect(X,Y,W,H,Navy);Rect(X,Y,W,1,Edge);Rect(X,Y+H-1,W,1,Edge);
    Rect(X,Y,1,H,Edge);Rect(X+W-1,Y,1,H,Edge);Bounds.Emplace(Screen(X,Y),Screen(X+W,Y+H));
}
void AImperialHUD::Button(FName Name,const FString& Label,float X,float Y,float W,float H,bool Active,bool Warning)
{
    const auto* RoleController=Cast<AImperialPlayerController>(PlayerOwner);
    const bool ReadOnly=RoleController&&RoleController->IsReadOnlyView();
    const bool AutonomousAction=Name==TEXT("Auto")||Name==TEXT("Hold")||Name==TEXT("Return");
    const bool Disabled=(ReadOnly&&(AutonomousAction||Name==TEXT("Manual")||Name==TEXT("Reset")||Name==TEXT("StartMatch")||Name==TEXT("Arm")||Name==TEXT("ArmSelected")))||
        (Match.IsValid()&&!Match->IsAutonomyAllowed()&&AutonomousAction);
    const bool Hover=FBox2D(Screen(X,Y),Screen(X+W,Y+H)).IsInside(Pointer);
    const auto Accent=Warning?Orange:Cyan;
    Rect(X,Y,W,H,Active||Hover?FLinearColor(.045f,.23f,.28f,.97f):FLinearColor(.055f,.10f,.15f,.97f));
    Rect(X,Y,W,1,Active||Hover?Accent:Edge);if(Active||Hover)Rect(X,Y,2,H,Accent);
    Text(Label,X+11,Y+(H-13)*.5f,13,Disabled?Muted:(Active||Hover||Warning?Accent:White));
    if(!Disabled)ButtonTargets.Add({Name,FBox2D(Screen(X,Y),Screen(X+W,Y+H)),ButtonPriority});
}
void AImperialHUD::ClickPointer()
{
    float X=0,Y=0;if(!PlayerOwner||!PlayerOwner->GetMousePosition(X,Y))return;
    const FVector2D Point(X,Y);
    for(int32 I=ButtonTargets.Num()-1;I>=0;--I)
    {
        const auto& Target=ButtonTargets[I];
        if(IsPilotMenuOpen()&&Target.Priority<100)continue;
        if(Target.Bounds.IsInside(Point)){NotifyHitBoxClick(Target.Name);return;}
    }
}
bool AImperialHUD::IsPointerOverUI(FVector2D P)const {if(IsPilotMenuOpen())return true;for(const auto& B:Bounds)if(B.IsInside(P))return true;return false;}

UImperialRadioSubsystem* AImperialHUD::GetRadio() const
{
    UGameInstance* Instance=GetGameInstance();
    return Instance?Instance->GetSubsystem<UImperialRadioSubsystem>():nullptr;
}

void AImperialHUD::ToggleRadioPanel()
{
    bAIPracticePanel=false;bRadioPanelOpen=!bRadioPanelOpen;
    bResponsePanel=false;bLearnArm=false;
    if(bRadioPanelOpen){bRadioAdvanced=false;if(auto* Manager=FindMatch())Manager->ManualSelected();}
    if(!bRadioPanelOpen)if(auto* Radio=GetRadio())Radio->CancelGuidedCalibration();
    // Preserve a radio deliberately enabled in this menu when closing it.
    if(auto* Pilot=Cast<AImperialPlayerController>(PlayerOwner))Pilot->FlushPilotContextKeys();
}
AImperialPresentationEffects* AImperialHUD::GetPresentation()
{
    if(!Presentation.IsValid())Presentation=Cast<AImperialPresentationEffects>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialPresentationEffects::StaticClass()));
    return Presentation.Get();
}

void AImperialHUD::AdvanceRadioSetup()
{
    auto* Radio=GetRadio();if(!bRadioPanelOpen||!Radio)return;
    switch(Radio->GetGuidedPhase())
    {
    case ImperialRadio::GuidedPhase::Ready:Radio->StartGuidedStep();break;
    case ImperialRadio::GuidedPhase::Capturing:Radio->FinishGuidedStep();break;
    case ImperialRadio::GuidedPhase::Neutral:Radio->FinishGuidedNeutral();break;
    default:break; // Final saving remains a deliberate click on the review page.
    }
}

void AImperialHUD::DrawHUD()
{
    Super::DrawHUD();if(!Canvas)return;Bounds.Reset();ButtonTargets.Reset();ButtonPriority=10;
    if(!bCheckedFirstRun)
    {
        bCheckedFirstRun=true;
        if(auto* Radio=GetRadio();Radio&&(!Radio->HasSavedProfiles()||FParse::Param(FCommandLine::Get(),TEXT("ImperialSetupRadio")))&&!FParse::Param(FCommandLine::Get(),TEXT("ImperialNoWelcome")))
            ToggleRadioPanel();
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialNoHUD")))return;
    Scale=FMath::Min(Canvas->SizeX/1600.f,Canvas->SizeY/1000.f);
    Origin=FVector2D((Canvas->SizeX-1600*Scale)*.5f,(Canvas->SizeY-1000*Scale)*.5f);
    Pointer=FVector2D(-1000,-1000);float MX=0,MY=0;
    if(PlayerOwner&&PlayerOwner->GetMousePosition(MX,MY))Pointer=FVector2D(MX,MY);
    auto* M=FindMatch();if(!M)return;
    const auto& Drones=M->GetDrones();auto* Selected=M->GetSelectedDrone();
    if(bCompactView&&!IsPilotMenuOpen())
    {
        Panel(470,22,660,76);
        Text(FString::Printf(TEXT("BLUE  %d   :   %d  RED"),M->GetScore(0),M->GetScore(1)),493,38,24,White);
        Text(TimeLabel(M->GetElapsed()),870,41,19,Cyan);
        const auto* CameraController=Cast<AImperialPlayerController>(PlayerOwner);
        Text(CameraController?CameraController->GetCameraRoleName():TEXT("CAMERA"),990,43,13,Muted);
        Text(ImperialBuildInfo::Label(),488,81,9,Muted);
        Panel(24,934,1552,42);
        if(Selected)Text(FString::Printf(TEXT("%s   %s   ALT %.2f m   THR %.0f%%   BAT %.0f%% / %.1f V"),*UnitName(M->GetSelected()),*Selected->GetPilotStatus(),Selected->GetAltitudeMeters(),Selected->GetThrottle()*100,Selected->GetBatteryPercent(),Selected->GetBatteryVoltage()),40,947,13,White);
        Text(TEXT("F10 PANELS  |  C USB"),1548,950,11,Cyan,true);
        if(M->IsTopgunActive())DrawTopgun(*M,true);
        return;
    }
    Panel(24,24,1552,102);Panel(24,142,1172,58);Panel(1220,142,356,452);
    Panel(1220,610,356,316);Panel(24,218,260,190);Panel(24,424,260,123);
    const bool UnsavedAI=M->GetAIResult().Exists()&&!M->GetAIResult().bSaved;
    Button(TEXT("AI_Open"),M->IsAIPracticeActive()?TEXT("AI PRACTICE / SETTINGS & STOP"):(UnsavedAI?TEXT("AI PRACTICE / UNSAVED RESULT"):TEXT("AI PRACTICE / 3-4 DEFENDERS")),730,736,466,44,M->IsAIPracticeActive(),UnsavedAI);
    Panel(24,736,680,44);Panel(24,798,680,128);Panel(24,944,1552,32);
    Rect(24,24,5,102,Cyan);
    Text(TEXT("IMPERIAL WINGS"),45,42,31,White);
    Text(TEXT("DRONE SOCCER  /  GWANGJU TRAINING CENTRE"),47,88,12,Cyan);
    Text(Fit(ImperialBuildInfo::Label(),9,440),47,109,9,Muted);
    Rect(500,43,1,64,Edge);
    Text(TEXT("BLUE"),540,43,11,M->GetTeamColor(0));Text(TEXT("RED"),821,43,11,M->GetTeamColor(5));
    Text(FString::Printf(TEXT("SETS  %d"),M->GetSetWins(0)),650,44,10,Muted);
    Text(FString::Printf(TEXT("SETS  %d"),M->GetSetWins(1)),915,44,10,Muted);
    Text(FString::FromInt(M->GetScore(0)),582,61,43,M->GetTeamColor(0));
    Text(TEXT(":"),732,64,35,Muted);Text(FString::FromInt(M->GetScore(1)),864,61,43,M->GetTeamColor(5));
    if(M->IsOffside(0))Text(TEXT("OFFSIDE"),650,85,10,Orange);
    if(M->IsOffside(1))Text(TEXT("OFFSIDE"),915,85,10,Orange);
    Text(Fit(M->GetMatchStatus(),10,415),540,111,10,White);
    Text(TEXT("SESSION / SET CLOCK"),987,43,11,Muted);Text(TimeLabel(M->GetElapsed()),987,66,29,White);
    Text(TEXT("GUIDANCE SOURCE"),1240,43,11,Muted);
    Rect(1242,74,7,7,M->IsConnected()?Green:Amber);
    Text(M->IsConnected()?TEXT("SDACS / APF LINKED"):TEXT("SDACS / OFFLINE"),1259,68,17,M->IsConnected()?Green:Amber);
    const FString Commit=M->GetSourceCommit();
    Text(Commit.IsEmpty()?TEXT("LOCAL PHYSICS READY"):TEXT("SOURCE ")+Commit.Left(10),1241,97,10,Muted);
    Button(TEXT("Exit"),TEXT("EXIT"),1488,91,70,25);

    float X=36;
    auto Action=[this,&X](FName Name,const FString& Label,float W,bool Active=false,bool Warning=false){Button(Name,Label,X,153,W,36,Active,Warning);X+=W+10;};
    Action(TEXT("Auto"),TEXT("AUTO FLEET"),146,M->IsConnected());
    Action(TEXT("Hold"),TEXT("HOLD"),92);Action(TEXT("Return"),TEXT("RETURN"),112);
    Action(TEXT("Manual"),TEXT("MANUAL"),112);Action(TEXT("Reset"),TEXT("RESET"),88);
    Action(TEXT("Export"),TEXT("EXPORT CSV"),116);
    Action(TEXT("Arm"),Selected&&Selected->IsArmed()?TEXT("DISARM"):TEXT("ARM"),100,Selected&&Selected->IsArmed(),true);
    Action(TEXT("RadioOpen"),TEXT("RADIO / USB"),144,bRadioPanelOpen);
    Button(TEXT("StartMatch"),TEXT("START MATCH"),1028,153,116,36);
    Text(Fit(M->GetModeName(),10,202),987,108,10,Cyan);

    Text(TEXT("TRAINING TELEMETRY"),42,237,13,White);Rect(42,263,224,1,Edge);
    Text(TEXT("Aircraft in arena"),42,282,12,Muted);Text(FString::Printf(TEXT("%02d"),Drones.Num()),265,280,15,White,true);
    Text(TEXT("Closest separation"),42,311,12,Muted);Text(FString::Printf(TEXT("%.2f m"),M->GetClosestSeparation()),265,309,15,Cyan,true);
    Text(TEXT("Close pairs < 0.9 m"),42,340,12,Muted);Text(FString::FromInt(M->GetConflicts()),265,338,15,M->GetConflicts()>0?Amber:White,true);
    Text(TEXT("FIDA 2026 / CLASS 40"),42,377,10,White);
    Text(TEXT("16 x 7 x 4.5 m COURT"),42,394,10,Muted);
    const auto* ViewController=Cast<AImperialPlayerController>(PlayerOwner);
    Text(ViewController?ViewController->GetCameraRoleName():TEXT("CAMERA VIEWS"),42,441,12,White);
    Button(TEXT("View1"),TEXT("1  LOS"),37,472,70,33,M->GetView()==1);
    Button(TEXT("View2"),TEXT("2  OBS"),115,472,80,33,M->GetView()==2);
    Button(TEXT("View3"),TEXT("3  ARENA"),203,472,69,33,M->GetView()==3);
    Button(TEXT("View4"),TEXT("4 AUDIENCE"),37,513,113,28,M->GetView()==4);
    Button(TEXT("View5"),TEXT("5 REFEREE"),158,513,114,28,M->GetView()==5);
    if(auto* Visuals=GetPresentation())
    {
        Panel(24,564,260,152);Text(TEXT("GRAPHICS"),42,581,12,White);
        const FString Quality=Visuals->GetQualityName();
        Button(TEXT("VisualBalanced"),TEXT("BALANCED"),37,610,84,32,Quality==TEXT("Balanced"));
        Button(TEXT("VisualHigh"),TEXT("HIGH"),129,610,62,32,Quality==TEXT("High"));
        Button(TEXT("VisualCinematic"),TEXT("CINEMA"),199,610,73,32,Quality==TEXT("Cinematic"));
        Text(TEXT("F8 CONNECT SAVED RADIO"),42,657,9,Muted);
        Button(TEXT("FlightRecord"),M->IsRecordingFlight()?TEXT("F6 STOP RECORDING"):TEXT("F6 RECORD FLIGHT"),37,680,235,25,M->IsRecordingFlight());
    }

    Text(TEXT("5 v 5  /  TEAM ROSTER"),1238,159,15,White);
    Text(TEXT("AIRCRAFT"),1254,194,10,Muted);Text(TEXT("CONTROL"),1356,194,10,Muted);Text(TEXT("ALT"),1556,194,10,Muted,true);
    for(int32 I=0;I<Drones.Num()&&I<10;++I)
    {
        auto* D=Drones[I].Get();if(!D)continue;const float Y=216+I*35.f;
        const bool Active=I==M->GetSelected();
        if(Active)Rect(1230,Y,336,33,FLinearColor(.035f,.20f,.25f,.97f));
        if(Active)Rect(1230,Y,2,33,M->GetTeamColor(I));
        Rect(1241,Y+13,6,6,M->GetTeamColor(I));
        Text(UnitName(I),1255,Y+7,14,Active?White:Muted);
        Text(D->IsAutonomousEnabled()?TEXT("AUTO"):(D->IsArmed()?TEXT("ANGLE"):TEXT("DISARMED")),1356,Y+8,11,D->IsArmed()?Green:Muted);
        Text(FString::Printf(TEXT("%.1f m"),D->GetAltitudeMeters()),1556,Y+8,12,White,true);
        Rect(1324,Y+10,5,14,D->GetRoleColor());
        ButtonTargets.Add({FName(*FString::Printf(TEXT("Drone_%d"),I)),FBox2D(Screen(1230,Y),Screen(1566,Y+33)),10});
    }
    if(Selected)
    {
        Rect(1220,610,4,44,M->GetTeamColor(M->GetSelected()));
        Text(UnitName(M->GetSelected()),1239,627,23,White);
        Text(ImperialSoccerLayout::RoleName(Selected->GetPlayerRole()),1556,634,10,Selected->GetRoleColor(),true);
        Text(Fit(Selected->GetPilotStatus(),12,190),1240,666,12,Selected->IsArmed()?Green:Amber);
        const auto* Controller=Cast<AImperialPlayerController>(PlayerOwner);
        Button(TEXT("Equipment"),Selected->IsArmed()?TEXT("DISARM FIRST"):(Controller&&Controller->IsEquipmentInspection()?TEXT("F9 BACK TO LOS"):TEXT("F9 EQUIPMENT")),1435,650,121,28,Controller&&Controller->IsEquipmentInspection());
        Rect(1239,694,317,1,Edge);
        Text(TEXT("FLIGHT SPEED"),1240,710,10,Muted);Text(FString::Printf(TEXT("%.2f m/s"),Selected->GetFlightSpeed()),1240,729,24,White);
        Text(TEXT("ALTITUDE"),1411,710,10,Muted);Text(FString::Printf(TEXT("%.2f m"),Selected->GetAltitudeMeters()),1411,729,24,White);
        Text(TEXT("THROTTLE"),1240,775,10,Muted);Text(FString::Printf(TEXT("%.0f %%"),Selected->GetThrottle()*100),1240,794,24,Cyan);
        Text(TEXT("MOTORS"),1411,775,10,Muted);Text(Selected->IsArmed()?TEXT("ARMED"):TEXT("SAFE"),1411,797,18,Selected->IsArmed()?Green:Muted);
        Rect(1240,832,315,3,Edge);Rect(1240,832,315*FMath::Clamp(Selected->GetThrottle(),0.f,1.f),3,M->GetTeamColor(M->GetSelected()));
        Button(TEXT("ArmSelected"),Selected->IsArmed()?TEXT("DISARM SELECTED"):TEXT("ARM SELECTED"),1239,854,316,34,Selected->IsArmed(),true);
        Text(FString::Printf(TEXT("BAT %.0f%% %.1fV   GUARD %.0f%%"),Selected->GetBatteryPercent(),Selected->GetBatteryVoltage(),Selected->GetGuardIntegrityPercent()),1240,898,10,Selected->GetGuardIntegrityPercent()<30?Amber:Muted);
        if(Selected->GetBatteryPercent()<=0)Text(TEXT("BATTERY EMPTY / NO MOTOR POWER"),1240,912,10,Amber);
        else if(Selected->IsBatteryPowerLimited())Text(FString::Printf(TEXT("POWER LIMITED / THRUST %.0f%%"),Selected->GetMotorThrustScale()*100),1240,912,10,Amber);
    }

    Rect(24,736,3,44,Cyan);Text(Fit(M->GetNotice(),12,636),42,752,12,White);
    Text(TEXT("SESSION EVENTS"),42,814,12,White);Rect(42,839,644,1,Edge);
    const auto& Events=M->GetEvents();int32 Row=0;
    for(int32 I=Events.Num()-1;I>=0&&Row<3;--I,++Row)Text(Fit(Events[I],12,640),43,851+Row*23,12,Row==0?White:Muted);
    Text(TEXT("SPACE ARM   ARROWS THROTTLE   WASD PITCH / ROLL   QE YAW"),42,956,10,Cyan);
    const auto& Timing=M->GetSimulationTime();
    if(Timing.LimitedFrames>0)Text(FString::Printf(TEXT("PHYSICS LIMITED: %llu frames | %.3f s omitted | match clock continues"),Timing.LimitedFrames,Timing.DroppedSeconds),42,928,11,Cyan);
    Text(TEXT("TAB SELECT   1 LOS / 2 OBSERVER / 3 ARENA"),583,956,10,Muted);
    Text(TEXT("ENTER AUTO   H HOLD   R RETURN   C RADIO"),1109,956,10,Muted);
    Text(FString::Printf(TEXT("%.0f FPS"),M->GetFPS()),1557,956,10,Muted,true);

    // A simple selected-aircraft reticle uses the actual Chaos body location.
    if(Selected&&M->GetView()==3&&PlayerOwner)
    {
        FVector2D P;
        if(PlayerOwner->ProjectWorldLocationToScreen(Selected->GetActorLocation()+FVector(0,0,35),P)&&!IsPointerOverUI(P))
        {
            const auto C=M->GetTeamColor(M->GetSelected());const float R=15*Scale;
            DrawLine(P.X-R,P.Y-R,P.X-R*.4f,P.Y-R,C,1.5f*Scale);DrawLine(P.X-R,P.Y-R,P.X-R,P.Y-R*.4f,C,1.5f*Scale);
            DrawLine(P.X+R,P.Y+R,P.X+R*.4f,P.Y+R,C,1.5f*Scale);DrawLine(P.X+R,P.Y+R,P.X+R,P.Y+R*.4f,C,1.5f*Scale);
            const auto U=(P-Origin)/Scale;Text(UnitName(M->GetSelected()),U.X+22,U.Y-8,12,C);
        }
    }
    DrawTopgun(*M);
    if(bRadioPanelOpen)DrawRadioPanel(*M);
    if(bAIPracticePanel)DrawAIPracticePanel(*M);
}

void AImperialHUD::DrawRadioPanel(AImperialMatchManager& Manager)
{
    DrawRect(FLinearColor(.006f,.012f,.020f,.82f),0,0,Canvas->SizeX,Canvas->SizeY);
    AddHitBox(FVector2D::ZeroVector,FVector2D(Canvas->SizeX,Canvas->SizeY),TEXT("Radio_Backdrop"),true,100);
    Panel(320,126,960,748);Rect(320,126,4,748,Cyan);ButtonPriority=110;
    Text(TEXT("RADIO / USB CONTROLLER"),348,151,25,White);
    Button(TEXT("Radio_Close"),TEXT("CLOSE"),1172,148,80,32);
    UImperialRadioSubsystem* Radio=GetRadio();
    if(!Radio){Text(TEXT("USB controller service is unavailable."),348,212,16,Amber);ButtonPriority=10;return;}
    if(bResponsePanel){DrawResponsePanel(*Radio);ButtonPriority=10;return;}
    const FImperialRadioDevice* Device=Radio->GetSelectedDevice();
    const bool Connected=Radio->IsConnected();
    const bool Guided=Radio->IsGuidedActive();
    const bool Calibrated=Radio->IsCalibrated();
    const bool Enabled=Radio->IsEnabled();
    const auto StickMode=Radio->GetStickMode();
    const int32 ModeNumber=static_cast<int32>(StickMode);
    const int32 ThrottleStick=ImperialRadio::StickForChannel(StickMode,3);
    const TCHAR* ThrottleSide=ThrottleStick==0?TEXT("LEFT"):TEXT("RIGHT");
    if(!Guided)Button(TEXT("Radio_Advanced"),bRadioAdvanced?TEXT("SIMPLE VIEW"):TEXT("ADVANCED OPTIONS"),985,148,170,32,bRadioAdvanced);
    FString NextStep;
    if(Guided)NextStep=Radio->GetGuidedPhase()==ImperialRadio::GuidedPhase::Review
        ?TEXT("STEP 3 / 4  REVIEW THE MEASURED AXES, THEN CLICK CONFIRM & SAVE")
        :TEXT("STEP 2 / 4  FOLLOW THE STICK GUIDE BELOW; ENTER CONFIRMS EACH HELD END");
    else if(!Device||!Connected)NextStep=TEXT("STEP 1 / 4  SELECT YOUR CONNECTED RADIO BELOW; NEVER GUESS THE AXES");
    else if(!Calibrated)NextStep=TEXT("STEP 2 / 4  CHOOSE MODE 1 OR MODE 2, THEN STICK SETUP; FOLLOW EACH STICK GUIDE");
    else if(!Enabled)NextStep=TEXT("STEP 4 / 4  LOWER THROTTLE, THEN CLICK ENABLE RADIO; MOTORS STAY DISARMED");
    else NextStep=TEXT("READY  CLOSE THIS PANEL, THEN SPACE ARMS AT LOW THROTTLE");
    Text(NextStep,348,181,11,Enabled?Green:Cyan);
    Rect(350,205,7,7,Connected?Green:Amber);
    Text(Connected?TEXT("CONNECTED"):TEXT("DISCONNECTED"),369,199,13,Connected?Green:Amber);
    const FString DevicePrompt=Radio->GetDevices().IsEmpty()?TEXT("No joystick: connect USB DATA and select USB Joystick on the radio.")
        :FString::Printf(TEXT("%d device(s) found. Choose your radio below, or use NEXT DEVICE."),Radio->GetDevices().Num());
    Text(Fit(Device?Device->Name:DevicePrompt,16,650),348,224,16,White);
    Text(Device?FString::Printf(TEXT("%d AXES  /  %d BUTTONS  /  DRIVER ID %04X:%04X"),Device->AxisCount,Device->ButtonCount,Device->ManufacturerId,Device->ProductId):TEXT("The device name shown here comes directly from Windows."),348,249,10,Muted);
    Button(TEXT("Radio_Refresh"),TEXT("REFRESH"),1032,205,106,34);
    Button(TEXT("Radio_Device"),TEXT("NEXT DEVICE"),1148,205,104,34);
    Rect(348,276,904,1,Edge);
    const bool ShowDevicePicker=!Device&&!Guided&&!bRadioAdvanced;
    Text(ShowDevicePicker?TEXT("CHOOSE YOUR USB CONTROLLER"):TEXT("LIVE STICK AXES"),348,292,12,White);
    Text(TEXT("CHANNEL ASSIGNMENT"),830,292,12,White);
    const TArray<float>& Axes=Radio->GetRawAxes();
    const TCHAR* AxisNames[]={TEXT("X"),TEXT("Y"),TEXT("Z"),TEXT("R"),TEXT("U"),TEXT("V")};
    if(ShowDevicePicker)
    {
        const auto& Devices=Radio->GetDevices();
        if(Devices.IsEmpty())
        {
            Text(TEXT("1. Power on the radio and use a USB DATA cable."),348,329,12,White);
            Text(TEXT("2. Select USB JOYSTICK on the radio screen."),348,354,12,White);
            Text(TEXT("3. Click REFRESH when Windows detects it."),348,379,12,Cyan);
            Text(TEXT("Windows test: Win+R, then joy.cpl"),348,425,11,Muted);
        }
        for(int32 I=0;I<FMath::Min(3,Devices.Num());++I)
        {
            const auto& Choice=Devices[I];const float Y=317+I*57.f;
            Button(FName(*FString::Printf(TEXT("Radio_Select_%d"),Choice.Id)),TEXT(""),348,Y,445,50);
            Text(Fit(Choice.Name,12,416),360,Y+8,12,White);
            Text(FString::Printf(TEXT("SLOT %d  /  %04X:%04X  /  %d AXES  /  %d BUTTONS"),Choice.Id,Choice.ManufacturerId,Choice.ProductId,Choice.AxisCount,Choice.ButtonCount),360,Y+29,10,Cyan);
        }
        if(Devices.Num()>3)Text(TEXT("More devices: use NEXT DEVICE above."),348,495,10,Muted);
    }
    else for(int32 I=0;I<6;++I)
    {
        const float Y=322+I*33.f;
        const bool Available=Connected&&Device&&Device->AxisAvailable.IsValidIndex(I)&&Device->AxisAvailable[I]&&Axes.IsValidIndex(I);
        Text(AxisNames[I],349,Y,13,Available?White:Muted);
        Rect(379,Y+3,340,11,FLinearColor(.05f,.09f,.12f,1));
        Rect(549,Y+1,1,15,Edge);
        if(Available)
        {
            const float Raw=FMath::Clamp(Axes[I],0.f,1.f);
            Rect(379,Y+4,340*Raw,9,Cyan);
            Text(FString::Printf(TEXT("%.3f"),Raw),792,Y,12,White,true);
            if(Radio->GetCaptureSampleCount()>0)
            {
                const float Lo=FMath::Clamp(Radio->GetCapturedMinimum(I),0.f,1.f);
                const float Hi=FMath::Clamp(Radio->GetCapturedMaximum(I),0.f,1.f);
                Rect(379+Lo*340,Y+19,FMath::Max(0.f,Hi-Lo)*340,2,Green);
            }
        }
        else Text(TEXT("--"),792,Y,12,Muted,true);
    }
    const TCHAR* Channels[]={TEXT("ROLL"),TEXT("PITCH"),TEXT("YAW"),TEXT("THROTTLE")};
    const float Values[]={Radio->GetNormalizedRoll(),Radio->GetNormalizedPitch(),Radio->GetNormalizedYaw(),Radio->GetThrottle()};
    TSet<int32> UsedAxes;
    for(int32 I=0;I<4;++I)
    {
        const float Y=321+I*44.f;const int32 Axis=Radio->GetMappedAxis(I);UsedAxes.Add(Axis);
        Text(Channels[I],830,Y+10,11,White);
        if(Guided||!bRadioAdvanced)
        {
            Text(Axis>=0&&Axis<6?FString::Printf(TEXT("%s  /  %s"),AxisNames[Axis],Radio->IsInverted(I)?TEXT("INVERTED"):TEXT("NORMAL")):TEXT("WAITING FOR STICK"),927,Y+10,11,Axis>=0?Cyan:Muted);
        }
        else
        {
            Button(FName(*FString::Printf(TEXT("Radio_Axis_%d"),I)),FString::Printf(TEXT("AXIS %s"),Axis>=0&&Axis<6?AxisNames[Axis]:TEXT("?")),919,Y,99,32);
            Button(FName(*FString::Printf(TEXT("Radio_Invert_%d"),I)),Radio->IsInverted(I)?TEXT("INVERTED"):TEXT("NORMAL"),1028,Y,105,32,Radio->IsInverted(I));
        }
        const float V=FMath::Clamp(Values[I],I==3?0.f:-1.f,1.f);
        Text(FString::Printf(TEXT("%+.2f"),V),1251,Y+8,12,Radio->IsCalibrated()?Green:Muted,true);
        Rect(1150,Y+28,101,2,Edge);
        if(Radio->IsCalibrated())Rect(1150,Y+28,101*(I==3?V:(V+1)*.5f),2,Green);
    }
    const bool FourAxes=UsedAxes.Num()==4&&!UsedAxes.Contains(INDEX_NONE);
    Text(FourAxes?TEXT("FOUR DISTINCT AXES ASSIGNED"):(bRadioAdvanced?TEXT("ASSIGN FOUR DIFFERENT AXES"):TEXT("STICK SETUP FINDS THE FOUR AXES")),830,506,10,FourAxes?Muted:Amber);
    if(Guided)
    {
        Button(TEXT("Radio_GuidedCancel"),TEXT("CANCEL SETUP"),1090,522,162,28);
        Rect(348,562,904,1,Edge);
        const auto Phase=Radio->GetGuidedPhase();
        const int32 Channel=FMath::Clamp(Radio->GetGuidedChannel(),0,3);
        const int32 ActiveStick=ImperialRadio::StickForChannel(StickMode,Channel);
        const TCHAR* ActiveSide=ActiveStick==0?TEXT("LEFT"):TEXT("RIGHT");
        const TCHAR* Travel=Channel==1?TEXT("FORWARD / BACK"):(Channel==3?TEXT("UP / DOWN"):TEXT("LEFT / RIGHT"));
        const TCHAR* Endpoint=Channel==1?TEXT("FORWARD"):(Channel==3?TEXT("DOWN (minimum)"):TEXT("RIGHT"));
        const FString Title=FString::Printf(TEXT("%d / 5  %s - %s STICK %s"),Channel+1,Channels[Channel],ActiveSide,Travel);
        Text(Phase==ImperialRadio::GuidedPhase::Neutral?FString(TEXT("5 / 5  NEUTRAL AND MINIMUM THROTTLE")):(Phase==ImperialRadio::GuidedPhase::Review?FString::Printf(TEXT("REVIEW MODE %d ASSIGNMENTS"),ModeNumber):Title),348,580,16,White);
        Text(FString::Printf(TEXT("MODE %d  /  %s THROTTLE"),ModeNumber,ThrottleSide),348,531,12,Cyan);
        if(Phase==ImperialRadio::GuidedPhase::Ready)
        {
            Text(FString::Printf(TEXT("Center roll / pitch / yaw. Hold %s throttle DOWN before starting."),ThrottleSide),348,609,12,Muted);
            Text(TEXT("Press START THIS STEP or ENTER when ready."),348,631,12,Cyan);
            Button(TEXT("Radio_GuidedStart"),TEXT("START THIS STEP"),931,667,285,38);
        }
        else if(Phase==ImperialRadio::GuidedPhase::Capturing)
        {
            Text(TEXT("Move only this direction through its full travel. Hold EACH end for 1 second."),348,609,12,Muted);
            Text(FString::Printf(TEXT("Finish by holding the %s stick fully %s."),ActiveSide,Endpoint),348,631,12,Cyan);
            Button(TEXT("Radio_GuidedStep"),TEXT("CONFIRM HELD END / ENTER"),897,666,319,38);
            Button(TEXT("Radio_GuidedStart"),TEXT("RETRY THIS STEP"),997,715,219,32);
        }
        else if(Phase==ImperialRadio::GuidedPhase::Neutral)
        {
            Text(FString::Printf(TEXT("Release roll / pitch / yaw to their centers. Hold %s throttle fully DOWN."),ThrottleSide),348,609,12,Muted);
            Text(TEXT("Keep still for 1 second, then CAPTURE NEUTRAL or ENTER."),348,631,12,Cyan);
            Button(TEXT("Radio_GuidedNeutral"),TEXT("CAPTURE NEUTRAL"),931,667,285,38);
        }
        else
        {
            Text(TEXT("The measured axes and directions appear above. Keep throttle fully down."),348,609,12,Muted);
            Text(TEXT("CONFIRM & SAVE stores this device profile. ENABLE RADIO follows separately."),348,631,12,Cyan);
            Button(TEXT("Radio_GuidedSave"),TEXT("CONFIRM & SAVE"),931,667,285,38);
        }
        // The accent follows the chosen physical layout, not the driver's axis order.
        for(int32 Stick=0;Stick<2;++Stick)
        {
            const float CX=437+Stick*176.f,CY=704;
            const bool Active=Phase==ImperialRadio::GuidedPhase::Neutral||(Phase!=ImperialRadio::GuidedPhase::Review&&Stick==ActiveStick);
            const auto Color=Active?Cyan:Muted;
            Rect(CX-38,CY-38,76,76,Edge);Rect(CX-37,CY-37,74,74,Navy);
            Rect(CX-31,CY,62,1,Color);Rect(CX,CY-31,1,62,Color);
            float PX=CX,PY=CY+(Stick==ThrottleStick?28.f:0.f);
            if(Active&&Phase==ImperialRadio::GuidedPhase::Capturing){if(Channel==0||Channel==2)PX+=28;else PY=CY+(Channel==3?28.f:-28.f);}
            Rect(PX-4,PY-4,8,8,Color);
            Text(Stick==0?(ThrottleStick==0?TEXT("LEFT / THROTTLE + YAW"):TEXT("LEFT / PITCH + YAW")):(ThrottleStick==1?TEXT("RIGHT / THROTTLE + ROLL"):TEXT("RIGHT / PITCH + ROLL")),CX-70,CY+44,10,Color);
        }
        Text(FString::Printf(TEXT("MODE %d  /  %d CURRENT-STEP SAMPLES"),ModeNumber,Radio->GetCaptureSampleCount()),741,738,9,Muted);
        Rect(348,778,904,49,FLinearColor(.035f,.065f,.09f,1));
        Text(Fit(Radio->GetStatus(),12,871),364,796,12,Amber);
        Text(TEXT("CLOSE cancels unfinished setup. Previous saved calibration is retained until final save."),348,844,11,Muted);
        ButtonPriority=10;return;
    }
    if(!Radio->IsCalibrating())
    {
        Button(TEXT("Radio_Mode1"),TEXT("MODE 1"),348,522,112,28,StickMode==ImperialRadio::StickMode::Mode1);
        Button(TEXT("Radio_Mode2"),TEXT("MODE 2"),472,522,112,28,StickMode==ImperialRadio::StickMode::Mode2);
    }
    else Text(FString::Printf(TEXT("MODE %d / FINISH RANGE SETUP FIRST"),ModeNumber),348,531,10,Muted);
    Button(TEXT("Radio_GuidedBegin"),FString::Printf(TEXT("STICK SETUP / MODE %d"),ModeNumber),596,522,220,28,!Calibrated);
    Text(FString::Printf(TEXT("DEADZONE  %.0f %%"),Radio->GetDeadzone()*100),830,532,11,Muted);
    if(bRadioAdvanced)
    {
        Button(TEXT("Radio_DeadzoneDown"),TEXT("-"),1104,522,44,28);
        Button(TEXT("Radio_DeadzoneUp"),TEXT("+"),1158,522,44,28);
    }
    Rect(348,562,904,1,Edge);
    if(bRadioAdvanced)
    {
        Text(TEXT("MANUAL AXIS AND RANGE SETUP"),348,580,12,White);
        Text(TEXT("1  START, then move all four sticks through their full ranges. CAPTURE RANGE when finished."),348,605,12,Muted);
        Text(TEXT("2  Center roll / pitch / yaw and move throttle fully down. CAPTURE NEUTRAL, then FINISH & SAVE."),348,627,12,Muted);
        Button(TEXT("Radio_Begin"),TEXT("1  START RANGE"),348,658,180,36,Radio->IsCalibrating());
        Button(TEXT("Radio_Range"),TEXT("2  CAPTURE RANGE"),540,658,180,36);
        Button(TEXT("Radio_Neutral"),TEXT("3  CAPTURE NEUTRAL"),732,658,210,36);
        Button(TEXT("Radio_Finish"),TEXT("4  FINISH & SAVE"),954,658,190,36,Calibrated);
    }
    else
    {
        Text(FString::Printf(TEXT("MODE %d: LEFT %s + YAW  /  RIGHT %s + ROLL"),ModeNumber,ThrottleStick==0?TEXT("THROTTLE"):TEXT("PITCH"),ThrottleStick==1?TEXT("THROTTLE"):TEXT("PITCH")),348,580,12,White);
        Text(Calibrated?TEXT("YOUR CONTROLLER IS CALIBRATED. Lower throttle, then ENABLE RADIO."):TEXT("Choose your transmitter's physical layout, then STICK SETUP > REVIEW / SAVE > ENABLE."),348,605,12,Calibrated?Green:Cyan);
        Text(TEXT("A layout change needs fresh setup. To reuse saved settings, keep the saved mode."),348,627,12,Muted);
        Button(TEXT("Radio_ConnectSaved"),TEXT("CONNECT SAVED RADIO"),348,658,335,36,Radio->HasSavedProfiles()&&!Enabled);
        Text(TEXT("Low throttle required. This does not arm the aircraft."),706,669,11,Cyan);
    }
    if(!Radio->IsCalibrating())
        Button(TEXT("Radio_PrecisionThrottle"),Radio->IsPrecisionThrottleEnabled()?TEXT("THROTTLE: PRECISION"):TEXT("THROTTLE: DIRECT"),348,704,245,28,Radio->IsPrecisionThrottleEnabled());
    else
        Text(TEXT("THROTTLE: LOCKED DURING SETUP"),348,712,10,Muted);
    if(!Radio->IsCalibrating())Button(TEXT("Radio_Response"),TEXT("RATES / EXPO / ARM BUTTON"),850,704,402,28);
    Rect(348,740,904,32,FLinearColor(.035f,.065f,.09f,1));Rect(348,740,3,32,Radio->IsCalibrated()?Green:Amber);
    Text(Fit(Radio->GetStatus(),12,865),364,750,12,Radio->IsCalibrated()?Green:Amber);
    Button(TEXT("Radio_Keyboard"),TEXT("USE KEYBOARD"),348,783,164,36,!Radio->IsEnabled());
    Button(TEXT("Radio_Enable"),TEXT("ENABLE RADIO"),524,783,183,36,Radio->IsEnabled());
    if(bRadioAdvanced)
    {
        Button(TEXT("Radio_Save"),TEXT("SAVE PROFILE"),719,783,144,36);
        Button(TEXT("Radio_Load"),TEXT("LOAD PROFILE"),875,783,144,36);
    }
    Text(Radio->IsEnabled()?TEXT("USB RADIO ACTIVE"):TEXT("KEYBOARD ACTIVE"),1252,797,11,Radio->IsEnabled()?Green:Muted,true);
    Text(Fit(TEXT("First use: SELECT DEVICE > CHOOSE MODE > STICK SETUP > SAVE > ENABLE RADIO."),10,670),348,844,10,Muted);
    Button(TEXT("Radio_Report"),TEXT("EXPORT DEVICE REPORT"),1032,835,220,28);
    ButtonPriority=10;
}

void AImperialHUD::BeginResponsePanel()
{
    auto* R=GetRadio();if(!R||R->IsCalibrating())return;
    DraftResponse=R->GetPilotResponse();DraftArmButton=R->GetArmButton();DraftArmDevice=R->GetArmDevice();
    bDraftArmToggle=R->IsArmToggle();bResponsePanel=true;bLearnArm=false;ResponseMessage=TEXT("Changes are a preview until SAVE. CANCEL keeps your saved settings.");
}

void AImperialHUD::DrawResponsePanel(UImperialRadioSubsystem& R)
{
    Text(TEXT("CONTROL RESPONSE / ANGLE"),348,198,18,Cyan);
    Text(TEXT("Roll/Pitch rate limits tilt response; full stick still targets 35 deg. Yaw rate is deg/s."),348,226,12,Muted);
    Text(TEXT("Expo softens the centre; minimum/maximum stick positions stay unchanged."),348,248,12,Muted);
    const TCHAR* Names[]={TEXT("ROLL"),TEXT("PITCH"),TEXT("YAW")};
    for(int I=0;I<3;++I)
    {
        const float Y=288+I*92;Text(Names[I],348,Y+15,15,White);
        Text(FString::Printf(TEXT("RATE %.0f deg/s"),DraftResponse.RateDegrees[I]),450,Y,12,White);
        Button(*FString::Printf(TEXT("Response_RateDown_%d"),I),TEXT("-"),450,Y+25,42,28);
        Button(*FString::Printf(TEXT("Response_RateUp_%d"),I),TEXT("+"),502,Y+25,42,28);
        Text(FString::Printf(TEXT("EXPO %.0f%%"),DraftResponse.Expo[I]*100),592,Y,12,White);
        Button(*FString::Printf(TEXT("Response_ExpoDown_%d"),I),TEXT("-"),592,Y+25,42,28);
        Button(*FString::Printf(TEXT("Response_ExpoUp_%d"),I),TEXT("+"),644,Y+25,42,28);
        const float Left=750,Top=Y-3,Width=230,Height=65;
        Rect(Left,Top,Width,Height,FLinearColor(.015,.025,.04,1));
        Rect(Left,Top+Height/2,Width,1,Edge);Rect(Left+Width/2,Top,1,Height,Edge);
        for(int P=0;P<=100;++P)
        {const double Input=P/50.-1;Rect(Left+P*Width/100.,Top+Height*(1-DraftResponse.Shape(I,Input))*.5,2,2,Cyan);}
        const float Raw=I==0?R.GetNormalizedRoll():I==1?R.GetNormalizedPitch():R.GetNormalizedYaw();
        Text(FString::Printf(TEXT("IN %+.2f  OUT %+.2f"),Raw,DraftResponse.Shape(I,Raw)),1002,Y+17,11,Muted);
    }
    Rect(348,570,904,1,Edge);
    const auto* Device=R.GetSelectedDevice();
    if(bLearnArm)
    {
        if(!R.IsConnected()||!Device||Device->Identity!=ArmLearningDevice){bLearnArm=false;ResponseMessage=TEXT("Controller disconnected or changed. Previous binding kept.");}
        else
        {
            const int32 Index=ArmLearner.Sample(R.GetButtons());
            if(Index>=0&&Index<FMath::Min(32,Device->ButtonCount))
            {DraftArmButton=Index;DraftArmDevice=Device->Identity;bLearnArm=false;ResponseMessage=TEXT("Button learned. Choose TOGGLE or SWITCH, then SAVE. Motors stay disarmed.");}
        }
    }
    const bool MatchDevice=Device&&Device->Identity==DraftArmDevice;
    Text(DraftArmButton<0?TEXT("ARM BUTTON: NOT ASSIGNED"):FString::Printf(TEXT("ARM BUTTON: %d  /  %s"),DraftArmButton+1,MatchDevice?TEXT("SELECTED CONTROLLER"):TEXT("SAVED OTHER CONTROLLER")),348,594,14,White);
    Button(TEXT("Response_Learn"),bLearnArm?TEXT("CANCEL LEARNING"):TEXT("LEARN ARM BUTTON"),348,625,245,34,bLearnArm);
    Button(TEXT("Response_Clear"),TEXT("REMOVE BINDING"),605,625,205,34);
    Button(TEXT("Response_ArmMode"),bDraftArmToggle?TEXT("TOGGLE BUTTON"):TEXT("2-POSITION SWITCH"),822,625,260,34);
    Text(bLearnArm?TEXT("Release the ARM button / switch OFF, then press it ON. Keep other switches unchanged."):
        (bDraftArmToggle?TEXT("Each press toggles ARM/DISARM. ARM needs low throttle and a fresh press."):TEXT("Switch ON arms at low throttle. Switch OFF disarms. OFF required after reconnect.")),348,677,12,Cyan);
    Text(Fit(ResponseMessage,12,895),348,718,12,Muted);
    Button(TEXT("Response_Defaults"),TEXT("RESET RATES / EXPO"),348,783,248,36);
    Button(TEXT("Response_Save"),TEXT("SAVE & RETURN"),730,783,260,36);
    Button(TEXT("Response_Cancel"),TEXT("CANCEL"),1002,783,250,36);
    Text(TEXT("30-360 deg/s / Expo 0-80%. USB is disabled after save; enable again at low throttle."),348,844,11,Muted);
}

void AImperialHUD::NotifyHitBoxClick(FName BoxName)
{
    Super::NotifyHitBoxClick(BoxName);auto* M=FindMatch();if(!M)return;const FString N=BoxName.ToString();
    if(const auto* ReadOnlyController=Cast<AImperialPlayerController>(PlayerOwner);ReadOnlyController&&ReadOnlyController->IsReadOnlyView()&&
       (N==TEXT("Auto")||N==TEXT("Hold")||N==TEXT("Return")||N==TEXT("Manual")||N==TEXT("Reset")||N==TEXT("StartMatch")||N==TEXT("Arm")||N==TEXT("ArmSelected")))return;
    UE_LOG(LogTemp,Display,TEXT("IMPERIAL_UI_CLICK %s"),*N);
    if(N==TEXT("Exit")){M->PrepareToExit();UKismetSystemLibrary::QuitGame(this,PlayerOwner,EQuitPreference::Quit,false);return;}
    if(bAIPracticePanel)
    {
        if(N==TEXT("AI_Close")){ToggleAIPracticePanel();return;}
        if(N==TEXT("AI_Last")){bAIResultPage=M->GetAIResult().Exists();return;}
        if(bAIResultPage)
        {
            if(N==TEXT("AI_ResultBack"))bAIResultPage=false;
            else if(N==TEXT("AI_ResultSave"))M->SaveAIResult();
            else if(N==TEXT("AI_ResultDiscard")){M->DiscardUnsavedAIResult();bAIResultPage=false;}
            return;
        }
        if(N==TEXT("AI_Stop")){M->StopAIPractice();return;}
        if(!M->IsAIPracticeActive())
        {
            if(N==TEXT("AI_Defence"))bAIDraftDuel=false;
            else if(N==TEXT("AI_Duel"))bAIDraftDuel=true;
            else if(N==TEXT("AI_Count3"))AIDraftCount=3;
            else if(N==TEXT("AI_Count4"))AIDraftCount=4;
            else if(N==TEXT("AI_Easy"))AIDraftDifficulty=0;
            else if(N==TEXT("AI_Medium"))AIDraftDifficulty=1;
            else if(N==TEXT("AI_Hard"))AIDraftDifficulty=2;
            else if(N==TEXT("AI_Seed")){AIDraftSeed=FMath::RandHelper(1000000)+1;}
            else if(N==TEXT("AI_Start")){if(M->StartAIPractice({AIDraftCount,AIDraftDifficulty,AIDraftSeed,bAIDraftDuel}))ToggleAIPracticePanel();}
        }
        return;
    }
    if(N==TEXT("TopgunStart")&&!IsPilotMenuOpen()){M->StartTopgun();return;}
    if(N==TEXT("TopgunExit")&&!IsPilotMenuOpen()){M->ExitTopgun();return;}
    if(N==TEXT("AI_Open")&&!bRadioPanelOpen){ToggleAIPracticePanel();return;}
    if(N==TEXT("Equipment")){if(auto* PC=Cast<AImperialPlayerController>(PlayerOwner))PC->ToggleEquipmentInspection();return;}
    if(N==TEXT("RadioOpen")){ToggleRadioPanel();return;}
    if(bRadioPanelOpen)
    {
        if(N==TEXT("Radio_Close")){ToggleRadioPanel();return;}
        auto* R=GetRadio();if(!R)return;
        if(N==TEXT("Radio_Response")){BeginResponsePanel();return;}
        if(bResponsePanel)
        {
            if(N==TEXT("Response_Cancel")){bResponsePanel=false;bLearnArm=false;return;}
            if(N==TEXT("Response_Save"))
            {
                if(R->SavePilotResponse(DraftResponse,DraftArmButton,DraftArmDevice,bDraftArmToggle))
                {bResponsePanel=false;bLearnArm=false;if(auto* PC=Cast<AImperialPlayerController>(PlayerOwner))PC->FlushPilotContextKeys();}
                else ResponseMessage=R->GetStatus();return;
            }
            if(N==TEXT("Response_Defaults"))DraftResponse={};
            else if(N==TEXT("Response_Learn"))
            {
                if(bLearnArm)bLearnArm=false;
                else if(const auto* Device=R->GetSelectedDevice();Device&&R->IsConnected())
                {bLearnArm=true;ArmLearningDevice=Device->Identity;ArmLearner.Begin(R->GetButtons());ResponseMessage=TEXT("Move only the intended ARM button: OFF / release, then ON / press.");}
                else ResponseMessage=TEXT("Connect and select a controller before learning an ARM button.");
            }
            else if(N==TEXT("Response_Clear")){DraftArmButton=INDEX_NONE;DraftArmDevice.Empty();bLearnArm=false;}
            else if(N==TEXT("Response_ArmMode"))bDraftArmToggle=!bDraftArmToggle;
            else if(N.StartsWith(TEXT("Response_Rate"))||N.StartsWith(TEXT("Response_Expo")))
            {
                const int32 Axis=FCString::Atoi(*N.Right(1));if(Axis<0||Axis>2)return;
                const double Direction=N.Contains(TEXT("Up"))?1:-1;
                if(N.StartsWith(TEXT("Response_Rate")))DraftResponse.RateDegrees[Axis]=FMath::Clamp(DraftResponse.RateDegrees[Axis]+Direction*15,30.,360.);
                else DraftResponse.Expo[Axis]=FMath::Clamp(DraftResponse.Expo[Axis]+Direction*.05,0.,.8);
            }
            return;
        }
        if(N==TEXT("Radio_Refresh"))R->RefreshDevices();
        else if(N==TEXT("Radio_Device"))R->CycleDevice();
        else if(N==TEXT("Radio_Advanced")){if(!R->IsGuidedActive())bRadioAdvanced=!bRadioAdvanced;}
        else if(N==TEXT("Radio_PrecisionThrottle"))
        {
            // Check the fleet's ARM state before any input flush could disarm it.
            if(R->SetPrecisionThrottleEnabled(!R->IsPrecisionThrottleEnabled()))
                if(auto* Pilot=Cast<AImperialPlayerController>(PlayerOwner))Pilot->FlushPilotContextKeys();
        }
        else if(N==TEXT("Radio_Mode1")||N==TEXT("Radio_Mode2"))
        {
            if(!R->IsCalibrating())
            {
                M->ManualSelected();
                if(auto* Pilot=Cast<AImperialPlayerController>(PlayerOwner))Pilot->FlushPilotContextKeys();
                R->SetStickMode(N==TEXT("Radio_Mode1")?ImperialRadio::StickMode::Mode1:ImperialRadio::StickMode::Mode2);
            }
        }
        else if(N==TEXT("Radio_ConnectSaved")){M->ManualSelected();R->EnableSavedRadio();}
        else if(N.StartsWith(TEXT("Radio_Select_"))){M->ManualSelected();R->SelectDevice(FCString::Atoi(*N.RightChop(13)));}
        else if(N.StartsWith(TEXT("Radio_Axis_")))R->CycleAxis(FCString::Atoi(*N.RightChop(11)));
        else if(N.StartsWith(TEXT("Radio_Invert_")))R->ToggleInvert(FCString::Atoi(*N.RightChop(13)));
        else if(N==TEXT("Radio_DeadzoneDown"))R->SetDeadzone(R->GetDeadzone()-.01f);
        else if(N==TEXT("Radio_DeadzoneUp"))R->SetDeadzone(R->GetDeadzone()+.01f);
        else if(N==TEXT("Radio_Begin")){M->ManualSelected();R->BeginCalibration();}
        else if(N==TEXT("Radio_GuidedBegin")){M->ManualSelected();R->BeginGuidedCalibration();}
        else if(N==TEXT("Radio_GuidedStart"))R->StartGuidedStep();
        else if(N==TEXT("Radio_GuidedStep"))R->FinishGuidedStep();
        else if(N==TEXT("Radio_GuidedNeutral"))R->FinishGuidedNeutral();
        else if(N==TEXT("Radio_GuidedSave"))R->SaveGuidedCalibration();
        else if(N==TEXT("Radio_GuidedCancel"))R->CancelGuidedCalibration();
        else if(N==TEXT("Radio_Range"))R->FinishRangeCapture();
        else if(N==TEXT("Radio_Neutral"))R->CaptureNeutral();
        else if(N==TEXT("Radio_Finish"))R->FinishCalibration();
        else if(N==TEXT("Radio_Save"))R->SaveProfile();
        else if(N==TEXT("Radio_Load"))R->LoadProfile();
        else if(N==TEXT("Radio_Report"))R->ExportDeviceReport();
        else if(N==TEXT("Radio_Enable")){M->ManualSelected();R->EnableRadio();}
        else if(N==TEXT("Radio_Keyboard")){R->DisableRadio();M->ManualSelected();}
        return;
    }
    if(N.StartsWith(TEXT("Drone_")))M->SelectDrone(FCString::Atoi(*N.RightChop(6)));
    else if(N==TEXT("FlightRecord"))M->ToggleFlightRecording();
    else if(N==TEXT("VisualBalanced")){if(auto* V=GetPresentation())V->SetVisualQuality(TEXT("Balanced"));}
    else if(N==TEXT("VisualHigh")){if(auto* V=GetPresentation())V->SetVisualQuality(TEXT("High"));}
    else if(N==TEXT("VisualCinematic")){if(auto* V=GetPresentation())V->SetVisualQuality(TEXT("Cinematic"));}
    else if(N==TEXT("Auto"))M->LaunchSwarm();else if(N==TEXT("Hold"))M->HoldFleet();
    else if(N==TEXT("Return"))M->ReturnFleet();else if(N==TEXT("Manual"))M->ManualSelected();
    else if(N==TEXT("Reset"))M->ResetMatch();else if(N==TEXT("Export"))M->ExportTelemetry();
    else if(N==TEXT("StartMatch"))M->StartMatch();
    else if(N==TEXT("Arm")||N==TEXT("ArmSelected"))M->ToggleArmSelected();else if(N==TEXT("View1"))M->ChangeView(1);
    else if(N==TEXT("View2"))M->ChangeView(2);else if(N==TEXT("View3"))M->ChangeView(3);else if(N==TEXT("View4"))M->ChangeView(4);else if(N==TEXT("View5"))M->ChangeView(5);
}
