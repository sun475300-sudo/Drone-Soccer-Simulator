#include "ImperialMatchManager.h"
#include "ImperialUserData.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

void AImperialMatchManager::OpenAIComparison()
{
    if(IsAIPracticeActive()){Log(TEXT("AI comparison is available after AI landing completes."));return;}
    AIComparison.bOpen=true;AIComparison.bShowOutcome=false;AIComparison.DetailsPage=0;
    RefreshAIComparison();
}
void AImperialMatchManager::CloseAIComparison()
{
    AIComparison.bOpen=false;AIComparison.bShowOutcome=false;
}
bool AImperialMatchManager::RefreshAIComparison()
{
    if(!AIComparison.bOpen||IsAIPracticeActive())return false;
    // Read only on explicit open/refresh, never from a Draw/Tick call.
    FString ReferenceName,CandidateName;
    if(AIComparison.Records.IsValidIndex(AIComparison.ReferenceIndex))ReferenceName=AIComparison.Records[AIComparison.ReferenceIndex].FileName;
    if(AIComparison.Records.IsValidIndex(AIComparison.CandidateIndex))CandidateName=AIComparison.Records[AIComparison.CandidateIndex].FileName;
    AIComparison.Records.Reset();AIComparison.ReferenceIndex=AIComparison.CandidateIndex=INDEX_NONE;
    const FString Dir=ImperialUserData::Path(TEXT("TrainingResults/AI"));TArray<FString> Files;
    int32 TotalFiles=0;
    // Keep at most 101 names in memory even when a history directory is large.
    // Filename order is deterministic; it does not authenticate record chronology.
    const bool bListed=IFileManager::Get().IterateDirectory(*Dir,[&](const TCHAR* Path,bool bDirectory)
    {
        if(bDirectory||!FPaths::GetExtension(Path).Equals(TEXT("json"),ESearchCase::IgnoreCase))return true;
        TotalFiles=TotalFiles<MAX_int32?TotalFiles+1:MAX_int32;
        Files.Add(FPaths::GetCleanFilename(Path));
        Files.Sort([](const FString& A,const FString& B){return A>B;});
        if(Files.Num()>ImperialAIComparison::MaxListedReports)Files.Pop();
        return true;
    });
    AIComparison.HiddenFileCount=FMath::Max(0,TotalFiles-Files.Num());
    int32 Invalid=0;
    for(int32 I=0;I<FMath::Min(Files.Num(),ImperialAIComparison::MaxListedReports);++I)
    {
        FImperialAIComparisonRecord Record;ImperialAIComparison::Load(FPaths::Combine(Dir,Files[I]),Record);
        if(!Record.bValid)++Invalid;
        AIComparison.Records.Add(MoveTemp(Record));
        if(AIComparison.Records.Last().bValid&&Files[I]==ReferenceName)AIComparison.ReferenceIndex=I;
        if(AIComparison.Records.Last().bValid&&Files[I]==CandidateName)AIComparison.CandidateIndex=I;
    }
    if(!bListed)AIComparison.LoadNotice=TEXT("AI result folder could not be listed. Existing reports were not changed.");
    else if(Files.IsEmpty())AIComparison.LoadNotice=TEXT("No saved AI results in this user profile.");
    else AIComparison.LoadNotice=FString::Printf(TEXT("Read-only listed %d reports / unreadable %d / additional reports not listed %d"),AIComparison.Records.Num(),Invalid,AIComparison.HiddenFileCount);
    AIComparison.Page=FMath::Clamp(AIComparison.Page,0,FMath::Max(0,(AIComparison.Records.Num()-1)/FImperialAIComparisonView::PageSize));
    AIComparison.DetailsPage=0;UpdateAIComparisonOutcome();return true;
}
void AImperialMatchManager::MoveAIComparisonPage(int32 Delta)
{
    if(!AIComparison.bOpen||IsAIPracticeActive())return;
    const int32 Last=FMath::Max(0,(AIComparison.Records.Num()-1)/FImperialAIComparisonView::PageSize);
    AIComparison.Page=FMath::Clamp(AIComparison.Page+FMath::Clamp(Delta,-1,1),0,Last);
}
void AImperialMatchManager::SelectAIComparisonRecord(int32 Slot,int32 Index)
{
    if(!AIComparison.bOpen||IsAIPracticeActive()||(Slot!=0&&Slot!=1)||!AIComparison.Records.IsValidIndex(Index)||!AIComparison.Records[Index].bValid)return;
    if(Slot==0)AIComparison.ReferenceIndex=Index;else AIComparison.CandidateIndex=Index;
    AIComparison.DetailsPage=0;UpdateAIComparisonOutcome();
}
void AImperialMatchManager::ShowAIComparisonOutcome(bool bShow)
{
    if(!AIComparison.bOpen||IsAIPracticeActive())return;
    AIComparison.bShowOutcome=bShow;AIComparison.DetailsPage=0;UpdateAIComparisonOutcome();
}
void AImperialMatchManager::MoveAIComparisonDetailsPage(int32 Delta)
{
    if(!AIComparison.bOpen||IsAIPracticeActive())return;
    const int32 Count=AIComparison.Outcome.bEligible?AIComparison.Outcome.Deltas.Num():AIComparison.Outcome.Reasons.Num();
    const int32 Last=FMath::Max(0,(Count-1)/FImperialAIComparisonView::DetailsPageSize);
    AIComparison.DetailsPage=FMath::Clamp(AIComparison.DetailsPage+FMath::Clamp(Delta,-1,1),0,Last);
}
void AImperialMatchManager::UpdateAIComparisonOutcome()
{
    if(AIComparison.Records.IsValidIndex(AIComparison.ReferenceIndex)&&AIComparison.Records.IsValidIndex(AIComparison.CandidateIndex))
        AIComparison.Outcome=ImperialAIComparison::Compare(AIComparison.Records[AIComparison.ReferenceIndex],AIComparison.Records[AIComparison.CandidateIndex]);
    else
    {
        AIComparison.Outcome={};
        AIComparison.Outcome.Reasons.Add(TEXT("Select reference A and candidate B before comparing."));
    }
}
