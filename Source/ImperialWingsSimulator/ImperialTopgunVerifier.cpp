#include "ImperialTopgunVerifier.h"
#include "ImperialTopgunCourse.h"
#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "ImperialHUD.h"
#include "ImperialBuildInfo.h"
#include "SoccerDronePawn.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"
using namespace ImperialTopgun;
AImperialTopgunVerifier::AImperialTopgunVerifier(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;}
void AImperialTopgunVerifier::Check(const TCHAR* Name,bool Passed,double Value)
{auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("case"),Case);R->SetStringField(TEXT("check"),Name);R->SetBoolField(TEXT("ok"),Passed);R->SetNumberField(TEXT("value"),Value);Checks.Add(MakeShared<FJsonValueObject>(R));bOk&=Passed;}
void AImperialTopgunVerifier::Finish()
{
    UGameplayStatics::SetGlobalTimeDilation(this,1);
    auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("ok"),bOk);R->SetArrayField(TEXT("checks"),Checks);R->SetArrayField(TEXT("samples"),Samples);R->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());R->SetNumberField(TEXT("min_game_dt_s"),MinDt);R->SetNumberField(TEXT("max_game_dt_s"),MaxDt);
    FString File,Json;FParse::Value(FCommandLine::Get(),TEXT("ImperialTopgunResult="),File);FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Json));const bool Saved=FFileHelper::SaveStringToFile(Json,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);SetActorTickEnabled(false);FPlatformMisc::RequestExitWithStatus(false,bOk&&Saved?0:1);
}
void AImperialTopgunVerifier::Tick(float Dt)
{
    Super::Tick(Dt);Age+=Dt;Total+=Dt;if(!bChangedClock){MinDt=FMath::Min(MinDt,static_cast<double>(Dt));MaxDt=FMath::Max(MaxDt,static_cast<double>(Dt));}
    if(Total>420){Check(TEXT("bounded Topgun fixture"),false);Finish();return;}
    auto* M=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));auto* PC=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));auto* H=PC?Cast<AImperialHUD>(PC->GetHUD()):nullptr;if(!M||!H||Age<.25)return;
    auto* D=M->GetDrone(0);auto* C=M->GetTopgun();
    if(Stage==0)
    {
        FParse::Value(FCommandLine::Get(),TEXT("ImperialTopgunCapture="),Capture);FParse::Value(FCommandLine::Get(),TEXT("ImperialTopgunCases="),CaseCount);CaseCount=FMath::Clamp(CaseCount,1,6);if(GEngine)GEngine->SetMaxFPS(Capture.IsEmpty()?0:60);
        M->ResetMatch();M->ChangeView(1);if(H->IsRadioPanelOpen())H->ToggleRadioPanel();H->NotifyHitBoxClick(TEXT("TopgunStart"));C=M->GetTopgun();
        Check(TEXT("native Topgun button selects manual disarmed blue LOS pilot"),C&&M->GetSelected()==0&&M->GetView()==1&&!D->IsArmed()&&!D->IsAutonomousEnabled());if(!C){Finish();return;}
        AddTickPrerequisiteActor(C);
        bool Geometry=true;for(int I=0;I<2;++I){auto* P=C->GetPylon(I);Geometry&=P&&P->GetCollisionEnabled()==ECollisionEnabled::QueryAndPhysics&&P->GetComponentLocation().Equals(FVector(I==0?-200:200,0,180),.1)&&P->Bounds.BoxExtent.Equals(FVector(35,35,180),.1);}
        Check(TEXT("two physical pylons have 70 cm diameter and 3.6 m height"),Geometry);
        M->SelectDrone(6);M->LaunchSwarm();M->HoldFleet();M->ReturnFleet();M->StartMatch();ImperialAI::Settings AI;AI.Duel=true;M->StartAIPractice(AI);
        bool Safe=M->GetSelected()==0&&M->GetTopgun()==C&&!M->IsAIPracticeActive();for(const auto& P:M->GetDrones())Safe&=!P->IsArmed();Check(TEXT("fleet official match AI and aircraft selection cannot replace course"),Safe);
        if(FParse::Param(FCommandLine::Get(),TEXT("ImperialTopgunControlSmoke")))
        {
            D->SetAutonomousEnabled(true);Check(TEXT("normal course refuses autonomous pilot assistance"),!M->IsTopgunDiagnostic()&&!D->IsAutonomousEnabled()&&!D->IsArmed());
            Check(TEXT("explicit manual arm is available"),D->SetArmed(true));Check(TEXT("armed retry and exit are rejected"),!M->StartTopgun()&&!M->ExitTopgun()&&M->GetTopgun()==C);
            D->SetArmed(false);Check(TEXT("disarmed exit restores soccer practice"),M->ExitTopgun()&&!M->IsTopgunActive()&&M->IsAutonomyAllowed());
            M->StartTopgun();M->ResetMatch();Check(TEXT("RESET removes course and leaves aircraft disarmed"),!M->IsTopgunActive()&&!D->IsArmed());Finish();return;
        }
        if(Case==0&&!M->IsRecordingFlight())M->ToggleFlightRecording();
        D->SetAutonomousEnabled(true);D->SetAutonomousTarget(FVector(-650,0,180));bSafe=true;LastSample=-1;Leg=0;Stage=1;Age=0;return;
    }
    if(!C){Check(TEXT("active course persists until explicit reset"),false);Finish();return;}
    const auto& R=C->GetRun();
    if(Stage==1)
    {
        for(int I=1;I<10;++I)bSafe&=!M->GetDrone(I)->IsArmed();bSafe&=M->GetScore(0)==0&&M->GetScore(1)==0&&!D->GetActorLocation().ContainsNaN()&&D->GetFlightSpeed()<12;
        if(Age-LastSample>=.25)
        {
            LastSample=Age;auto S=MakeShared<FJsonObject>();const FVector P=D->GetActorLocation()/100.;S->SetNumberField(TEXT("case"),Case);S->SetNumberField(TEXT("time"),Age);S->SetNumberField(TEXT("state"),static_cast<int>(R.State));S->SetNumberField(TEXT("reason"),static_cast<int>(R.Failure));S->SetNumberField(TEXT("next_gate"),R.Next);S->SetNumberField(TEXT("leg"),Leg);S->SetArrayField(TEXT("position"),{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)});Samples.Add(MakeShared<FJsonValueObject>(S));
        }
        static const FVector Targets[]={FVector(-650,0,180),FVector(-440,0,180),FVector(-260,-150,180),FVector(-140,-150,180),FVector(-60,0,180),FVector(60,0,180),FVector(140,150,180),FVector(260,150,180),FVector(440,0,180),FVector(560,0,180)};
        if(Case==0||R.Next==0)
        {
            if(Leg<9&&((Leg%2==0&&FVector::Dist(D->GetActorLocation(),Targets[Leg])<15&&D->GetFlightSpeed()<.35)||(Leg%2==1&&R.Next>Leg/2)))++Leg;
            D->SetAutonomousTarget(Targets[Leg]);
        }
        else if(Case==1){if(Leg<3){D->SetAutonomousTarget(FVector(-260,-270,180));if(FVector::Dist(D->GetActorLocation(),FVector(-260,-270,180))<15&&D->GetFlightSpeed()<.35)Leg=3;}else D->SetAutonomousTarget(FVector(-140,-270,180));}
        else if(Case==2)D->SetAutonomousTarget(FVector(-600,0,180));
        else if(Case==3)D->SetAutonomousTarget(FVector(-200,0,180));
        else if(Case==4&&!bChangedClock){UGameplayStatics::SetGlobalTimeDilation(this,.5f);bChangedClock=true;}
        else if(Case==5)D->SetAutonomousEnabled(false);
        if(R.State!=Phase::Finished&&R.State!=Phase::Invalid&&Age<100)return;
        Check(TEXT("inactive roster soccer scores and finite motion preserved"),bSafe);
        const Reason Expected[]={Reason::None,Reason::MissedGate,Reason::Reverse,Reason::Contact,Reason::Clock,Reason::Disarmed};
        Check(TEXT("real flight produces expected completion or rejection"),Case==0?(R.State==Phase::Finished&&R.Next==5&&R.Elapsed>5&&R.Elapsed<100):(R.State==Phase::Invalid&&R.Failure==Expected[Case]),static_cast<int>(R.Failure));
        if(Case==0){bool Splits=R.Split[0]==0;for(int I=1;I<5;++I)Splits&=R.Split[I]>R.Split[I-1];Check(TEXT("all four segment times and final time are ordered"),Splits&&R.Split[4]==R.Elapsed,R.Elapsed);}
        CompletedTime=R.Elapsed;UGameplayStatics::SetGlobalTimeDilation(this,1);
        // Diagnostic landing only; the normal course never arms or lands the pilot.
        if(D->IsAutonomousEnabled())D->SetAutonomousTarget(FVector(D->GetActorLocation().X,D->GetActorLocation().Y,12));Stage=2;Age=0;return;
    }
    if(Stage==2)
    {
        if(D->GetAltitudeMeters()>.32||D->GetFlightSpeed()>.35){if(Age<20)return;Check(TEXT("bounded physical landing"),false);}
        D->SetAutonomousEnabled(false);Check(TEXT("timing remains frozen through landing"),R.Elapsed==CompletedTime);Check(TEXT("pilot physically lands before motors stop"),D->GetAltitudeMeters()<.32&&!D->IsArmed());
        if(M->IsRecordingFlight())M->ToggleFlightRecording();
        if(!Capture.IsEmpty()){M->ChangeView(3);Stage=8;Age=0;return;}
        M->ResetMatch();Check(TEXT("explicit reset destroys course and restores practice"),!M->IsTopgunActive()&&M->IsAutonomyAllowed()&&!D->IsArmed());
        if(++Case>=CaseCount){Finish();return;}Stage=0;Age=0;return;
    }
    if(Stage==8&&Age>1){FScreenshotRequest::RequestScreenshot(Capture,true,false);Stage=9;Age=0;return;}
    if(Stage==9&&Age>1)Finish();
}
