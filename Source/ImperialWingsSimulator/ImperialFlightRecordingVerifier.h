#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialFlightRecordingVerifier.generated.h"
class FJsonValue;
/** Explicit, fresh-data-only native filesystem fault fixture. Never sends OS input. */
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialFlightRecordingVerifier : public AActor
{
    GENERATED_BODY()
public:
    AImperialFlightRecordingVerifier();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void Check(const TCHAR* Name,bool Passed);
    void Finish();
    void Next(int32 Value);
    bool Lock(const FString& Path);
    void Unlock();
    FString OnlyNewFile(const TArray<FString>& Before) const;
    TArray<FString> Files() const;
    bool Read(const FString& Path,FString& Text) const;
    bool HasEvents(const FString& Path,bool StopExpected) const;
    int32 Stage=0;
    double StartWall=0,StageWall=0,MinDt=1e9,MaxDt=0;
    bool bOk=true,bGuarded=false,bFinished=false;
    FString DataDir,ResultFile,LogDir,BlockedText,SampleFile,SamplePrefix,StopFile,StopPrefix,RetryFile,Sentinel;
    void* LockedFile=nullptr;
    TArray<TSharedPtr<FJsonValue>> Checks;
};
