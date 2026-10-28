#include "ImperialHUD.h"
#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "ImperialRadioSubsystem.h"
#include "ImperialLocalization.h"
#include "Engine/Canvas.h"

void AImperialHUD::ToggleAIPracticePanel()
{
    if(bRadioPanelOpen)ToggleRadioPanel();
    bAIPracticePanel=!bAIPracticePanel;
    bAIResultPage=false;
    if(auto* M=FindMatch();M&&bAIPracticePanel)
    {
        M->ManualSelected();
        const auto S=M->GetAISettings();AIDraftCount=S.Count;AIDraftDifficulty=S.Difficulty;AIDraftSeed=S.Seed;bAIDraftDuel=S.Duel;
    }
    if(auto* PC=Cast<AImperialPlayerController>(PlayerOwner))PC->FlushPilotContextKeys();
}
void AImperialHUD::DrawAIPracticePanel(AImperialMatchManager& M)
{
    const float ModalScale=FMath::Max(Scale,FMath::Min(1.f,FMath::Min((Canvas->SizeX-32.f)/960.f,(Canvas->SizeY-32.f)/748.f)));
    const TGuardValue<float> ScaleGuard(Scale,ModalScale);
    const TGuardValue<FVector2D> OriginGuard(Origin,FVector2D(Canvas->SizeX*.5f-800.f*Scale,Canvas->SizeY*.5f-500.f*Scale));
    const TGuardValue<float> ButtonSizeGuard(ButtonTextSize,16);
    const FLinearColor White(.89f,.96f,1),Muted(.55f,.69f,.76f),Cyan(.1f,.75f,.96f),Amber(1,.71f,.24f);
    DrawRect(FLinearColor(0,0,0,.82f),0,0,Canvas->SizeX,Canvas->SizeY);Panel(320,126,960,748);Rect(320,126,4,748,Cyan);ButtonPriority=110;
    Button(TEXT("Radio_Language"),ImperialLocalization::IsKorean()?TEXT("English"):TEXT("한국어"),1084,148,168,32);
    if(bAIResultPage&&M.GetAIResult().Exists()){DrawAIResultPanel(M);ButtonPriority=10;return;}
    Text(bAIDraftDuel?TEXT("AI DUEL / TRAINING VARIANT"):TEXT("AI DEFENCE PRACTICE"),348,155,28,White);
    Text(TEXT("Solo goal training  /  Blue striker  /  LOS + ANGLE"),348,197,15,Cyan);
    Button(TEXT("AI_Defence"),TEXT("DEFENCE PRACTICE"),348,228,280,32,!bAIDraftDuel);
    Button(TEXT("AI_Duel"),TEXT("DUEL / 3 MINUTES"),640,228,280,32,bAIDraftDuel);
    Button(TEXT("AI_Count3"),bAIDraftDuel?TEXT("3 AI: STRIKER + 2 DEFENDERS"):TEXT("3 DEFENDERS"),348,277,280,42,AIDraftCount==3);
    Button(TEXT("AI_Count4"),bAIDraftDuel?TEXT("4 AI: STRIKER + 3 DEFENDERS"):TEXT("4 DEFENDERS"),640,277,280,42,AIDraftCount==4);
    if(M.GetAIResult().Exists())Button(TEXT("AI_Last"),TEXT("LAST RESULT"),932,277,320,42);
    Text(TEXT("DIFFICULTY"),348,347,16,Muted);
    Button(TEXT("AI_Easy"),TEXT("BEGINNER"),348,375,280,42,AIDraftDifficulty==0);
    Button(TEXT("AI_Medium"),TEXT("INTERMEDIATE"),640,375,280,42,AIDraftDifficulty==1);
    Button(TEXT("AI_Hard"),TEXT("ADVANCED"),932,375,320,42,AIDraftDifficulty==2);
    Text(TEXT("Slower reaction / patrol"),348,434,14,Muted);
    Text(TEXT("Faster reaction / tracking"),932,434,14,Muted);
    Text(FString::Printf(TEXT("FORMATION SEED   %u"),AIDraftSeed),348,482,18,White);
    Button(TEXT("AI_Seed"),TEXT("NEW RANDOM FORMATION"),850,476,402,38);
    Text(TEXT("Same seed repeats tactical targets. Physics / collisions may differ."),348,531,16,Muted);
    Text(bAIDraftDuel?TEXT("You: blue striker. Red striker attacks; red defenders guard. Not official 5v5."):TEXT("Only red defenders launch automatically. You arm the blue striker manually."),348,576,16,White);
    Text(bAIDraftDuel?TEXT("Most valid goals in 3 minutes wins. Retreat to your own half after scoring."):TEXT("3-minute practice limit. STOP AI lands defenders; your throttle stays manual."),348,605,16,White);
    Text(TEXT("Settings lock while running. Official match play remains manual."),348,634,16,Muted);
    Text(M.IsAIPracticeActive()?Fit(M.GetAIStatus(),16,900):TEXT("Ready: all aircraft must be landed and disarmed before START."),348,679,16,Cyan);
    StatusText(LanguageMessage.IsEmpty()?M.GetNotice():LanguageMessage,348,711,895,Amber);
    if(M.IsAIPracticeActive())Button(TEXT("AI_Stop"),M.IsAILanding()?TEXT("LANDING IN PROGRESS"):TEXT("STOP AI / LAND OPPONENTS"),348,783,550,42,false,true);
    else Button(TEXT("AI_Start"),TEXT("START PRACTICE / RESET PADS & SCORE"),348,783,550,42);
    Button(TEXT("AI_Close"),TEXT("CLOSE"),922,783,330,42);
    Text(TEXT("Local solo training. Your blue striker always needs manual landing; STOP does not land it."),348,846,13,Muted);
    ButtonPriority=10;
}

