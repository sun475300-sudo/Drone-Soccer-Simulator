#include "ImperialTopgunCourse.h"
#include "ImperialMatchManager.h"
#include "SoccerDronePawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
using namespace ImperialTopgun;
AImperialTopgunCourse::AImperialTopgunCourse()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;PrimaryActorTick.bTickEvenWhenPaused=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("CourseRoot"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> C(TEXT("/Engine/BasicShapes/Cube.Cube")),P(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));Cube=C.Object;Cylinder=P.Object;
}
UMaterialInstanceDynamic* AImperialTopgunCourse::Material(FLinearColor Color,float Glow)
{
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Surface.M_Surface"));if(!Base)Base=UMaterial::GetDefaultMaterial(MD_Surface);
    auto* M=UMaterialInstanceDynamic::Create(Base,this);M->SetVectorParameterValue(TEXT("Color"),Color);M->SetScalarParameterValue(TEXT("Glow"),Glow);Materials.Add(M);return M;
}
UStaticMeshComponent* AImperialTopgunCourse::Piece(UStaticMesh* Mesh,UMaterialInstanceDynamic* Mat,FVector P,FVector Size,bool Collision)
{
    auto* C=NewObject<UStaticMeshComponent>(this);AddInstanceComponent(C);C->SetupAttachment(RootComponent);C->SetStaticMesh(Mesh);C->SetMaterial(0,Mat);C->SetRelativeLocation(P);C->SetRelativeScale3D(Size/100.);C->SetCollisionEnabled(Collision?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);if(Collision){C->SetCollisionResponseToAllChannels(ECR_Block);C->SetPhysMaterialOverride(Rubber);}C->RegisterComponent();return C;
}
void AImperialTopgunCourse::Label(const FString& Value,FVector P,FColor Color)
{
    auto* T=NewObject<UTextRenderComponent>(this);AddInstanceComponent(T);T->SetupAttachment(RootComponent);T->SetRelativeLocation(P);T->SetRelativeRotation(FRotator(0,180,0));T->SetText(FText::FromString(Value));T->SetHorizontalAlignment(EHTA_Center);T->SetWorldSize(20);T->SetTextRenderColor(Color);T->SetCollisionEnabled(ECollisionEnabled::NoCollision);T->RegisterComponent();
}
void AImperialTopgunCourse::BuildCourse()
{
    Rubber=NewObject<UPhysicalMaterial>(this);Rubber->Friction=.8f;Rubber->Restitution=.1f;
    auto* Dark=Material(FLinearColor(.055f,.065f,.075f));auto* Light=Material(FLinearColor(.75f,.78f,.8f));auto* Amber=Material(FLinearColor(1,.42f,.04f),2);
    for(int I=0;I<2;++I)
    {
        const double X=I==0?-200:200;Pylons.Add(Piece(Cylinder,Dark,FVector(X,0,180),FVector(70,70,360),true));
        for(int Band=0;Band<7;++Band)Piece(Cylinder,Band%2?Light:Amber,FVector(X,0,20+Band*50),FVector(71,71,8));
        Piece(Cylinder,Dark,FVector(X,0,3),FVector(92,92,6));Label(FString::Printf(TEXT("PYLON %d"),I+1),FVector(X-38,0,379),FColor(255,180,70));
    }
    for(int I=0;I<5;++I)
    {
        const auto G=Gates[I];auto* M=Material(FLinearColor(.1f,.8f,1),3);GateMaterials.Add(M);
        for(double Side:{-1.,1.})Piece(Cube,M,FVector(G.X*100,G.Y*100+Side*HalfWidth*100,180),FVector(3,3,240));
        for(double Z:{Bottom,Top})Piece(Cube,M,FVector(G.X*100,G.Y*100,Z*100),FVector(3,HalfWidth*200,3));
        Label(I==0?TEXT("START >>"):(I==4?TEXT("FINISH >>"):FString::Printf(TEXT("GATE %d >>"),I)),FVector(G.X*100-4,G.Y*100,321),FColor(100,230,255));
        if(I>0){const auto A=Gates[I-1];for(int J=1;J<=10;++J){const double T=J/10.;Piece(Cube,M,FVector((A.X+(G.X-A.X)*T)*100,(A.Y+(G.Y-A.Y)*T)*100,1.5),FVector(8,8,1));}}
    }
}
void AImperialTopgunCourse::Initialize(ASoccerDronePawn* InPilot,bool Diagnostic)
{
    Pilot=InPilot;bDiagnostic=Diagnostic;BuildCourse();LastWall=FPlatformTime::Seconds();Clock=bDiagnostic?0:LastWall;
    const FVector P=Pilot->GetActorLocation()/100.;Run.Reset({P.X,P.Y,P.Z},Clock);Pilot->GetBodyMesh()->OnComponentHit.AddDynamic(this,&AImperialTopgunCourse::OnContact);
    if(const auto* M=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass())))PreviousDropped=M->GetSimulationTime().DroppedSeconds;
    bInitialized=true;
}
void AImperialTopgunCourse::OnContact(UPrimitiveComponent*,AActor*,UPrimitiveComponent*,FVector,const FHitResult&){Run.Contact();}
void AImperialTopgunCourse::Tick(float Dt)
{
    Super::Tick(Dt);if(!bInitialized||!Pilot)return;
    const double Now=FPlatformTime::Seconds(),WallDt=Now-LastWall;LastWall=Now;Clock=bDiagnostic?Clock+Dt:Now;
    const FVector P=Pilot->GetActorLocation()/100.;double Dropped=PreviousDropped;
    if(const auto* M=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass())))Dropped=M->GetSimulationTime().DroppedSeconds;
    const bool Timing=Dt>0&&Dt<=.25&&FMath::Abs(GetWorld()->GetWorldSettings()->GetEffectiveTimeDilation()-1)<.0001&&Dropped<=PreviousDropped+.00001&&(bDiagnostic||(!FApp::UseFixedTimeStep()&&FMath::Abs(WallDt-Dt)<FMath::Max(.03f,Dt*.25)));
    PreviousDropped=Dropped;
    if(Run.State==Phase::Ready&&!Pilot->IsArmed())Run.Reset({P.X,P.Y,P.Z},Clock);
    else
    {
        if(Run.State==Phase::Running&&!Pilot->IsArmed())Run.Invalidate(Reason::Disarmed);
        if(Pilot->IsAutonomousEnabled()&&!bDiagnostic)Run.Invalidate(Reason::Assistance);
        Run.Update({P.X,P.Y,P.Z},Clock,Timing);
    }
    for(int I=0;I<GateMaterials.Num();++I)GateMaterials[I]->SetVectorParameterValue(TEXT("Color"),Run.State==Phase::Invalid?FLinearColor(1,.05f,.04f):(I<Run.Next?FLinearColor(.05f,1,.25f):(I==Run.Next?FLinearColor(.1f,.8f,1):FLinearColor(.25f,.3f,.35f))));
}
UStaticMeshComponent* AImperialTopgunCourse::GetPylon(int32 I)const{return Pylons.IsValidIndex(I)?Pylons[I].Get():nullptr;}
void AImperialTopgunCourse::Abort(){Run.Invalidate(Reason::Reset);}
void AImperialTopgunCourse::EndPlay(const EEndPlayReason::Type R){if(Pilot)Pilot->GetBodyMesh()->OnComponentHit.RemoveDynamic(this,&AImperialTopgunCourse::OnContact);Super::EndPlay(R);}
FString AImperialTopgunCourse::Status() const
{
    if(Run.State==Phase::Ready)return TEXT("READY / arm manually, fly through START");
    if(Run.State==Phase::Running)return FString::Printf(TEXT("GATE %d / 4   %.3f s"),Run.Next,Run.Elapsed);
    if(Run.State==Phase::Finished)return FString::Printf(TEXT("COMPLETE   %.3f s / land manually"),Run.Elapsed);
    const TCHAR* Reason=TEXT("RESET");switch(Run.Failure){case ImperialTopgun::Reason::MissedGate:Reason=TEXT("MISSED GATE / wrong order or clearance");break;case ImperialTopgun::Reason::Reverse:Reason=TEXT("REVERSE CROSSING");break;case ImperialTopgun::Reason::Contact:Reason=TEXT("CONTACT");break;case ImperialTopgun::Reason::Clock:Reason=TEXT("TIMING INTERRUPTED");break;case ImperialTopgun::Reason::Motion:Reason=TEXT("INVALID MOVEMENT");break;case ImperialTopgun::Reason::Disarmed:Reason=TEXT("DISARMED DURING RUN");break;case ImperialTopgun::Reason::Assistance:Reason=TEXT("AUTONOMOUS ASSISTANCE");break;default:break;}return FString(TEXT("INVALID / "))+Reason;
}
