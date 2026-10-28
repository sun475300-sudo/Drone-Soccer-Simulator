#include "ImperialMatchManager.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "ImperialUserData.h"
#include "ImperialHUD.h"
#include "Misc/App.h"
#include "SoccerDronePawn.h"
#include "ImperialPlayerController.h"
#include "ImperialRadioSubsystem.h"
#include "PrototypeArenaBuilder.h"
#include "ImperialSuspendedGoal.h"
#include "Components/SphereComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "ImperialElasticNet.h"
#include "ImperialSoccerLayout.h"
#include "ImperialBuildInfo.h"
#include "ImperialSolverProbe.h"
#include "ImperialFlightTrace.h"
#include "ImperialPresentationEffects.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "HttpModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"

namespace
{
TArray<TSharedPtr<FJsonValue>> JsonVector(FVector V)
{
    return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};
}
bool ReadVector(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,FVector& V)
{
    const TArray<TSharedPtr<FJsonValue>>* A=nullptr;
    if(!O.IsValid()||!O->TryGetArrayField(Key,A)||!A||A->Num()!=3) return false;
    for(const auto& Item:*A)if(!Item.IsValid())return false;
    double X,Y,Z;if(!(*A)[0]->TryGetNumber(X)||!(*A)[1]->TryGetNumber(Y)||!(*A)[2]->TryGetNumber(Z))return false;
    V=FVector(X,Y,Z);return !V.ContainsNaN()&&FMath::IsFinite(X)&&FMath::IsFinite(Y)&&FMath::IsFinite(Z);
}
void AttachVerificationIdentity(FString& Value,float MinDt=0,float MaxDt=0)
{
    TSharedPtr<FJsonObject> Root;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Value),Root)||!Root.IsValid())return;
    Root->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());
    if(MinDt>0&&MaxDt>0){Root->SetNumberField(TEXT("actual_min_game_dt_s"),MinDt);Root->SetNumberField(TEXT("actual_max_game_dt_s"),MaxDt);}
    FString Settings;TSharedPtr<FJsonObject> Defaults;
    if(FFileHelper::LoadFileToString(Settings,*FPaths::Combine(FPaths::ProjectContentDir(),TEXT("Data/physics-defaults.json")))&&
       FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Settings),Defaults)&&Defaults.IsValid())Root->SetObjectField(TEXT("physics_defaults"),Defaults);
    Value.Reset();FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Value));
}
}
AImperialMatchManager::AImperialMatchManager(){PrimaryActorTick.bCanEverTick=true;}
void AImperialMatchManager::BeginPlay()
{
    Super::BeginPlay();
    ImperialBuildInfo::WriteRuntimeIdentity();
    GuidanceSession=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    FString RequestedUrl;
    if(FParse::Value(FCommandLine::Get(),TEXT("ImperialBridgeUrl="),RequestedUrl)&&RequestedUrl.StartsWith(TEXT("http://127.0.0.1:")))GuidanceUrl=RequestedUrl;
    GuidanceUrl.RemoveFromEnd(TEXT("/"));
    bSmoke=FParse::Param(FCommandLine::Get(),TEXT("ImperialPhysicsSmoke"));
    bAngleSmoke=FParse::Param(FCommandLine::Get(),TEXT("ImperialAngleSmoke"));
    bContactSmoke=FParse::Param(FCommandLine::Get(),TEXT("ImperialContactSmoke"));
    bViewSmoke=FParse::Param(FCommandLine::Get(),TEXT("ImperialViewSmoke"));
    bSwarmSmoke=FParse::Param(FCommandLine::Get(),TEXT("ImperialSwarmSmoke"));
    // Shipping ignores the engine's developer-only -FPS override. Configure only
    // these explicit non-USB verification scenarios, then report actual Tick dt.
    double FixtureFPS=0;
    if((bSmoke||bAngleSmoke||bContactSmoke||bViewSmoke||FParse::Param(FCommandLine::Get(),TEXT("ImperialAIRecoverySmoke"))||FParse::Param(FCommandLine::Get(),TEXT("ImperialAIDuelSmoke"))||FParse::Param(FCommandLine::Get(),TEXT("ImperialAIMotionSmoke"))||FParse::Param(FCommandLine::Get(),TEXT("ImperialAIResultsSmoke"))||FParse::Param(FCommandLine::Get(),TEXT("ImperialAIPracticeSmoke"))||FParse::Param(FCommandLine::Get(),TEXT("ImperialPowerSmoke"))||FParse::Param(FCommandLine::Get(),TEXT("ImperialThrottleSmoke")))&&FParse::Param(FCommandLine::Get(),TEXT("UseFixedTimeStep"))&&FParse::Value(FCommandLine::Get(),TEXT("FPS="),FixtureFPS)&&FixtureFPS>=20&&FixtureFPS<=240)
    {FApp::SetFixedDeltaTime(1./FixtureFPS);FApp::SetUseFixedTimeStep(true);}
    bDemo=FParse::Param(FCommandLine::Get(),TEXT("ImperialDemo"));
    FParse::Value(FCommandLine::Get(),TEXT("ImperialCapture="),CaptureFile);
    FParse::Value(FCommandLine::Get(),TEXT("ImperialCaptureTime="),CaptureAt);
    FParse::Value(FCommandLine::Get(),TEXT("ImperialResult="),SmokeResultFile);
    int32 RequestedView=1;FParse::Value(FCommandLine::Get(),TEXT("ImperialView="),RequestedView);CameraView=FMath::Clamp(RequestedView,1,5);
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialShowcase")))CameraView=3;
    if(GEngine&&!bSmoke&&!bAngleSmoke)GEngine->SetMaxFPS(120);
    if(bSwarmSmoke&&GEngine)GEngine->SetMaxFPS(60); // Real time: HTTP watchdog uses wall time.
    for(int32 I=0;I<10;++I)
    {
        const FVector Home=ImperialSoccerLayout::StartPosition(I<5?0:1,I%5);
        FActorSpawnParameters Params; Params.Name=FName(*FString::Printf(TEXT("IW_%02d"),I+1));Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* D=GetWorld()->SpawnActor<ASoccerDronePawn>(ASoccerDronePawn::StaticClass(),Home,FRotator(0,I<5?0:180,0),Params);
        D->SetTeamColor(GetTeamColor(I));D->SetPlayerRole(I%5); Drones.Add(D);Homes.Add(Home);PreviousPositions.Add(Home);GoalReady.Add(true);
    }
    LoadAIResult();Log(TEXT("10 aircraft ready. Keyboard input selected; USB axes are uncalibrated."));
    UE_LOG(LogTemp,Display,TEXT("IMPERIAL_RUNTIME_READY drones=%d physics=Chaos backend=SDACS_APF"),Drones.Num());
    if(auto* Scene=GetWorld()->GetPhysicsScene())Scene->OnPhysScenePreTick.AddUObject(this,&AImperialMatchManager::OnPhysicsTime);
}
void AImperialMatchManager::OnPhysicsTime(FPhysScene_Chaos* Scene,float Dt)
{
    const uint64 Previous=SimulationTime.LimitedFrames;
    SimulationTime.Record(GetWorld()->GetDeltaSeconds(),Dt);
    if(SimulationTime.LimitedFrames>Previous)
        UE_LOG(LogTemp,Warning,TEXT("IMPERIAL_TIME_LIMIT game_dt_s=%.6f physics_dt_s=%.6f omitted_total_s=%.6f deferred_s=0 match_clock=game"),GetWorld()->GetDeltaSeconds(),Dt,SimulationTime.DroppedSeconds);
}
void AImperialMatchManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    FinishAIResult(TEXT("application_exit"));
    if(auto* Scene=GetWorld()->GetPhysicsScene())Scene->OnPhysScenePreTick.RemoveAll(this);
    if(IsRecordingFlight()){RecordFlightSample(TEXT("stop"));FlightLogPath.Empty();}
    AdvanceGuidanceCommand();
    Super::EndPlay(EndPlayReason);
}
void AImperialMatchManager::CancelPendingGuidance()
{
    auto Previous=PendingGuidance;
    PendingGuidance.Reset();bRequestPending=false;GuidanceWindow.Abort();
    if(Previous.IsValid()){Previous->OnProcessRequestComplete().Unbind();Previous->CancelRequest();}
}
void AImperialMatchManager::AdvanceGuidanceCommand()
{
    GuidanceWindow.NewCommand();CancelPendingGuidance();LastRequestWallTime=0;
}
FString AImperialMatchManager::GuidanceMode() const
{
    return Mode==EImperialMode::Mission?TEXT("mission"):(Mode==EImperialMode::Returning?TEXT("return"):TEXT("hold"));
}
FLinearColor AImperialMatchManager::GetTeamColor(int32 Id)const{return Id<5?FLinearColor(.02f,.22f,1.f):FLinearColor(1.f,.025f,.035f);}
ASoccerDronePawn* AImperialMatchManager::GetDrone(int32 Id)const{return Drones.IsValidIndex(Id)?Drones[Id].Get():nullptr;}
ASoccerDronePawn* AImperialMatchManager::GetSelectedDrone()const{return GetDrone(Selected);}
void AImperialMatchManager::SelectDrone(int32 Id)
{
    if(!Drones.IsValidIndex(Id))return;
    if(IsAIPracticeActive()&&Id!=0){Log(TEXT("AI practice uses the blue striker. Stop AI or RESET before changing aircraft."));return;}Selected=Id;
    if(auto* PC=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0))){PC->Possess(Drones[Id]);PC->SetView(CameraView);}
}
void AImperialMatchManager::SelectNext(){SelectDrone((Selected+1)%FMath::Max(1,Drones.Num()));}
FString AImperialMatchManager::GetModeName()const
{
    if(!IsAutonomyAllowed())return TEXT("LOS MANUAL MATCH");
    if(bAIScoringClosed&&!IsAIPracticeActive())return TEXT("AI DUEL FINISHED / RESET TO TRAIN");
    if(AISettings.Duel&&IsAIPracticeActive())return IsAILanding()?TEXT("AI DUEL LANDING"):TEXT("AI DUEL / TRAINING VARIANT");
    switch(Mode){case EImperialMode::AIPractice:return TEXT("AI DEFENCE PRACTICE");case EImperialMode::AILanding:return TEXT("AI LANDING");case EImperialMode::Mission:return TEXT("SDACS AUTONOMOUS");case EImperialMode::Hold:return TEXT("POSITION HOLD");case EImperialMode::Returning:return TEXT("RETURNING");default:return TEXT("MANUAL TRAINING");}
}
void AImperialMatchManager::Log(const FString& Message){Notice=Message;Events.Add(Message);if(Events.Num()>40)Events.RemoveAt(0);}
void AImperialMatchManager::LaunchSwarm()
{
    if(IsAIPracticeActive()){Log(TEXT("Stop AI practice or RESET before starting a fleet mission."));return;}
    if(!IsAutonomyAllowed()){Log(TEXT("Autonomous guidance is available in practice mode. Reset to practice first."));return;}
    if(!bConnected){Log(TEXT("SDACS bridge offline. Run ImperialWings.cmd to start the local planner."));return;}
    AdvanceGuidanceCommand();Mode=EImperialMode::Mission;
    for(auto& D:Drones){D->SetEquipmentInspection(false);D->SetAutonomousEnabled(true);D->SetArmed(true);}
    Log(TEXT("Fleet autonomous: upstream SDACS APF targets, local Chaos motor forces."));
}
void AImperialMatchManager::HoldFleet()
{
    if(IsAIPracticeActive()){Log(TEXT("AI practice owns the defenders. Use STOP AI to land."));return;}
    if(!IsAutonomyAllowed()){Log(TEXT("HOLD is a practice aid. Keep manual control during a match."));return;}
    AdvanceGuidanceCommand();Mode=EImperialMode::Hold;
    for(auto& D:Drones)if(D->IsArmed()){D->SetAutonomousEnabled(true);D->SetAutonomousTarget(D->GetActorLocation());}
    Log(TEXT("Fleet holding current positions. Physics and collision remain active."));
}
void AImperialMatchManager::ReturnFleet()
{
    if(IsAIPracticeActive()){StopAIPractice();return;}
    if(!IsAutonomyAllowed()){Log(TEXT("RETURN is a practice aid. Land manually during a match."));return;}
    if(!bConnected){HoldFleet();Log(TEXT("SDACS offline: return unavailable. Holding locally; request RETURN after reconnect."));return;}
    AdvanceGuidanceCommand();Mode=EImperialMode::Returning;
    for(int32 I=0;I<Drones.Num();++I)if(Drones[I]->IsArmed()){Drones[I]->SetAutonomousEnabled(true);Drones[I]->SetAutonomousTarget(Drones[I]->GetActorLocation());}
    Log(TEXT("Fleet returning to launch pads for a controlled landing."));
}
void AImperialMatchManager::ManualSelected()
{
    if(auto* D=GetSelectedDrone()){AdvanceGuidanceCommand();D->SetAutonomousEnabled(false);D->SetArmed(false);if(!IsAIPracticeActive())Mode=EImperialMode::Manual;Log(TEXT("Selected aircraft disarmed. Current input source retained; low throttle is required to arm."));}
}
void AImperialMatchManager::ToggleArmSelected(){if(const auto* Pilot=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));Pilot&&Pilot->IsInputResetThisFrame())return;if(auto* D=GetSelectedDrone()){if(D->IsAutonomousEnabled())D->SetAutonomousEnabled(false);else D->SetArmed(!D->IsArmed());}}
void AImperialMatchManager::ResetMatch()
{
    FinishAIResult(TEXT("reset"));
    AdvanceGuidanceCommand();AILandingTargets.Reset();AIAge=AILandingAge=0;bAILandingFault=false;
    bAIScoringClosed=false;for(auto& D:Drones)D->ResetDrone();Mode=EImperialMode::Manual;BlueScore=OrangeScore=0;Elapsed=0;Rules.Reset();
    for(int32 I=0;I<Drones.Num();++I){PreviousPositions[I]=Drones[I]->GetActorLocation();GoalReady[I]=true;}
    // The next generation resets only this session's planner state. An unordered
    // separate /reset request could otherwise clear a newer mission.
    Events.Empty();Log(LastAIResult.Exists()&&!LastAIResult.bSaved?TEXT("Training reset. AI result NOT SAVED: open LAST RESULT to retry or discard."):TEXT("Training reset. All aircraft disarmed; scores cleared."));
    if(auto* Pilot=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0)))Pilot->FlushPilotContextKeys();
    if(auto* Arena=Cast<APrototypeArenaBuilder>(UGameplayStatics::GetActorOfClass(GetWorld(),APrototypeArenaBuilder::StaticClass()))){Arena->ResetGoals();for(int32 I=0;I<2;++I)if(auto* Goal=Arena->GetGoal(I))Goal->SetCompetitionStable(false);}
}
void AImperialMatchManager::StartMatch()
{
    if(IsAIPracticeActive()){Log(TEXT("Stop AI practice and land before starting an official manual match."));return;}
    ResetMatch();Rules.StartMatch();ChangeView(1);Log(TEXT("FIDA Class 40 LOS match: 10-second ready signal, then 3-minute sets. Manual control."));
    if(auto* Arena=Cast<APrototypeArenaBuilder>(UGameplayStatics::GetActorOfClass(GetWorld(),APrototypeArenaBuilder::StaticClass())))for(int32 I=0;I<2;++I)if(auto* Goal=Arena->GetGoal(I))Goal->SetCompetitionStable(true);
}
FString AImperialMatchManager::GetMatchStatus() const
{
    switch(Rules.GetPhase())
    {
    case drone_soccer::Phase::Ready:return TEXT("READY / MOTORS LOCKED");
    case drone_soccer::Phase::Playing:return FString::Printf(TEXT("SET %d / FIDA CLASS 40"),Rules.CurrentSet());
    case drone_soccer::Phase::Break:return TEXT("MAINTENANCE / 5 MIN");
    case drone_soccer::Phase::Finished:return TEXT("MATCH FINISHED");
    default:return TEXT("PRACTICE / FIDA CLASS 40");
    }
}
void AImperialMatchManager::ChangeView(int32 View)
{auto* PC=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));if(PC&&!PC->CanSetView(View)){Log(TEXT("Disarm manual aircraft before entering an audience/referee view."));return;}CameraView=FMath::Clamp(View,1,6);if(PC)PC->SetView(CameraView);}
void AImperialMatchManager::Tick(float Dt)
{
    Super::Tick(Dt);TickAIPractice(Dt);Elapsed+=Dt;SmoothFPS=FMath::Lerp(SmoothFPS,1.f/FMath::Max(.001f,Dt),.04f);
    if((bSmoke||bAngleSmoke||bContactSmoke||bViewSmoke)&&Elapsed>.2){SmokeMinDt=FMath::Min(SmokeMinDt,Dt);SmokeMaxDt=FMath::Max(SmokeMaxDt,Dt);}
    for(int32 I=0;I<Drones.Num();++I){TArray<FVector> Sources;TArray<float> Thrusts;for(int32 J=0;J<Drones.Num();++J)if(J!=I&&Drones[J]->IsArmed()){Sources.Add(Drones[J]->GetActorLocation()/100.);Thrusts.Add(Drones[J]->GetTotalThrustNewtons());}Drones[I]->SetWakeSources(Sources,Thrusts);}
    ClosestSeparation=1000;Conflicts=0;
    for(int32 I=0;I<Drones.Num();++I)for(int32 J=I+1;J<Drones.Num();++J){float D=FVector::Distance(Drones[I]->GetActorLocation(),Drones[J]->GetActorLocation())/100.f;ClosestSeparation=FMath::Min(ClosestSeparation,D);if(D<.9f)++Conflicts;}
    if(Mode==EImperialMode::Returning)
    {
        bool Landed=true;
        for(int32 I=0;I<Drones.Num();++I)if(Drones[I]->IsArmed()){
            if(FVector::Dist2D(Drones[I]->GetActorLocation(),Homes[I])<20&&Drones[I]->GetActorLocation().Z<28&&Drones[I]->GetFlightSpeed()<.2){Drones[I]->SetAutonomousEnabled(false);Drones[I]->SetArmed(false);}
            else{Landed=false;}
        }
        if(Landed){AdvanceGuidanceCommand();Mode=EImperialMode::Manual;Log(TEXT("Fleet landing complete. All motors disarmed."));}
    }
    const double WallNow=FPlatformTime::Seconds();
    if(GuidanceWindow.Expired(WallNow))CancelPendingGuidance();
    if(!bRequestPending&&!bSmoke&&!bAngleSmoke&&!bContactSmoke&&!bViewSmoke&&(LastRequestWallTime==0||WallNow-LastRequestWallTime>=.1))RequestGuidance();
    if(bDemo&&!bDemoStarted&&bConnected&&Elapsed>2){bDemoStarted=true;LaunchSwarm();}
    if(bConnected&&WallNow-LastResponse>2){bConnected=false;if(Mode==EImperialMode::Mission||Mode==EImperialMode::Returning)HoldFleet();Log(TEXT("SDACS timeout. Local hold; reconnect does not resume mission or return."));}
    UpdateScoring();if(bSmoke&&!bSmokeDone)SmokeTick();
    if(bAngleSmoke&&!bSmokeDone)AngleSmokeTick();
    if(bContactSmoke&&!bSmokeDone)ContactSmokeTick(Dt);
    if(bViewSmoke&&!bSmokeDone)ViewSmokeTick(Dt);
    if(bSwarmSmoke&&!bSmokeDone)SwarmSmokeTick();
    if(IsRecordingFlight()&&WallNow-LastFlightRecord>=.05){RecordFlightSample(TEXT("sample"));LastFlightRecord=WallNow;}
    if(!CaptureFile.IsEmpty()&&Elapsed>CaptureAt&&!bCaptureRequested){bCaptureRequested=true;FScreenshotRequest::RequestScreenshot(CaptureFile,true,false);}
    if(bCaptureRequested&&Elapsed>CaptureAt+1&&FPaths::FileExists(CaptureFile))FPlatformMisc::RequestExit(false);
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialProfile"))&&Elapsed>3&&!bProfileWritten)
    {
        FrameTimes.Add(Dt*1000.);
        if(Elapsed>18&&FrameTimes.Num()>20)
        {
            FrameTimes.Sort();double Total=0;for(double T:FrameTimes)Total+=T;
            FVector2D Size(0,0);if(auto* V=GetWorld()->GetGameViewport())V->GetViewportSize(Size);
            FString Profile=FString::Printf(TEXT("{\"samples\":%d,\"average_frame_ms\":%.3f,\"p95_frame_ms\":%.3f,\"average_fps\":%.2f,\"res_x\":%.0f,\"res_y\":%.0f,\"note\":\"rendered game frame pacing; not USB input latency\"}\n"),FrameTimes.Num(),Total/FrameTimes.Num(),FrameTimes[FMath::Min(FrameTimes.Num()-1,FMath::FloorToInt(FrameTimes.Num()*.95))],1000.*FrameTimes.Num()/Total,Size.X,Size.Y);
            AttachVerificationIdentity(Profile);
            FFileHelper::SaveStringToFile(Profile,*FPaths::Combine(ImperialUserData::Directory(),TEXT("render-profile.json")));bProfileWritten=true;
        }
    }
}
void AImperialMatchManager::RequestGuidance()
{
    const double Now=FPlatformTime::Seconds();ImperialGuidance::Ticket Ticket;
    if(!GuidanceWindow.Begin(Now,Ticket))return;
    const double Interval=LastRequestWallTime>0?Now-LastRequestWallTime:.1;
    auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("dt"),FMath::Clamp(Interval,.001,.5));
    O->SetNumberField(TEXT("sample_interval_s"),Interval);
    O->SetNumberField(TEXT("protocol_version"),ImperialGuidance::ProtocolVersion);
    O->SetStringField(TEXT("session_id"),GuidanceSession);
    O->SetNumberField(TEXT("command_id"),static_cast<double>(Ticket.Command));
    O->SetNumberField(TEXT("request_id"),static_cast<double>(Ticket.Request));
    O->SetStringField(TEXT("mode"),GuidanceMode());
    TArray<TSharedPtr<FJsonValue>> List;
    for(int32 I=0;I<Drones.Num();++I){auto D=MakeShared<FJsonObject>();D->SetNumberField(TEXT("id"),I);D->SetArrayField(TEXT("position_m"),JsonVector(Drones[I]->GetActorLocation()/100.));D->SetArrayField(TEXT("velocity_mps"),JsonVector(Drones[I]->GetBodyMesh()->GetPhysicsLinearVelocity()/100.));D->SetArrayField(TEXT("home_m"),JsonVector(Homes[I]/100.));List.Add(MakeShared<FJsonValueObject>(D));}
    O->SetArrayField(TEXT("drones"),List);FString Body;auto Writer=TJsonWriterFactory<>::Create(&Body);FJsonSerializer::Serialize(O,Writer);
    auto R=FHttpModule::Get().CreateRequest();R->SetURL(GuidanceUrl+TEXT("/step"));R->SetVerb(TEXT("POST"));R->SetTimeout(1.f);R->SetHeader(TEXT("Content-Type"),TEXT("application/json"));R->SetContentAsString(Body);R->OnProcessRequestComplete().BindUObject(this,&AImperialMatchManager::OnGuidance,Ticket);
    PendingGuidance=R;bRequestPending=R->ProcessRequest();LastRequestWallTime=Now;
    if(!bRequestPending){PendingGuidance.Reset();GuidanceWindow.Abort();}
}
void AImperialMatchManager::OnGuidance(FHttpRequestPtr Request,FHttpResponsePtr Response,bool bSuccess,ImperialGuidance::Ticket Ticket)
{
    if(Request!=PendingGuidance||!GuidanceWindow.IsCurrent(Ticket))return;
    PendingGuidance.Reset();bRequestPending=false;
    if(!GuidanceWindow.Complete(Ticket,FPlatformTime::Seconds())||!bSuccess||!Response.IsValid()||Response->GetResponseCode()!=200)return;
    TSharedPtr<FJsonObject> O;auto Reader=TJsonReaderFactory<>::Create(Response->GetContentAsString());if(!FJsonSerializer::Deserialize(Reader,O)||!O.IsValid())return;
    double Protocol=0,Command=0,Sequence=0;FString Session,ResponseMode;
    if(!O->TryGetNumberField(TEXT("protocol_version"),Protocol)||Protocol!=ImperialGuidance::ProtocolVersion||
       !O->TryGetStringField(TEXT("session_id"),Session)||Session!=GuidanceSession||
       !O->TryGetNumberField(TEXT("command_id"),Command)||Command!=static_cast<double>(Ticket.Command)||
       !O->TryGetNumberField(TEXT("request_id"),Sequence)||Sequence!=static_cast<double>(Ticket.Request)||
       !O->TryGetStringField(TEXT("mode"),ResponseMode)||ResponseMode!=GuidanceMode())return;
    FString Source;if(!O->TryGetStringField(TEXT("source"),Source)||Source!=TEXT("swarm-drone-atc"))return;
    const TArray<TSharedPtr<FJsonValue>>* Targets=nullptr;if(!O->TryGetArrayField(TEXT("targets"),Targets)||Targets->Num()!=Drones.Num())return;
    TArray<FVector> Goals;Goals.SetNum(Drones.Num());TSet<int32> Seen;
    for(auto T:*Targets){if(!T.IsValid()||T->Type!=EJson::Object)return;auto D=T->AsObject();double Number=0;if(!D.IsValid()||!D->TryGetNumberField(TEXT("id"),Number)||!FMath::IsFinite(Number)||Number<0||Number>=Drones.Num()||Number!=FMath::FloorToDouble(Number))return;const int32 Id=static_cast<int32>(Number);if(Seen.Contains(Id))return;FVector V;if(!ReadVector(D,TEXT("target_m"),V)||FMath::Abs(V.X)>7.8||FMath::Abs(V.Y)>3.3||V.Z<.24||V.Z>4.3)return;Goals[Id]=V*100;Seen.Add(Id);}
    bool WasConnected=bConnected;bConnected=true;LastResponse=FPlatformTime::Seconds();O->TryGetStringField(TEXT("source_commit"),SourceCommit);
    if(!WasConnected)Log(TEXT("SDACS upstream APF connected. Session and command verified."));
    if(IsAutonomyAllowed()&&(Mode==EImperialMode::Mission||Mode==EImperialMode::Returning))for(int32 I=0;I<Drones.Num();++I)if(Drones[I]->IsAutonomousEnabled())Drones[I]->SetAutonomousTarget(Goals[I]);
}
void AImperialMatchManager::UpdateScoring()
{
    if(bAIScoringClosed)return;
    std::array<drone_soccer::Vec3,10> Poses;
    for(int32 I=0;I<Drones.Num();++I){FVector P=Drones[I]->GetActorLocation()/100.;Poses[I]={P.X,P.Y,P.Z};}
    const int Before=Rules.Score(0)+Rules.Score(1);const auto Phase=Rules.GetPhase();
    auto Frames=drone_soccer::NominalGoalFrames();
    if(auto* Arena=Cast<APrototypeArenaBuilder>(UGameplayStatics::GetActorOfClass(GetWorld(),APrototypeArenaBuilder::StaticClass())))
        for(int32 I=0;I<2;++I)if(const auto* Goal=Arena->GetGoal(I))
        {const FVector C=Goal->GetCentre()/100.;const FQuat Q=Goal->GetRingRotation();const FVector N=Q.GetAxisX(),S=Q.GetAxisY(),U=Q.GetAxisZ();Frames[I].center={C.X,C.Y,C.Z};Frames[I].normal={N.X,N.Y,N.Z};Frames[I].side={S.X,S.Y,S.Z};Frames[I].up={U.X,U.Y,U.Z};}
    Rules.Update(Poses,GetWorld()->GetDeltaSeconds(),Frames);
    if(Rules.Score(0)+Rules.Score(1)>Before)Log(FString::Printf(TEXT("GOAL! Blue %d : %d Red. Scoring team must all retreat."),Rules.Score(0),Rules.Score(1)));
    if(auto* Arena=Cast<APrototypeArenaBuilder>(UGameplayStatics::GetActorOfClass(GetWorld(),APrototypeArenaBuilder::StaticClass()))){Arena->SetGoalSignal(1,Rules.IsOffside(0));Arena->SetGoalSignal(0,Rules.IsOffside(1));}
    HandleMatchPhaseTransition(Phase);
}
void AImperialMatchManager::HandleMatchPhaseTransition(drone_soccer::Phase Previous)
{
    if(Previous!=drone_soccer::Phase::Playing||Rules.GetPhase()==Previous)return;
    AdvanceGuidanceCommand();Mode=EImperialMode::Manual;
    // No automatic route, position hold, or mid-air motor cut for a manual pilot.
    // A disarmed aircraft cannot re-arm in the maintenance/finished phase.
    for(auto& D:Drones)if(D->IsAutonomousEnabled())D->SetAutonomousEnabled(false);
    Log(TEXT("Set ended. Manual landing control retained; new ARM and autonomous aids are locked."));
}
bool AImperialMatchManager::VerifyMatchAutonomyGates()
{
    // Explicit clock/API fixture only, not a native keyboard or USB pilot test.
    StartMatch();auto* D=GetDrone(0);if(!D)return false;
    std::array<drone_soccer::Vec3,10> Poses;
    for(int32 I=0;I<Drones.Num();++I){const FVector P=Drones[I]->GetActorLocation()/100.;Poses[I]={P.X,P.Y,P.Z};}
    const auto Reject=[&]()
    {
        const bool WasArmed=D->IsArmed();const auto Generation=GuidanceWindow.Command();
        LaunchSwarm();HoldFleet();ReturnFleet();D->SetAutonomousEnabled(true);
        bool Safe=Mode==EImperialMode::Manual&&GuidanceWindow.Command()==Generation&&D->IsArmed()==WasArmed;
        for(auto& Item:Drones)Safe&=!Item->IsAutonomousEnabled();return Safe;
    };
    bool Safe=Rules.GetPhase()==drone_soccer::Phase::Ready&&Reject()&&!D->SetArmed(true);
    Rules.Update(Poses,10);Safe&=Rules.GetPhase()==drone_soccer::Phase::Playing&&D->SetArmed(true)&&Reject();
    const auto Before=Rules.GetPhase();Rules.Update(Poses,180);HandleMatchPhaseTransition(Before);
    Safe&=Rules.GetPhase()==drone_soccer::Phase::Break&&Reject()&&D->IsArmed()&&!GetDrone(1)->SetArmed(true);
    // Reach set three's final whistle through the real rule clock transitions.
    Rules.Update(Poses,800);const auto FinalBefore=Rules.GetPhase();Rules.Update(Poses,180);HandleMatchPhaseTransition(FinalBefore);
    Safe&=Rules.GetPhase()==drone_soccer::Phase::Finished&&Reject()&&D->IsArmed()&&!GetDrone(1)->SetArmed(true);
    D->SetArmed(false);return Safe;
}
bool AImperialMatchManager::ExportTelemetry()
{
    FString Dir=FPaths::Combine(ImperialUserData::Directory(),TEXT("Reports"));IFileManager::Get().MakeDirectory(*Dir,true);
    FString File=FPaths::Combine(Dir,FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))+FString::Printf(TEXT("-%llu-fleet.csv"),FPlatformTime::Cycles64()));
    auto* Radio=GetWorld()->GetGameInstance()->GetSubsystem<UImperialRadioSubsystem>();
    const auto* Device=Radio?Radio->GetSelectedDevice():nullptr;
    const FString RadioId=Device?FString::Printf(TEXT("%04X:%04X"),Device->ManufacturerId,Device->ProductId):TEXT("NONE");
    auto* Arena=Cast<APrototypeArenaBuilder>(UGameplayStatics::GetActorOfClass(GetWorld(),APrototypeArenaBuilder::StaticClass()));
    auto* Pilot=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    const FVector Station=Pilot?Pilot->GetPilotStationPosition()/100.:FVector::ZeroVector;
    auto* Visuals=Cast<AImperialPresentationEffects>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialPresentationEffects::StaticClass()));
    const auto N=[](double Value){return FString::Printf(TEXT("%.6f"),Value);};
    const auto B=[](bool Value){return Value?TEXT("1"):TEXT("0");};
    FString CSV=TEXT("id,team,armed,autonomous,x_m,y_m,z_m,speed_mps,throttle,mode,sdacs_connected,input_source,selected,camera_view,radio_connected,radio_calibrated,radio_enabled,battery_soc,battery_v,net_deflection_m,radio_setup_phase,radio_setup_channel,radio_setup_samples,radio_device_id,build_version,build_commit,build_source_sha256,build_utc,guidance_session,guidance_command,flight_mode,tilt_deg,roll_deg,pitch_deg,yaw_deg,angle_limit_deg,role,home_x_m,home_y_m,pad_x_m,pad_y_m,pilot_x_m,pilot_y_m\n");
    for(int32 I=0;I<Drones.Num();++I)
    {
        auto* D=Drones[I].Get();const FVector P=D->GetActorLocation()/100.;
        const FRotator Rotation=D->GetBodyMesh()->GetComponentRotation();
        FVector Pad(-1000000);if(Arena)Arena->GetStartPadPosition(I,Pad);Pad/=100.;
        const TArray<FString> Row={
            FString::Printf(TEXT("IW-%02d"),I+1),I<5?TEXT("BLUE"):TEXT("RED"),B(D->IsArmed()),B(D->IsAutonomousEnabled()),
            N(P.X),N(P.Y),N(P.Z),N(D->GetFlightSpeed()),N(D->GetThrottle()),GetModeName(),B(bConnected),D->GetInputSource(),B(I==Selected),FString::FromInt(CameraView),
            B(Radio&&Radio->IsConnected()),B(Radio&&Radio->IsCalibrated()),B(Radio&&Radio->IsEnabled()),N(D->GetBatteryPercent()/100.f),N(D->GetBatteryVoltage()),N(D->GetNetDeflection()),
            FString::FromInt(Radio?static_cast<int32>(Radio->GetGuidedPhase()):0),FString::FromInt(Radio?Radio->GetGuidedChannel():0),FString::FromInt(Radio?Radio->GetCaptureSampleCount():0),RadioId,
            ImperialBuildInfo::Version(),ImperialBuildInfo::Commit(),ImperialBuildInfo::SourceHash(),ImperialBuildInfo::BuiltAt(),GuidanceSession,FString::Printf(TEXT("%llu"),static_cast<uint64>(GuidanceWindow.Command())),
            D->GetFlightModeName(),N(D->GetTiltDegrees()),N(Rotation.Roll),N(Rotation.Pitch),N(Rotation.Yaw),N(D->GetAngleLimitDegrees()),ImperialSoccerLayout::RoleName(D->GetPlayerRole()),
            N(Homes[I].X/100.),N(Homes[I].Y/100.),N(Pad.X),N(Pad.Y),N(Station.X),N(Station.Y)};
        const auto* HUD=Pilot?Cast<AImperialHUD>(Pilot->GetHUD()):nullptr;
        if(I==0)CSV.ReplaceInline(TEXT("pilot_y_m\n"),TEXT("pilot_y_m,visual_quality,dust_capacity,radio_panel_open,flight_recording,radio_roll,radio_pitch,radio_yaw,radio_throttle,guard_integrity_pct,guard_impact_count,guard_impact_work_j,blue_goal_sway_cm,red_goal_sway_cm,camera_role,camera_x_m,camera_y_m,camera_z_m,photoreal_equipment\n"));
        const FVector Camera=Pilot?Pilot->GetRoleCameraPosition()/100.:FVector::ZeroVector;
        const float BlueSway=Arena&&Arena->GetGoal(0)?Arena->GetGoal(0)->GetSwayCm():0,RedSway=Arena&&Arena->GetGoal(1)?Arena->GetGoal(1)->GetSwayCm():0;
        CSV+=FString::Join(Row,TEXT(","))+FString::Printf(TEXT(",%s,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.3f,%d,%.4f,%.3f,%.3f,%s,%.4f,%.4f,%.4f,%d\n"),Visuals?*Visuals->GetQualityName():TEXT("Unavailable"),Visuals?Visuals->GetDustCapacity():0,
            HUD&&HUD->IsRadioPanelOpen()?1:0,IsRecordingFlight()?1:0,Radio?Radio->GetNormalizedRoll():0.f,Radio?Radio->GetNormalizedPitch():0.f,Radio?Radio->GetNormalizedYaw():0.f,Radio?Radio->GetThrottle():0.f,
            D->GetGuardIntegrityPercent(),D->GetGuardImpactCount(),D->GetGuardImpactWorkJ(),BlueSway,RedSway,Pilot?*Pilot->GetCameraRoleName():TEXT("NONE"),Camera.X,Camera.Y,Camera.Z,D->HasPhotorealEquipment()?1:0);
    }
    bool Ok=FFileHelper::SaveStringToFile(CSV,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);Log(Ok?TEXT("Telemetry CSV exported to Saved/Reports."):TEXT("Telemetry export failed."));return Ok;
}
void AImperialMatchManager::RecordGuardImpact(ASoccerDronePawn* Drone,float DissipatedJ)
{
    const int32 Id=Drones.IndexOfByKey(Drone);
    if(Id!=INDEX_NONE)Log(FString::Printf(TEXT("IW-%02d guard contact: %.0f%% integrity, %.1f J estimated dissipated work."),Id+1,Drone->GetGuardIntegrityPercent(),DissipatedJ));
}
void AImperialMatchManager::ToggleFlightRecording()
{
    if(IsRecordingFlight()){RecordFlightSample(TEXT("stop"));FlightLogPath.Empty();Log(TEXT("Flight recording saved in Saved/FlightLogs."));return;}
    const FString Directory=FPaths::Combine(ImperialUserData::Directory(),TEXT("FlightLogs"));
    IFileManager::Get().MakeDirectory(*Directory,true);
    FlightLogPath=FPaths::Combine(Directory,FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"))+TEXT("-")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".jsonl"));
    LastFlightRecord=0;RecordFlightSample(TEXT("start"));Log(TEXT("Recording actual controls and physics. F6 stops recording."));
}
void AImperialMatchManager::RecordFlightSample(const FString& Event)
{
    if(FlightLogPath.IsEmpty())return;
    auto Root=MakeShared<FJsonObject>();Root->SetStringField(TEXT("event"),Event);
    Root->SetStringField(TEXT("utc"),FDateTime::UtcNow().ToIso8601());Root->SetNumberField(TEXT("wall_s"),FPlatformTime::Seconds());
    Root->SetNumberField(TEXT("game_s"),Elapsed);Root->SetNumberField(TEXT("selected"),Selected);
    Root->SetStringField(TEXT("source_sha256"),ImperialBuildInfo::SourceHash());
    Root->SetObjectField(TEXT("timing"),SimulationTime.Json());
    auto AI=MakeShared<FJsonObject>();AI->SetBoolField(TEXT("active"),IsAIPracticeActive());AI->SetBoolField(TEXT("landing"),IsAILanding());
    AI->SetNumberField(TEXT("opponents"),AISettings.Count);AI->SetNumberField(TEXT("defenders"),AISettings.DefenderCount());AI->SetNumberField(TEXT("difficulty"),AISettings.Difficulty);AI->SetNumberField(TEXT("seed"),AISettings.Seed);
    AI->SetBoolField(TEXT("duel"),AISettings.Duel);AI->SetNumberField(TEXT("attack_phase"),GetAIAttackPhase());AI->SetNumberField(TEXT("attack_cycles"),GetAIAttackCycles());AI->SetNumberField(TEXT("recovery_attempts"),GetAIRecoveryAttempts());AI->SetNumberField(TEXT("recovering"),GetAIRecoveringCount());AI->SetNumberField(TEXT("decisions"),AIPlanner.DecisionCount());AI->SetNumberField(TEXT("age_s"),AIAge);Root->SetObjectField(TEXT("ai_practice"),AI);
    auto* Radio=GetWorld()->GetGameInstance()->GetSubsystem<UImperialRadioSubsystem>();
    Root->SetBoolField(TEXT("radio_connected"),Radio&&Radio->IsConnected());Root->SetBoolField(TEXT("radio_enabled"),Radio&&Radio->IsEnabled());
    if(Radio){Root->SetArrayField(TEXT("stick"),{MakeShared<FJsonValueNumber>(Radio->GetNormalizedRoll()),MakeShared<FJsonValueNumber>(Radio->GetNormalizedPitch()),MakeShared<FJsonValueNumber>(Radio->GetNormalizedYaw()),MakeShared<FJsonValueNumber>(Radio->GetThrottle())});}
    TArray<TSharedPtr<FJsonValue>> Rows;
    for(int32 I=0;I<Drones.Num();++I)
    {
        auto* D=Drones[I].Get();auto Row=MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("id"),I);Row->SetBoolField(TEXT("armed"),D->IsArmed());Row->SetBoolField(TEXT("autonomous"),D->IsAutonomousEnabled());
        Row->SetStringField(TEXT("input_source"),D->GetInputSource());Row->SetStringField(TEXT("flight_mode"),D->GetFlightModeName());
        Row->SetArrayField(TEXT("position_m"),JsonVector(D->GetActorLocation()/100.));Row->SetNumberField(TEXT("speed_mps"),D->GetFlightSpeed());
        Row->SetArrayField(TEXT("autonomous_target_m"),JsonVector(D->GetAutonomousTarget()/100.));
        Row->SetNumberField(TEXT("throttle"),D->GetThrottle());Row->SetNumberField(TEXT("tilt_deg"),D->GetTiltDegrees());
        Row->SetNumberField(TEXT("throttle_command"),D->GetThrottleCommand());Row->SetBoolField(TEXT("precision_throttle"),D->UsesPrecisionThrottle());
        Row->SetNumberField(TEXT("flight_model_s"),D->GetFlightModelSeconds());Row->SetNumberField(TEXT("sensor_model_s"),D->GetSensorModelSeconds());
        Row->SetNumberField(TEXT("battery_consumed_ah"),D->GetBatteryConsumedAh());Row->SetNumberField(TEXT("battery_current_a"),D->GetBatteryCurrentA());
        Row->SetNumberField(TEXT("battery_delivered_power_w"),D->GetBatteryDeliveredPowerW());Row->SetBoolField(TEXT("battery_power_limited"),D->IsBatteryPowerLimited());
        Row->SetNumberField(TEXT("motor_requested_power_w"),D->GetRequestedMotorPowerW());Row->SetNumberField(TEXT("motor_applied_power_w"),D->GetMotorElectricalPowerW());
        Row->SetNumberField(TEXT("motor_available_power_w"),D->GetAvailableMotorPowerW());Row->SetNumberField(TEXT("motor_thrust_scale"),D->GetMotorThrustScale());
        Row->SetNumberField(TEXT("guard_integrity_pct"),D->GetGuardIntegrityPercent());Row->SetNumberField(TEXT("guard_impact_count"),D->GetGuardImpactCount());
        const auto Rotation=D->GetBodyMesh()->GetComponentRotation();Row->SetArrayField(TEXT("rotation_deg"),JsonVector(FVector(Rotation.Roll,Rotation.Pitch,Rotation.Yaw)));
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Root->SetArrayField(TEXT("aircraft"),Rows);FString Text;FJsonSerializer::Serialize(Root,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
    Text+=TEXT("\n");
    if(!FFileHelper::SaveStringToFile(Text,*FlightLogPath,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append))
    {FlightLogPath.Empty();Log(TEXT("Flight recording stopped: could not write to Saved/FlightLogs."));}
}
void AImperialMatchManager::ContactSmokeTick(float Dt)
{
    auto* Arena=Cast<APrototypeArenaBuilder>(UGameplayStatics::GetActorOfClass(GetWorld(),APrototypeArenaBuilder::StaticClass()));
    auto* Goal=Arena?Arena->GetGoal(1):nullptr;auto* Drone=GetDrone(0);
    if(!Goal||!Drone)return;
    FixtureAge+=Dt;FixtureGoalPeak=FMath::Max(FixtureGoalPeak,Goal->GetSwayCm());
    const auto LaunchContact=[&]()
    {
        Drone->SetArmed(false);Drone->GetBodyMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Drone->GetBodyMesh()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        const FQuat Q=Goal->GetRingRotation();Drone->GetBodyMesh()->SetWorldLocationAndRotation(Goal->GetCentre()+Q.RotateVector(FVector(-100,42,0)),Q,false,nullptr,ETeleportType::TeleportPhysics);
        Drone->GetBodyMesh()->SetPhysicsLinearVelocity(Q.RotateVector(FVector(500,0,0)));
    };
    if(SmokePhase==4)
    {
        FixtureStableGoalPeak=FMath::Max(FixtureStableGoalPeak,Goal->GetSwayCm());if(FixtureAge<1.5)return;
        const bool Stable=FixtureStableGoalPeak<.5f&&Goal->GetImpactCount()>0;bFixtureOk&=Stable;
        ContactVerification->SetNumberField(TEXT("competition_peak_sway_cm"),FixtureStableGoalPeak);ContactVerification->SetBoolField(TEXT("competition_suspension_stable"),Stable);
        ContactVerification->SetNumberField(TEXT("guard_before_reset_pct"),Drone->GetGuardIntegrityPercent());ResetMatch();
        const bool ResetOk=Drone->GetGuardIntegrityPercent()>99.999&&Drone->GetGuardImpactCount()==0&&Goal->GetSwayCm()<.01;bFixtureOk&=ResetOk;
        ContactVerification->SetBoolField(TEXT("reset_repairs_guard_and_centres_goal"),ResetOk);ContactVerification->SetBoolField(TEXT("ok"),bFixtureOk);
        FString Text;FJsonSerializer::Serialize(ContactVerification.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));AttachVerificationIdentity(Text,SmokeMinDt,SmokeMaxDt);
        FFileHelper::SaveStringToFile(Text,*(SmokeResultFile.IsEmpty()?ImperialUserData::Path(TEXT("contact-smoke.json")):SmokeResultFile));
        bSmokeDone=true;FPlatformMisc::RequestExitWithStatus(false,bFixtureOk?0:1);return;
    }
    if(SmokePhase==0&&Elapsed>.5)
    {
        for(auto& D:Drones)D->ResetDrone();Arena->ResetGoals();FixtureGoalPeak=0;
        bFixtureOk=Goal->HasOpenPhysicalHoop()&&Arena->GetGoal(0)&&Arena->GetGoal(0)->HasOpenPhysicalHoop();
        LaunchContact();SmokePhase=1;FixtureAge=0;return;
    }
    if(SmokePhase==1&&FixtureAge>.65f)
    {
        FixtureGuardFirst=Drone->GetGuardIntegrityPercent();bFixtureOk&=FixtureGuardFirst<99.99f&&Goal->GetImpactCount()>0;
        LaunchContact();SmokePhase=2;FixtureAge=0;return;
    }
    if(SmokePhase==2&&FixtureAge>.65f)
    {
        FixtureGuardSecond=Drone->GetGuardIntegrityPercent();FixtureGoalHits=Goal->GetImpactCount();
        bFixtureOk&=FixtureGuardSecond<FixtureGuardFirst&&Drone->GetGuardImpactCount()>=2;
        Drone->GetBodyMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);Drone->GetBodyMesh()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        // Place the collision sphere on the floor. Homes are launch poses 35cm
        // high: returning there would add a new drop impact to an idle test.
        Drone->GetBodyMesh()->SetWorldLocationAndRotation(FVector(Homes[0].X,Homes[0].Y,20),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
        SmokePhase=3;FixtureAge=0;return;
    }
    if(SmokePhase!=3||FixtureAge<12)return;
    const float Settled=Goal->GetSwayCm();const bool IdleUnchanged=FMath::Abs(Drone->GetGuardIntegrityPercent()-FixtureGuardSecond)<.001;
    auto* FX=Cast<AImperialPresentationEffects>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialPresentationEffects::StaticClass()));
    const int32 Chips=FX?FX->GetGuardChipBursts():0;
    bFixtureOk&=FixtureGoalPeak>.5&&FixtureGoalPeak<100&&Settled<FMath::Max(.5f,FixtureGoalPeak*.5f)&&IdleUnchanged&&Rules.Score(0)==0&&Rules.Score(1)==0&&Chips>0;
    const float GuardBeforeReset=Drone->GetGuardIntegrityPercent();
    const FVector GoalOffset=Goal->GetCentre()-FVector(650,0,300);
    const double GoalMass=Goal->GetPhysicsRoot()->GetMass();
    const FVector GoalOmega=Goal->GetPhysicsRoot()->GetPhysicsAngularVelocityInRadians();
    auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("ok"),bFixtureOk);
    Result->SetStringField(TEXT("fixture"),TEXT("two practice and one competition initial-velocity Chaos collisions; idle damping and explicit reset; no USB input"));
    Result->SetNumberField(TEXT("guard_after_first_pct"),FixtureGuardFirst);Result->SetNumberField(TEXT("guard_after_second_pct"),FixtureGuardSecond);Result->SetNumberField(TEXT("guard_before_reset_pct"),GuardBeforeReset);
    Result->SetNumberField(TEXT("goal_contact_events"),FixtureGoalHits);Result->SetNumberField(TEXT("goal_peak_sway_cm"),FixtureGoalPeak);Result->SetNumberField(TEXT("goal_settled_sway_cm"),Settled);
    Result->SetNumberField(TEXT("goal_mass_kg"),GoalMass);Result->SetArrayField(TEXT("goal_settled_offset_cm"),JsonVector(GoalOffset));Result->SetArrayField(TEXT("goal_settled_omega_rad_s"),JsonVector(GoalOmega));
    Result->SetNumberField(TEXT("guard_chip_bursts"),Chips);Result->SetBoolField(TEXT("idle_contact_does_not_wear"),IdleUnchanged);
    ContactVerification=Result;Goal->ResetGoal();Goal->SetCompetitionStable(true);LaunchContact();SmokePhase=4;FixtureAge=0;
}
void AImperialMatchManager::ViewSmokeTick(float Dt)
{
    auto* PC=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));auto* Drone=GetSelectedDrone();if(!PC||!Drone)return;
    FixtureAge+=Dt;
    if(SmokePhase==0&&Elapsed>.5){Drone->SetArmed(false);ChangeView(1);SmokePhase=1;FixtureAge=0;return;}
    if(SmokePhase>=1&&SmokePhase<=5&&FixtureAge>.45f)
    {
        const FVector Expected=PC->GetRoleCameraPosition();const FVector Actual=PC->PlayerCameraManager?PC->PlayerCameraManager->GetCameraLocation():FVector(1e9);
        const bool Position=FVector::Dist(Expected,Actual)<3;
        bool Safe=true;if(SmokePhase==4||SmokePhase==5)Safe=!Drone->SetArmed(true)&&!Drone->IsArmed()&&Drone->GetInputSource()==TEXT("VIEW ONLY");
        for(auto& D:Drones)Safe&=D->GetGuardIntegrityPercent()>99.999f;
        const bool Valid=CameraView==SmokePhase&&Position&&Safe;bFixtureOk&=Valid;
        auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("view"),CameraView);Row->SetStringField(TEXT("camera_role"),PC->GetCameraRoleName());Row->SetArrayField(TEXT("position_cm"),JsonVector(Actual));Row->SetBoolField(TEXT("read_only_arm_rejected"),Safe);Row->SetBoolField(TEXT("ok"),Valid);FixtureViewRows.Add(MakeShared<FJsonValueObject>(Row));
        ++SmokePhase;FixtureAge=0;if(SmokePhase<=5)ChangeView(SmokePhase);return;
    }
    if(SmokePhase==6)
    {
        ChangeView(1);const bool Armed=Drone->SetArmed(true);ChangeView(4);
        const bool SwitchRejected=Armed&&CameraView==1&&Drone->IsArmed();bFixtureOk&=SwitchRejected;
        Drone->SetArmed(false);PC->ToggleEquipmentInspection();
        bFixtureOk&=CameraView==6&&Drone->HasPhotorealEquipment()&&Drone->IsEquipmentInspection()&&!Drone->SetArmed(true);
        SmokePhase=7;FixtureAge=0;return;
    }
    if(SmokePhase!=7||FixtureAge<.5f)return;
    auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("view"),6);Row->SetStringField(TEXT("camera_role"),PC->GetCameraRoleName());Row->SetBoolField(TEXT("ok"),Drone->IsEquipmentInspection()&&!Drone->IsArmed()&&Drone->HasPhotorealEquipment());FixtureViewRows.Add(MakeShared<FJsonValueObject>(Row));
    bFixtureOk&=Drone->IsEquipmentInspection()&&!Drone->IsArmed();PC->ToggleEquipmentInspection();bFixtureOk&=CameraView==1&&!Drone->IsEquipmentInspection();
    auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("ok"),bFixtureOk);Result->SetArrayField(TEXT("views"),FixtureViewRows);
    Result->SetStringField(TEXT("fixture"),TEXT("actual camera/controller API calls, camera-manager poses and arm rejection; not physical USB or native mouse input"));
    FString Text;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text));AttachVerificationIdentity(Text,SmokeMinDt,SmokeMaxDt);
    FFileHelper::SaveStringToFile(Text,*(SmokeResultFile.IsEmpty()?ImperialUserData::Path(TEXT("view-smoke.json")):SmokeResultFile));
    bSmokeDone=true;FPlatformMisc::RequestExitWithStatus(false,bFixtureOk?0:1);
}
void AImperialMatchManager::SwarmCheckpoint(const FString& Event)
{
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("event"),Event);Row->SetNumberField(TEXT("game_time_s"),Elapsed);
    Row->SetStringField(TEXT("mode"),GetModeName());Row->SetBoolField(TEXT("connected"),bConnected);
    TArray<TSharedPtr<FJsonValue>> Aircraft;
    for(int32 I=0;I<Drones.Num();++I)
    {
        auto Item=MakeShared<FJsonObject>();Item->SetNumberField(TEXT("id"),I);Item->SetArrayField(TEXT("position_cm"),JsonVector(Drones[I]->GetActorLocation()));
        Item->SetArrayField(TEXT("target_cm"),JsonVector(Drones[I]->GetAutonomousTarget()));Item->SetBoolField(TEXT("autonomous"),Drones[I]->IsAutonomousEnabled());
        Item->SetNumberField(TEXT("speed_mps"),Drones[I]->GetFlightSpeed());Item->SetNumberField(TEXT("tilt_deg"),Drones[I]->GetTiltDegrees());
        Item->SetNumberField(TEXT("guard_pct"),Drones[I]->GetGuardIntegrityPercent());Item->SetBoolField(TEXT("armed"),Drones[I]->IsArmed());Aircraft.Add(MakeShared<FJsonValueObject>(Item));
    }
    Row->SetArrayField(TEXT("drones"),Aircraft);
    FString Line;FJsonSerializer::Serialize(Row,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line));
    if(!SmokeResultFile.IsEmpty())FFileHelper::SaveStringToFile(Line+TEXT("\n"),*(SmokeResultFile+TEXT(".events.jsonl")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);
    if(Event!=TEXT("sample"))SwarmCheckpoints.Add(MakeShared<FJsonValueObject>(Row));
}
void AImperialMatchManager::SwarmSmokeTick()
{
    const double Age=Elapsed-SwarmStageStart;
    if(SmokePhase>0&&Elapsed-SwarmLastSample>.25)
    {
        SwarmLastSample=Elapsed;SwarmMinSeparation=FMath::Min(SwarmMinSeparation,ClosestSeparation);
        for(int32 I=0;I<Drones.Num();++I)
        {
            const FVector P=Drones[I]->GetActorLocation();
            bSwarmFinite&=!P.ContainsNaN()&&FMath::Abs(P.X)<805&&FMath::Abs(P.Y)<355&&P.Z>10&&P.Z<455;
            if(Drones[I]->IsArmed()){SwarmMaxTilt=FMath::Max(SwarmMaxTilt,Drones[I]->GetTiltDegrees());SwarmMaxAltitude=FMath::Max(SwarmMaxAltitude,(float)P.Z);}
            if(SmokePhase==2&&Age>1&&SwarmHoldPositions.IsValidIndex(I))SwarmHoldDrift=FMath::Max(SwarmHoldDrift,(float)FVector::Dist(P,SwarmHoldPositions[I]));
        }
        SwarmCheckpoint(TEXT("sample"));
    }
    if(SmokePhase==0&&bConnected&&Elapsed>.5)
    {LaunchSwarm();SmokePhase=1;SwarmStageStart=Elapsed;SwarmCheckpoint(TEXT("launch"));return;}
    if(SmokePhase==1&&Age>18)
    {
        bSwarmAirborne=Drones.Num()==10;
        for(auto& D:Drones)bSwarmAirborne&=D->IsArmed()&&D->GetAltitudeMeters()>.8&&D->GetAltitudeMeters()<4.1;
        HoldFleet();for(auto& D:Drones)SwarmHoldPositions.Add(D->GetActorLocation());
        SmokePhase=2;SwarmStageStart=Elapsed;SwarmCheckpoint(TEXT("hold"));return;
    }
    if(SmokePhase==2&&Age>4)
    {ReturnFleet();SmokePhase=3;SwarmStageStart=Elapsed;SwarmCheckpoint(TEXT("return"));return;}
    if(SmokePhase==3&&Age>1.5)
    {SmokePhase=4;SwarmStageStart=Elapsed;SwarmCheckpoint(TEXT("interrupt_return"));return;}
    if(SmokePhase==4)
    {
        if(!bConnected&&Mode==EImperialMode::Hold&&!bSwarmTimeoutHeld){bSwarmTimeoutHeld=true;SwarmCheckpoint(TEXT("timeout_held"));}
        if(bSwarmTimeoutHeld&&bConnected)
        {bSwarmReconnectHeld=Mode==EImperialMode::Hold;SwarmCheckpoint(TEXT("reconnect_held"));ReturnFleet();SmokePhase=5;SwarmStageStart=Elapsed;return;}
    }
    if(SmokePhase==5&&Mode==EImperialMode::Manual)
    {SmokePhase=6;SwarmStageStart=Elapsed;SwarmCheckpoint(TEXT("landed"));return;}
    bool Finish=SmokePhase==6&&Age>2;
    if(Elapsed>125)Finish=true;
    if(!Finish)return;
    bool Landed=SmokePhase==6&&Drones.Num()==10;
    for(int32 I=0;I<Drones.Num();++I)Landed&=!Drones[I]->IsArmed()&&FVector::Dist2D(Drones[I]->GetActorLocation(),Homes[I])<45&&Drones[I]->GetAltitudeMeters()<.3&&Drones[I]->GetFlightSpeed()<.15;
    const float Separation=SwarmMinSeparation,Drift=SwarmHoldDrift,PeakTilt=SwarmMaxTilt,PeakAltitude=SwarmMaxAltitude;
    // Match mode must reject auto-launch and keep all ten motors stopped.
    bool ManualMatchGate=VerifyMatchAutonomyGates();
    for(auto& D:Drones)ManualMatchGate&=!D->IsArmed()&&!D->IsAutonomousEnabled();
    const bool Valid=Landed&&bSwarmAirborne&&bSwarmFinite&&bSwarmTimeoutHeld&&bSwarmReconnectHeld&&ManualMatchGate&&Separation>.9&&Drift<100&&PeakTilt<70;
    auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("ok"),Valid);Result->SetStringField(TEXT("fixture"),TEXT("real-time ten Chaos aircraft using actual upstream APF HTTP; bounded server intentionally returns 503 during return; no USB or flight position corrections"));
    Result->SetStringField(TEXT("source_commit"),SourceCommit);Result->SetBoolField(TEXT("ten_airborne"),bSwarmAirborne);Result->SetBoolField(TEXT("ten_landed_disarmed"),Landed);
    Result->SetBoolField(TEXT("finite_inside_court"),bSwarmFinite);Result->SetBoolField(TEXT("timeout_local_hold"),bSwarmTimeoutHeld);Result->SetBoolField(TEXT("reconnect_does_not_resume"),bSwarmReconnectHeld);Result->SetBoolField(TEXT("manual_match_auto_rejected"),ManualMatchGate);
    Result->SetNumberField(TEXT("minimum_separation_m"),Separation);Result->SetNumberField(TEXT("hold_max_drift_cm"),Drift);Result->SetNumberField(TEXT("armed_peak_tilt_deg"),PeakTilt);Result->SetNumberField(TEXT("peak_altitude_cm"),PeakAltitude);Result->SetArrayField(TEXT("checkpoints"),SwarmCheckpoints);
    FString Text;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text));AttachVerificationIdentity(Text);
    FFileHelper::SaveStringToFile(Text,*SmokeResultFile);bSmokeDone=true;FPlatformMisc::RequestExitWithStatus(false,Valid?0:1);
}
void AImperialMatchManager::AngleSmokeTick()
{
    // Explicit initial-pose and non-USB control fixture. All later movement is
    // motor forces/Chaos. Vertical compensation isolates the attitude test.
    if(SmokePhase==0&&Elapsed>.5)
    {
        for(int32 I=0;I<Drones.Num();++I)
        {
            auto* D=Drones[I].Get();D->ResetDrone();D->SetAngleVerificationInput(FVector::ZeroVector,0);
            D->GetBodyMesh()->SetWorldLocationAndRotation(FVector(Homes[I].X,Homes[I].Y,180),FRotator(20,I<5?0:180,15),false,nullptr,ETeleportType::TeleportPhysics);
            D->SetArmed(true);
        }
        SmokePhase=1;AnglePhaseStart=Elapsed;
    }
    if(SmokePhase==0)return;
    float WorstTilt=0;bool Airborne=true;
    for(auto& D:Drones)
    {
        const FVector Velocity=D->GetBodyMesh()->GetPhysicsLinearVelocity()/100.;
        const double Acceleration=FMath::Clamp((1.8-D->GetAltitudeMeters())*3.-Velocity.Z*2.,-3.,3.);
        const double Thrust=.9*(9.81+Acceleration)/(4*FMath::Max(.45,D->GetBodyMesh()->GetUpVector().Z));
        const double VoltageFactor=FMath::Clamp(FMath::Square(D->GetBatteryVoltage()/14.8),.2,1.5);
        const float PWM=FMath::Sqrt(FMath::Clamp((Thrust/VoltageFactor-.12)/7.88,0.,1.));
        D->SetAngleVerificationInput(SmokePhase==2?FVector(0,.5,0):FVector::ZeroVector,PWM);
        WorstTilt=FMath::Max(WorstTilt,D->GetTiltDegrees());
        Airborne&=D->GetAltitudeMeters()>.7&&D->GetAltitudeMeters()<3.7;
    }
    AnglePeak=FMath::Max(AnglePeak,WorstTilt);
    const double Age=Elapsed-AnglePhaseStart;
    if(SmokePhase==1&&Age>1.8){bAngleInitialLevel=WorstTilt<3&&Airborne;SmokePhase=2;AnglePhaseStart=Elapsed;}
    else if(SmokePhase==2)
    {
        AngleInputPeak=FMath::Max(AngleInputPeak,WorstTilt);
        if(Age>.65){SmokePhase=3;AnglePhaseStart=Elapsed;}
    }
    else if(SmokePhase==3&&Age>1.8)
    {
        AngleReleaseWorst=WorstTilt;bAngleReleasedLevel=WorstTilt<3&&Airborne;
        const bool Valid=bAngleInitialLevel&&bAngleReleasedLevel&&AngleInputPeak>8&&AngleInputPeak<40&&AnglePeak<45&&Drones.Num()==10;
        auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("ok"),Valid);
        Result->SetNumberField(TEXT("drones"),Drones.Num());Result->SetBoolField(TEXT("initial_self_level"),bAngleInitialLevel);
        Result->SetNumberField(TEXT("stick_tilt_peak_deg"),AngleInputPeak);Result->SetNumberField(TEXT("overall_tilt_peak_deg"),AnglePeak);
        Result->SetNumberField(TEXT("released_worst_tilt_deg"),AngleReleaseWorst);Result->SetBoolField(TEXT("released_self_level"),bAngleReleasedLevel);
        Result->SetStringField(TEXT("input_source"),TEXT("explicit non-USB verification fixture; vertical compensation isolates angle control"));
        FString Text;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text));AttachVerificationIdentity(Text,SmokeMinDt,SmokeMaxDt);
        FFileHelper::SaveStringToFile(Text,*(SmokeResultFile.IsEmpty()?FPaths::Combine(ImperialUserData::Directory(),TEXT("angle-smoke.json")):SmokeResultFile));
        ExportTelemetry();bSmokeDone=true;UE_LOG(LogTemp,Display,TEXT("IMPERIAL_ANGLE_SMOKE %s"),*Text);
        FPlatformMisc::RequestExitWithStatus(false,Valid?0:1);
    }
}
void AImperialMatchManager::SmokeTick()
{
    auto* D=GetDrone(0);if(!D)return;
    if(Elapsed>1&&SmokePhase==0){D->SetAutonomousEnabled(true);D->SetAutonomousTarget(FVector(-550,-300,220));SmokePhase=1;}
    if(SmokePhase==1){if(Elapsed>9)D->SetAutonomousTarget(FVector(-350,-150,220));SmokeMaxHeight=FMath::Max(SmokeMaxHeight,(float)D->GetActorLocation().Z);if(Elapsed>12)SmokeMinHeight=FMath::Min(SmokeMinHeight,(float)D->GetActorLocation().Z);}
    if(Elapsed>20&&SmokePhase==1){SmokeHoverError=FVector::Distance(D->GetActorLocation(),FVector(-350,-150,220));bool Stable=SmokeHoverError<70&&SmokeMaxHeight<450&&SmokeMinHeight>120;D->SetAutonomousEnabled(false);D->SetArmed(false);SmokePhase=Stable?2:9;UE_LOG(LogTemp,Display,TEXT("IMPERIAL_HOVER error_cm=%.2f max_z=%.2f min_z=%.2f stable=%d"),SmokeHoverError,SmokeMaxHeight,SmokeMinHeight,Stable);}
    if(Elapsed>24&&SmokePhase==2){bool Floor=D->GetActorLocation().Z>15&&D->GetActorLocation().Z<30;D->ResetDrone();D->GetBodyMesh()->SetPhysicsLinearVelocity(FVector(-2500,0,0));SmokePhase=Floor?3:9;}
    if(Elapsed>27&&SmokePhase==3)
    {
        bSmokeFloorWall=D->GetActorLocation().X>-805&&D->GetActorLocation().Z>15&&D->GetActorLocation().Z<30;
        // Explicit collision fixture: one initial placement and a 5 m/s net impact.
        // No position correction is used during subsequent flight or contact.
        D->ResetDrone();D->GetBodyMesh()->SetWorldLocationAndRotation(FVector(0,-200,220),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
        D->GetBodyMesh()->SetPhysicsLinearVelocity(FVector(0,-500,0));SmokeNetMinY=-200;SmokePhase=4;
    }
    if(SmokePhase==4)
    {
        SmokeNetPeak=FMath::Max(SmokeNetPeak,D->GetNetDeflection());SmokeNetMinY=FMath::Min(SmokeNetMinY,(float)D->GetActorLocation().Y);
        SmokeNetRebound=FMath::Max(SmokeNetRebound,(float)D->GetBodyMesh()->GetPhysicsLinearVelocity().Y);
        if(auto* N=Cast<AImperialElasticNet>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialElasticNet::StaticClass())))SmokeNetVisualPeak=FMath::Max(SmokeNetVisualPeak,N->GetMaxDeflection());
        if(Elapsed>30)
        {
            bSmokeNet=SmokeNetPeak>.001f&&SmokeNetPeak<=.2001f&&SmokeNetVisualPeak>.001f&&SmokeNetVisualPeak<=.2001f&&SmokeNetRebound>5&&SmokeNetMinY>-351;
            UE_LOG(LogTemp,Display,TEXT("IMPERIAL_NET force_deflection_m=%.4f visual_deflection_m=%.4f rebound_cm_s=%.2f min_y_cm=%.2f pass=%d"),SmokeNetPeak,SmokeNetVisualPeak,SmokeNetRebound,SmokeNetMinY,bSmokeNet);
            Rules.Reset();D->ResetDrone();D->GetBodyMesh()->SetWorldLocationAndRotation(FVector(500,0,300),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
            D->SetAutonomousEnabled(true);D->SetAutonomousTarget(FVector(500,0,300));SmokePhase=5;
        }
    }
    if(SmokePhase==5)
    {
        if(Elapsed>33)D->SetAutonomousTarget(FVector(725,0,300));
        if(D->GetActorLocation().X>610&&D->GetActorLocation().X<690)UE_LOG(LogTemp,Display,TEXT("IMPERIAL_GOAL_TRACE elapsed=%.3f pose_cm=%s score=%d offside=%d"),Elapsed,*D->GetActorLocation().ToString(),Rules.Score(0),Rules.IsOffside(0));
    }
    if(Elapsed<43)return;bSmokeDone=true;
    bSmokeGoal=SmokePhase==5&&Rules.Score(0)==1&&Rules.Score(1)==0&&Rules.IsOffside(0)&&D->GetActorLocation().X>678;
    const bool CSV=ExportTelemetry();const bool Valid=SmokePhase==5&&bSmokeFloorWall&&bSmokeNet&&bSmokeGoal&&Drones.Num()==10&&CSV;
    FString Result=FString::Printf(TEXT("{\"ok\":%s,\"drones\":%d,\"hover_error_cm\":%.3f,\"hover_max_z_cm\":%.3f,\"hover_min_z_cm\":%.3f,\"floor_wall_contact\":%s,\"net_contact\":%s,\"net_peak_m\":%.5f,\"net_visible_peak_m\":%.5f,\"net_rebound_mps\":%.4f,\"striker_physical_goal\":%s,\"blue_score\":%d,\"csv_export\":%s}\n"),Valid?TEXT("true"):TEXT("false"),Drones.Num(),SmokeHoverError,SmokeMaxHeight,SmokeMinHeight,bSmokeFloorWall?TEXT("true"):TEXT("false"),bSmokeNet?TEXT("true"):TEXT("false"),SmokeNetPeak,SmokeNetVisualPeak,SmokeNetRebound/100,bSmokeGoal?TEXT("true"):TEXT("false"),Rules.Score(0),CSV?TEXT("true"):TEXT("false"));
    AttachVerificationIdentity(Result,SmokeMinDt,SmokeMaxDt);
    FFileHelper::SaveStringToFile(Result,*(SmokeResultFile.IsEmpty()?FPaths::Combine(ImperialUserData::Directory(),TEXT("physics-smoke.json")):SmokeResultFile));
    UE_LOG(LogTemp,Display,TEXT("IMPERIAL_PHYSICS_SMOKE_%s %s"),Valid?TEXT("PASS"):TEXT("FAIL"),*Result);
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialFlightTrace")))
    {
        if(auto* Trace=Cast<AImperialFlightTrace>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialFlightTrace::StaticClass())))
        {
            Trace->RequestFinish(Elapsed,Valid);
            return;
        }
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialSolverProbe"))&&FParse::Param(FCommandLine::Get(),TEXT("ImperialSolverProbePassthrough")))
    {
        if(auto* Probe=Cast<AImperialSolverProbe>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialSolverProbe::StaticClass())))
        {
            Probe->CompletePassthrough(Elapsed,[Valid](bool ProbeOk){FPlatformMisc::RequestExitWithStatus(false,Valid&&ProbeOk?0:1);});
            return;
        }
    }
    FPlatformMisc::RequestExitWithStatus(false,Valid?0:1);
}