void AImperialHUD::DrawAIResultPanel(AImperialMatchManager& M)
{
    const auto& R=M.GetAIResult();
    const FLinearColor White(.89f,.96f,1),Muted(.55f,.69f,.76f),Cyan(.1f,.75f,.96f),Amber(1,.71f,.24f);
    Text(R.Settings.Duel?TEXT("AI DUEL / LAST RESULT"):TEXT("AI PRACTICE / LAST RESULT"),348,155,27,White);
    Text(FString::Printf(TEXT("%d AI  /  %s  /  SEED %u"),R.Settings.Count,R.Settings.Difficulty==0?TEXT("BEGINNER"):(R.Settings.Difficulty==1?TEXT("INTERMEDIATE"):TEXT("ADVANCED")),R.Settings.Seed),348,201,15,Cyan);
    const auto ReasonLabel=[](const FString& Reason)->const TCHAR*
    {
        if(Reason==TEXT("manual_stop"))return TEXT("STOP REQUESTED");if(Reason==TEXT("time_limit"))return TEXT("TIME LIMIT");
        if(Reason==TEXT("landed"))return TEXT("LANDED");if(Reason==TEXT("application_exit"))return TEXT("APP CLOSED");
        if(Reason==TEXT("defender_grounded"))return TEXT("AI OVERTURNED");if(Reason==TEXT("defender_unavailable"))return TEXT("AI DISABLED");
        if(Reason==TEXT("recovery_failed"))return TEXT("RECOVERY FAILED");if(Reason==TEXT("reset"))return TEXT("RESET");return TEXT("MODE CHANGED");
    };
    Text(FString::Printf(TEXT("STOP: %s   /   FINISH: %s"),ReasonLabel(R.StopReason),ReasonLabel(R.CompletionReason)),348,234,16,R.CompletionReason==TEXT("landed")?White:Amber);
    Panel(348,275,280,95);Panel(640,275,280,95);Panel(932,275,320,95);
    Text(R.Settings.Duel?TEXT("BLUE : RED / VALID GOALS"):TEXT("VALID GOALS"),367,293,14,Muted);Text(R.Settings.Duel?FString::Printf(TEXT("%d : %d"),R.Goals,R.OpponentGoals):FString::FromInt(R.Goals),367,320,28,Cyan);
    Text(TEXT("PRACTICE TIME"),659,293,14,Muted);Text(FString::Printf(TEXT("%.1f s"),R.PracticeSeconds),659,320,28,White);
    Text(TEXT("AI LANDING TIME"),951,293,14,Muted);Text(FString::Printf(TEXT("%.1f s"),R.LandingSeconds),951,320,28,White);
    Text(TEXT("AIRCRAFT"),348,400,16,Muted);Text(TEXT("GUARD CONTACTS"),680,400,16,Muted);Text(TEXT("GUARD REMAINING"),990,400,16,Muted);
    for(int32 I=0;I<R.Aircraft.Num();++I)
    {
        const auto& A=R.Aircraft[I];const float Y=432+I*38;
        Text(A.Id==0?TEXT("BLUE STRIKER"):(A.Id==5?FString(TEXT("RED STRIKER")):FString::Printf(TEXT("RED DEFENDER %d"),A.Id-5)),348,Y,15,White);
        Text(FString::FromInt(A.GuardImpacts),680,Y,15,White);Text(FString::Printf(TEXT("%.1f%%"),A.GuardRemaining),990,Y,15,White);
    }
    Text(FString::Printf(TEXT("RECOVERY ATTEMPTS   %d"),R.RecoveryAttempts),348,615,16,Cyan);
    if(R.Settings.Duel)Text(R.Outcome(),990,615,17,Amber);
    Text(TEXT("Goals and guard values stop at the stop request. Landing is timed separately."),348,645,14,Muted);
    Text(TEXT("Contacts are per-aircraft registered impacts, not unique collision pairs or scoring attempts."),348,670,14,Muted);
    Text(R.bTestSession?TEXT("AUTOMATED TEST SESSION / no physical pilot evidence"):TEXT("Local practice record / no online ranking verification"),348,700,16,Amber);
    Text(FString::Printf(TEXT("%s   /   %s"),*R.BuildVersion,*R.EndedUtc),348,728,11,Muted);
    Text(R.bSaved?TEXT("SAVED: user data / TrainingResults / AI"):TEXT("NOT SAVED: retry, or discard this unsaved result before starting again."),348,752,16,R.bSaved?Cyan:Amber);
    if(!R.bSaved){Button(TEXT("AI_ResultSave"),TEXT("RETRY SAVE"),348,790,205,38);Button(TEXT("AI_ResultDiscard"),TEXT("DISCARD UNSAVED"),565,790,230,38,false,true);}
    Button(TEXT("AI_ResultBack"),TEXT("SETUP"),807,790,205,38);Button(TEXT("AI_Close"),TEXT("CLOSE"),1024,790,228,38);
    Text(Fit(LanguageMessage.IsEmpty()?M.GetNotice():LanguageMessage,13,900),348,845,13,LanguageMessage.IsEmpty()?Muted:Amber);
}
