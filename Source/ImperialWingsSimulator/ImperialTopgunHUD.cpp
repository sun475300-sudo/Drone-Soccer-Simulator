#include "ImperialHUD.h"
#include "ImperialMatchManager.h"
#include "ImperialTopgunCourse.h"
void AImperialHUD::DrawTopgun(AImperialMatchManager& M,bool Compact)
{
    const FLinearColor Cyan(.1f,.8f,1),White(.9f,.95f,1),Muted(.55f,.69f,.76f),Amber(1,.7f,.2f);
    const auto* Course=M.GetTopgun();
    if(Compact)
    {
        if(Course){Panel(24,118,640,80);Text(TEXT("TOPGUN S-01 / ")+Course->Status(),40,133,15,Cyan);Text(Course->IsDiagnostic()?TEXT("AUTOMATED TEST / NOT A PILOT RECORD"):TEXT("LOCAL PRACTICE TIMING / LAND BEFORE RETRY"),40,165,11,Muted);}return;
    }
    Panel(730,798,466,128);
    if(!Course)
    {
        Button(TEXT("TopgunStart"),TEXT("TOPGUN / TWO-PYLON S-COURSE"),744,810,438,36);
        Text(TEXT("LOS manual / clean run / ordered checkpoints"),747,862,12,White);
        Text(TEXT("Land and disarm every aircraft before starting."),747,892,11,Muted);return;
    }
    Button(TEXT("TopgunStart"),TEXT("RETRY / RESET COURSE"),744,810,230,33);
    Button(TEXT("TopgunExit"),TEXT("BACK TO SOCCER"),982,810,200,33);
    Text(Fit(Course->Status(),14,430),747,855,14,Course->GetRun().State==ImperialTopgun::Phase::Invalid?Amber:Cyan);
    Text(Course->IsDiagnostic()?TEXT("AUTOMATED TEST / NOT A PILOT RECORD"):TEXT("LOCAL PRACTICE TIME / NO RANKING"),747,885,11,Muted);
    Text(TEXT("Land manually. Retry clears the current time."),747,905,10,Muted);
}
