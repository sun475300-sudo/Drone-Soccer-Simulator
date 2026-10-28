#include "ImperialGameMode.h"
#include "ImperialPlayerController.h"
#include "ImperialMatchManager.h"
#include "ImperialHUD.h"
#include "PrototypeArenaBuilder.h"
#include "ImperialPresentationEffects.h"
#include "ImperialRoleRibbons.h"
#include "ImperialInputSafetyVerifier.h"
#include "ImperialTimeVerifier.h"
#include "ImperialRadioModeVerifier.h"
#include "ImperialRadioUXVerifier.h"
#include "ImperialFirstFlightGuide.h"
#include "ImperialAIMotionVerifier.h"
#include "ImperialFirstFlightVerifier.h"
#include "ImperialFlightRecordingVerifier.h"
#include "ImperialPowerVerifier.h"
#include "ImperialThrottleVerifier.h"
#include "ImperialSolverProbe.h"
#include "ImperialFlightTrace.h"
#include "ImperialResponseVerifier.h"
#include "ImperialAIPracticeVerifier.h"
#include "ImperialAIResultsVerifier.h"
#include "ImperialAIRecoveryVerifier.h"
#include "ImperialAIDuelVerifier.h"
#include "ImperialSolverBridgeVerifier.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
AImperialGameMode::AImperialGameMode()
{
    DefaultPawnClass=nullptr; PlayerControllerClass=AImperialPlayerController::StaticClass(); HUDClass=AImperialHUD::StaticClass();
}
void AImperialGameMode::StartPlay()
{
    GetWorld()->SpawnActor<APrototypeArenaBuilder>();
    GetWorld()->SpawnActor<AImperialMatchManager>();
    GetWorld()->SpawnActor<AImperialFirstFlightGuide>();
    GetWorld()->SpawnActor<AImperialPresentationEffects>();
    GetWorld()->SpawnActor<AImperialRoleRibbons>();
    // The bridge validates conflicting diagnostic flags itself, before another verifier can start.
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialFlightRecordingSmoke")))
    {
        GetWorld()->SpawnActor<AImperialFlightRecordingVerifier>();
    }
    else if(FParse::Param(FCommandLine::Get(),TEXT("ImperialSolverBridgeSmoke")))
    {
        GetWorld()->SpawnActor<AImperialSolverBridgeVerifier>();
    }
    else
    {
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialAIMotionSmoke")))GetWorld()->SpawnActor<AImperialAIMotionVerifier>();
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialFirstFlightSmoke")))GetWorld()->SpawnActor<AImperialFirstFlightVerifier>();
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialAIDuelSmoke")))GetWorld()->SpawnActor<AImperialAIDuelVerifier>();
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialAIRecoverySmoke")))GetWorld()->SpawnActor<AImperialAIRecoveryVerifier>();
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialAIResultsSmoke")))GetWorld()->SpawnActor<AImperialAIResultsVerifier>();
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialAIPracticeSmoke")))GetWorld()->SpawnActor<AImperialAIPracticeVerifier>();
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialResponseSmoke")))GetWorld()->SpawnActor<AImperialResponseVerifier>();
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialInputSmoke")))GetWorld()->SpawnActor<AImperialInputSafetyVerifier>();
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialTimeSmoke")))GetWorld()->SpawnActor<AImperialTimeVerifier>();
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialRadioUXSmoke")))GetWorld()->SpawnActor<AImperialRadioUXVerifier>();
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialRadioModeSmoke")))GetWorld()->SpawnActor<AImperialRadioModeVerifier>();
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialPowerSmoke")))GetWorld()->SpawnActor<AImperialPowerVerifier>();
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialThrottleSmoke")))GetWorld()->SpawnActor<AImperialThrottleVerifier>();
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialSolverProbe")))GetWorld()->SpawnActor<AImperialSolverProbe>();
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialFlightTrace")))GetWorld()->SpawnActor<AImperialFlightTrace>();
    }
    Super::StartPlay();
}
