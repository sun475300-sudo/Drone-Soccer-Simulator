#include "ImperialResponseVerifier.h"
#include "ImperialRadioSubsystem.h"
#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "ImperialHUD.h"
#include "ImperialBuildInfo.h"
#include "ImperialUserData.h"
#include "SoccerDronePawn.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "UnrealClient.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"

AImperialResponseVerifier::AImperialResponseVerifier(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;}
void AImperialResponseVerifier::Check(const TCHAR* Name,bool Passed,double Value)
{auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("check"),Name);R->SetBoolField(TEXT("ok"),Passed);R->SetNumberField(TEXT("value"),Value);Checks.Add(MakeShared<FJsonValueObject>(R));bOk&=Passed;}
void AImperialResponseVerifier::Finish()
{
    auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("ok"),bOk);R->SetStringField(TEXT("case"),Case);R->SetArrayField(TEXT("checks"),Checks);R->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());
    R->SetStringField(TEXT("scope"),TEXT("Explicit synthetic engine/HUD response test with isolated settings and actual motor forces. No physical USB button or calibration input."));
    FString File,Json;FParse::Value(FCommandLine::Get(),TEXT("ImperialResponseResult="),File);FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Json));
    const bool Saved=FFileHelper::SaveStringToFile(Json,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    SetActorTickEnabled(false);FPlatformMisc::RequestExitWithStatus(false,bOk&&Saved?0:1);
}
void AImperialResponseVerifier::Tick(float Dt)
{
    Super::Tick(Dt);Age+=Dt;
    if(Age>12){Check(TEXT("bounded runtime"),false);Finish();return;}
    auto* M=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));
    auto* PC=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    auto* H=PC?Cast<AImperialHUD>(PC->GetHUD()):nullptr;auto* R=GetGameInstance()->GetSubsystem<UImperialRadioSubsystem>();auto* D=M?M->GetSelectedDrone():nullptr;
    if(Stage==2&&D)
    {
        const double Roll=FMath::Abs(D->GetManualTargetRateDegrees().X);
        bRollRateInRange&=FMath::IsFinite(Roll)&&Roll<=30.001;
        PeakRollRate=FMath::Max(PeakRollRate,Roll);
    }
    if(!H||!R||!D||Age<.6)return;
    if(Stage==0)
    {
        FParse::Value(FCommandLine::Get(),TEXT("ImperialResponseCase="),Case);
        const bool Reload=Case==TEXT("reload")||Case==TEXT("replace")||Case==TEXT("replacefail");
        Check(TEXT("cold start USB disabled and disarmed"),!R->IsEnabled()&&!D->IsArmed());
        Check(TEXT("restored or default response"),FMath::IsNearlyEqual(R->GetPilotResponse().Expo[0],Reload?.2:0.,1e-8)&&R->GetPilotResponse().RateDegrees[0]==(Reload?30:180));
        Check(TEXT("ARM unassigned on isolated profile"),R->GetArmButton()==INDEX_NONE);
        if(Case==TEXT("invalid")){Finish();return;}
        if(Case==TEXT("replace")||Case==TEXT("replacefail"))
        {
            auto Candidate=R->GetPilotResponse();Candidate.Expo[0]=.25;
            const bool Expected=Case==TEXT("replace");
            Check(TEXT("existing settings replacement result"),R->SavePilotResponse(Candidate,-1,FString(),false)==Expected);
            Check(TEXT("live response changes only on successful replacement"),FMath::IsNearlyEqual(R->GetPilotResponse().Expo[0],Expected?.25:.2,1e-8));
            Finish();return;
        }
        if(Case==TEXT("savefail"))
        {auto Candidate=R->GetPilotResponse();Candidate.Expo[0]=.5;Check(TEXT("failed write leaves live response unchanged"),!R->SavePilotResponse(Candidate,-1,FString(),true)&&R->GetPilotResponse().Expo[0]==0);Finish();return;}
        if(!Reload)
        {
            if(!H->IsRadioPanelOpen())H->ToggleRadioPanel();
            H->NotifyHitBoxClick(TEXT("Radio_Response"));H->NotifyHitBoxClick(TEXT("Response_Advanced"));H->NotifyHitBoxClick(TEXT("Response_ExpoUp_0"));H->NotifyHitBoxClick(TEXT("Response_Cancel"));
            Check(TEXT("cancel does not change response or save"),R->GetPilotResponse().Expo[0]==0&&!IFileManager::Get().FileExists(*ImperialUserData::Path(TEXT("Config/PilotResponse.json"))));
            H->NotifyHitBoxClick(TEXT("Radio_Response"));
            H->NotifyHitBoxClick(TEXT("Response_Advanced"));
            for(int I=0;I<4;++I)H->NotifyHitBoxClick(TEXT("Response_ExpoUp_0"));
            for(int I=0;I<8;++I)H->NotifyHitBoxClick(TEXT("Response_ExpoUp_2"));
            for(int I=0;I<10;++I)H->NotifyHitBoxClick(TEXT("Response_RateDown_0"));
            for(int I=0;I<4;++I)H->NotifyHitBoxClick(TEXT("Response_RateUp_2"));
            H->NotifyHitBoxClick(TEXT("Response_ArmMode"));H->NotifyHitBoxClick(TEXT("Response_Save"));
            Check(TEXT("HUD saves response and switch mode"),FMath::IsNearlyEqual(R->GetPilotResponse().Expo[0],.2,1e-8)&&R->GetPilotResponse().RateDegrees[0]==30&&!R->IsArmToggle());
        }
        Check(TEXT("raw calibration never created"),!IFileManager::Get().FileExists(*R->GetProfilePath()));
        auto Bad=R->GetPilotResponse();Bad.Expo[0]=1;
        Check(TEXT("out of range settings rejected"),!R->SavePilotResponse(Bad,-1,FString(),true));
        Check(TEXT("unknown controller binding rejected"),!R->SavePilotResponse(R->GetPilotResponse(),0,TEXT("unconnected-test-device"),true));
        FString Capture;FParse::Value(FCommandLine::Get(),TEXT("ImperialResponseCapture="),Capture);
        if(!Capture.IsEmpty())
        {if(!H->IsRadioPanelOpen())H->ToggleRadioPanel();H->NotifyHitBoxClick(TEXT("Radio_Response"));FScreenshotRequest::RequestScreenshot(Capture,true,false);Stage=3;Age=0;return;}
        if(H->IsRadioPanelOpen())H->ToggleRadioPanel();Stage=1;Age=0;return;
    }
    if(Stage==3){if(Age>2)Finish();return;}
    if(Stage==1)
    {
        D->ResetDrone();D->SetActorLocation(FVector(0,0,150),false,nullptr,ETeleportType::TeleportPhysics);D->SetActorRotation(FRotator::ZeroRotator,ETeleportType::TeleportPhysics);
        D->GetBodyMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);D->GetBodyMesh()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        D->SetAngleVerificationInput(FVector::ZeroVector,0);Check(TEXT("explicit zero-throttle ARM"),D->SetArmed(true));
        Check(TEXT("armed editing refused"),!R->SavePilotResponse(ImperialInput::PilotResponse{},-1,FString(),true));
        D->SetAngleVerificationInput(FVector(.5,0,.5),.55);Stage=2;Age=0;return;
    }
    if(Stage==2)
    {
        const FVector Rate=D->GetManualTargetRateDegrees();
        Check(TEXT("actual yaw target reflects Expo and selected rate"),FMath::Abs(Rate.Z-84)<.01,Rate.Z);
        Check(TEXT("actual roll target reaches and obeys selected ANGLE rate throughout flight"),bRollRateInRange&&PeakRollRate>29.9,PeakRollRate);
        Check(TEXT("real motor forces present"),D->GetTotalThrustNewtons()>0,D->GetTotalThrustNewtons());
        Check(TEXT("ANGLE tilt limit retained"),D->GetFlightModeName()==TEXT("ANGLE")&&D->GetAngleLimitDegrees()==35);
        Check(TEXT("raw throttle unmodified"),FMath::Abs(D->GetThrottle()-.55)<1e-6);
        D->SetArmed(false);Finish();
    }
}
