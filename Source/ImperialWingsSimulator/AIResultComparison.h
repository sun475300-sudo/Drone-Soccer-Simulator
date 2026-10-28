#pragma once
#include "CoreMinimal.h"
#include "AIPracticeResult.h"

// Read-only local report metadata. Schema/optional fields must survive loading:
// schema1 has no opponent-goal metric and a missing recovery field is not zero.
struct FImperialAIComparisonRecord
{
    FString FileName,Error;
    FImperialAIResult Result;
    int32 Schema=0;
    bool bValid=false,bHasRecovery=false;
};
struct FImperialAIComparisonDelta
{
    FString Key;
    double Value=0;
};
struct FImperialAIComparisonOutcome
{
    bool bHasPair=false,bEligible=false,bRecordedConditionsMatch=false;
    TArray<FString> Reasons;
    TArray<FImperialAIComparisonDelta> Deltas;
};
struct FImperialAIComparisonView
{
    static constexpr int32 PageSize=5;
    static constexpr int32 DetailsPageSize=8;
    bool bOpen=false,bShowOutcome=false;
    int32 ReferenceIndex=INDEX_NONE,CandidateIndex=INDEX_NONE,Page=0,DetailsPage=0,HiddenFileCount=0;
    FString LoadNotice;
    TArray<FImperialAIComparisonRecord> Records;
    FImperialAIComparisonOutcome Outcome;
};
namespace ImperialAIComparison
{
    constexpr int64 MaxReportBytes=65536;
    constexpr int32 MaxListedReports=100;
    // Never writes, renames or removes a report. FileName is display-only.
    bool Load(const FString& Path,FImperialAIComparisonRecord& Out);
    FImperialAIComparisonOutcome Compare(const FImperialAIComparisonRecord& Reference,const FImperialAIComparisonRecord& Candidate);
}
