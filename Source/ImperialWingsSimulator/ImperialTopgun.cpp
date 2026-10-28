#include "ImperialMatchManager.h"
#include "ImperialTopgunCourse.h"
#include "ImperialPlayerController.h"
#include "SoccerDronePawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
bool AImperialMatchManager::StartTopgun()
{
    if(Rules.GetPhase()!=drone_soccer::Phase::Practice||IsAIPracticeActive()){Log(TEXT("Finish the current match or AI session first."));return false;}
    if(const auto* PC=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));PC&&PC->IsReadOnlyView()){Log(TEXT("Select a pilot view before starting Topgun."));return false;}
    for(const auto& D:Drones)if(D->IsArmed()||D->GetAltitudeMeters()>.35||D->GetFlightSpeed()>.35){Log(TEXT("Land and disarm all aircraft before starting or retrying Topgun."));return false;}
    ResetMatch();SelectDrone(0);ChangeView(1);TopgunCourse=GetWorld()->SpawnActor<AImperialTopgunCourse>();
    if(!TopgunCourse){Log(TEXT("Course could not be created."));return false;}
    TopgunCourse->Initialize(GetDrone(0),bTopgunDiagnostic);Log(TEXT("Topgun S-01 ready. Arm manually, pass START then gates 1, 2, 3 and FINISH. Contact invalidates the run."));return true;
}
bool AImperialMatchManager::ExitTopgun()
{
    if(!TopgunCourse)return true;
    auto* D=GetDrone(0);if(D->IsArmed()||D->GetAltitudeMeters()>.35||D->GetFlightSpeed()>.35){Log(TEXT("Land and disarm before leaving Topgun, or use RESET for an immediate reset."));return false;}
    ResetMatch();return true;
}
double AImperialMatchManager::GetTopgunElapsed()const{return TopgunCourse?TopgunCourse->GetRun().Elapsed:0;}
