#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TopgunCourse.h"
#include "ImperialTopgunCourse.generated.h"
class ASoccerDronePawn;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPhysicalMaterial;
UCLASS()
class AImperialTopgunCourse : public AActor
{
    GENERATED_BODY()
public:
    AImperialTopgunCourse();
    void Initialize(ASoccerDronePawn* Pilot,bool Diagnostic);
    virtual void Tick(float Dt) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    const ImperialTopgun::Run& GetRun() const { return Run; }
    bool IsDiagnostic() const { return bDiagnostic; }
    FString Status() const;
    UStaticMeshComponent* GetPylon(int32 I) const;
    void Abort();
private:
    UFUNCTION() void OnContact(UPrimitiveComponent* HitComponent,AActor* Other,UPrimitiveComponent* OtherComp,FVector Impulse,const FHitResult& Hit);
    UStaticMeshComponent* Piece(UStaticMesh* Mesh,UMaterialInstanceDynamic* Material,FVector Position,FVector Size,bool Collision=false);
    UMaterialInstanceDynamic* Material(FLinearColor Color,float Glow=0);
    void Label(const FString& Value,FVector Position,FColor Color);
    void BuildCourse();
    UPROPERTY() TObjectPtr<ASoccerDronePawn> Pilot;
    UPROPERTY() TObjectPtr<UStaticMesh> Cube;
    UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
    UPROPERTY() TObjectPtr<UPhysicalMaterial> Rubber;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> GateMaterials;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Pylons;
    ImperialTopgun::Run Run;
    bool bDiagnostic=false,bInitialized=false;
    double Clock=0,LastWall=0,PreviousDropped=0;
};
