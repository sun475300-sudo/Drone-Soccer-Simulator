#include "SoccerDronePawn.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "ImperialRadioSubsystem.h"
#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "ImperialHUD.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"

#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "ImperialElasticNet.h"
#include "ImperialSoccerLayout.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Crc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

ASoccerDronePawn::ASoccerDronePawn()
{
    PrimaryActorTick.bCanEverTick = true;
    // Sample inputs and submit forces before Chaos. Chaos distributes these forces over
    // configured solver substeps. This is deliberately not called a fixed-step PID:
    // UE 5.8's legacy AddCustomPhysics callback runs only once per game frame.
    PrimaryActorTick.TickGroup = TG_PrePhysics;

    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DroneBody"));
    RootComponent = Body;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (Sphere.Succeeded())
    {
        Body->SetStaticMesh(Sphere.Object);
    }
    // Class 40 nominal diameter. The single sphere is a collision approximation of
    // the open exoskeleton; its inertia is not a measured production airframe model.
    Body->SetRelativeScale3D(FVector(0.4)); // 40 cm diameter, 20 cm radius.
    Body->SetVisibility(false, false);
    Body->SetCollisionProfileName(TEXT("PhysicsActor"));
    Body->SetSimulatePhysics(true);
    Body->SetEnableGravity(true);
    Body->SetLinearDamping(0.04f);
    Body->SetAngularDamping(0.10f);
    Body->SetUseCCD(true);
    Body->SetNotifyRigidBodyCollision(true);
    Body->OnComponentHit.AddDynamic(this,&ASoccerDronePawn::OnBodyHit);

    // Visual meshes never participate in collision or change the root body's mass.
    const auto MakePart = [this](const FString& Name, UStaticMesh* Mesh, const FVector& LocationCm,
                                const FVector& Dimensions, const FRotator& Rotation)
    {
        UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(*Name);
        Part->SetupAttachment(Body);
        Part->SetStaticMesh(Mesh);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetGenerateOverlapEvents(false);
        Part->SetAbsolute(false, false, true);
        Part->SetRelativeLocation(LocationCm * 2.5); // Root collision mesh has scale 0.4.
        Part->SetRelativeScale3D(Dimensions / 100.0);
        Part->SetRelativeRotation(Rotation);
        return Part;
    };

    DarkVisuals.Add(MakePart(TEXT("FlightController"), Cube.Object, FVector(0, 0, 0),
                             FVector(10, 7, 3), FRotator::ZeroRotator));
    DarkVisuals.Add(MakePart(TEXT("PowerDistributionBoard"), Cube.Object, FVector(0, 0, 3.5),
                                FVector(8, 5, .4), FRotator::ZeroRotator));
    // Original LOS reference design: receiver and battery tray replace camera furniture.
    DarkVisuals.Add(MakePart(TEXT("Receiver"),Cube.Object,FVector(3,0,3),FVector(4,3,1.2),FRotator::ZeroRotator));
    DarkVisuals.Add(MakePart(TEXT("ReceiverAntenna"),Cylinder.Object,FVector(4,-2,6),FVector(.25,.25,6),FRotator(15,0,0)));
    DarkVisuals.Add(MakePart(TEXT("LowerBatteryTray"),Cube.Object,FVector(-2,0,-4.5),FVector(11,6,.6),FRotator::ZeroRotator));
    DarkVisuals.Add(MakePart(TEXT("BatteryPack"),Cube.Object,FVector(-2,0,-2.4),FVector(10,5,3.5),FRotator::ZeroRotator));
    for(int32 I=0;I<2;++I)DarkVisuals.Add(MakePart(*FString::Printf(TEXT("BatteryStrap%d"),I),Cube.Object,FVector(-2,I?2.2:-2.2,-2.4),FVector(10.5,.5,4),FRotator::ZeroRotator));
    TeamHighlights.Add(MakePart(TEXT("PowerConnector"),Cube.Object,FVector(-6,1,-1),FVector(1.6,1.1,1.5),FRotator::ZeroRotator));
    for(int32 I=0;I<4;++I)DarkVisuals.Add(MakePart(*FString::Printf(TEXT("BoardStandoff%d"),I),Cylinder.Object,FVector(I%2?3.5:-3.5,I/2?2.0:-2.0,2.5),FVector(.4,.4,3),FRotator::ZeroRotator));
    DarkVisuals.Add(MakePart(TEXT("UpperCarbonPlate"),Cube.Object,FVector(0,0,1),FVector(10,7,.4),FRotator::ZeroRotator));

    const float SignX[4] = {1.0f, 1.0f, -1.0f, -1.0f};
    const float SignY[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const FVector RotorPosition(SignX[Index] * ArmLengthM * 100.0, SignY[Index] * ArmLengthM * 100.0, 0);
        const float ArmYaw = FMath::RadiansToDegrees(FMath::Atan2(RotorPosition.Y, RotorPosition.X));
        FrameVisuals.Add(MakePart(FString::Printf(TEXT("Arm%d"), Index), Cube.Object,
                                RotorPosition * 0.5, FVector(RotorPosition.Size(), 1.9, 1.5), FRotator(0, ArmYaw, 0)));
        TeamHighlights.Add(MakePart(FString::Printf(TEXT("Motor%d"), Index), Cylinder.Object,
                                    RotorPosition + FVector(0, 0, 0.75), FVector(4.5, 4.5, 3), FRotator::ZeroRotator));
        UStaticMeshComponent* Rotor=MakePart(FString::Printf(TEXT("RotorHub%d"),Index),Cylinder.Object,RotorPosition+FVector(0,0,2.5),FVector(1.6,1.6,.6),FRotator::ZeroRotator);
        DarkVisuals.Add(Rotor);
        auto* Pivot=CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("RotorPivot%d"),Index));Pivot->SetupAttachment(Body);Pivot->SetAbsolute(false,false,true);Pivot->SetRelativeScale3D(FVector::OneVector);Pivot->SetRelativeLocation((RotorPosition+FVector(0,0,2.5))*2.5);RotorPivots.Add(Pivot);
        for(int32 Blade=0;Blade<3;++Blade){double A=Blade*2*PI/3;auto* P=MakePart(FString::Printf(TEXT("Blade%d_%d"),Index,Blade),Sphere.Object,FVector::ZeroVector,FVector(5.2,1.1,.22),FRotator::ZeroRotator);P->SetupAttachment(Pivot);P->SetRelativeLocation(FVector(2.6*FMath::Cos(A),2.6*FMath::Sin(A),0));P->SetRelativeRotation(FRotator(9,Blade*120,0));PropellerBlades.Add(P);}
    }

    const auto MakeInstances = [this](const TCHAR* Name, UStaticMesh* Mesh)
    {
        UInstancedStaticMeshComponent* Instances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
        Instances->SetupAttachment(Body);
        Instances->SetStaticMesh(Mesh);
        Instances->SetMobility(EComponentMobility::Movable);
        Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Instances->SetGenerateOverlapEvents(false);
        Instances->SetCanEverAffectNavigation(false);
        // The ISM component has unit absolute scale, so its instance coordinates
        // are centimetres. Only ordinary attached components need the 2.5 factor.
        Instances->SetAbsolute(false, false, true);
        Instances->SetRelativeScale3D(FVector::OneVector);
        return Instances;
    };
    CageRods = MakeInstances(TEXT("Class40Exoskeleton"), Cylinder.Object);
    CageJoints=MakeInstances(TEXT("RoundedCageJoints"),Sphere.Object);
    GuardScuffs=MakeInstances(TEXT("GuardImpactScuffs"),Sphere.Object);
    MetalHardware=MakeInstances(TEXT("AirframeFasteners"),Cylinder.Object);
    MotorVents=MakeInstances(TEXT("MotorCoolingSlots"),Cube.Object);
    PowerWires=MakeInstances(TEXT("RedPowerHarness"),Cylinder.Object);
    GroundWires=MakeInstances(TEXT("BlackMotorHarness"),Cylinder.Object);
    CageClips=MakeInstances(TEXT("FiveEquatorClamps"),Cube.Object);
    TeamLedElements = MakeInstances(TEXT("TeamLedRing"), Cube.Object);
    RoleLedElements = MakeInstances(TEXT("PositionLedRing"), Cube.Object);
    EquatorFrame=MakeInstances(TEXT("CarbonEquatorFrame"),Cube.Object);
    FrontLedElements=MakeInstances(TEXT("WhiteFrontDirectionLED"),Cube.Object);
    RearLedElements=MakeInstances(TEXT("RedTwinRearDirectionLED"),Cube.Object);
    FrontLedElements->SetCastShadow(false);RearLedElements->SetCastShadow(false);
    TeamLedElements->SetCastShadow(false);
    RoleLedElements->SetCastShadow(false);
    EquipmentAssembly=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PhotorealEquipmentAssembly"));
    EquipmentAssembly->SetupAttachment(Body);EquipmentAssembly->SetAbsolute(false,false,true);
    EquipmentAssembly->SetCollisionEnabled(ECollisionEnabled::NoCollision);EquipmentAssembly->SetGenerateOverlapEvents(false);EquipmentAssembly->SetVisibility(false);
    for(int32 I=0;I<4;++I)
    {
        auto* Prop=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("DetailedPropeller%d"),I));
        Prop->SetupAttachment(RotorPivots[I]);Prop->SetRelativeLocation(FVector::ZeroVector);Prop->SetRelativeScale3D(FVector::OneVector);
        Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);Prop->SetGenerateOverlapEvents(false);Prop->SetVisibility(false);DetailedPropellers.Add(Prop);
    }

    RoleTagPlate = MakePart(TEXT("RoleTag"), Cube.Object, FVector(11, 0, 8),
                            FVector(0.4, 7.5, 6.5), FRotator::ZeroRotator);
    RoleTagLetter = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RoleTagLetter"));
    RoleTagLetter->SetupAttachment(Body);
    RoleTagLetter->SetAbsolute(false, false, true);
    RoleTagLetter->SetRelativeLocation(FVector(11.3, 0, 8) * 2.5);
    RoleTagLetter->SetWorldSize(4.5f);
    RoleTagLetter->SetHorizontalAlignment(EHTA_Center);
    RoleTagLetter->SetVerticalAlignment(EVRTA_TextCenter);
    RoleTagLetter->SetTextRenderColor(FColor(8, 10, 12));
    RoleTagLetter->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RoleTagLetter->SetCastShadow(false);
    UpdateRoleTag();

    FpvCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FpvCamera"));
    FpvCamera->SetupAttachment(Body);
    FpvCamera->SetRelativeLocation(FVector(55, 0, 10)); // 22 cm forward: just outside the 20 cm cage.
    FpvCamera->SetRelativeRotation(FRotator(12, 0, 0));
    FpvCamera->SetFieldOfView(105.0f);
    FpvCamera->SetAutoActivate(false);

    ChaseArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ChaseArm"));
    ChaseArm->SetupAttachment(Body);
    ChaseArm->TargetArmLength = 240.0f;
    ChaseArm->SetRelativeRotation(FRotator(-18, 0, 0));
    ChaseArm->SocketOffset = FVector(0, 0, 45);
    ChaseArm->bInheritPitch = false;
    ChaseArm->bInheritRoll = false;
    ChaseArm->bDoCollisionTest = false;
    ChaseArm->bEnableCameraLag = true;
    ChaseArm->CameraLagSpeed = 8.0f;

    ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
    ChaseCamera->SetupAttachment(ChaseArm, USpringArmComponent::SocketName);
    ChaseCamera->SetFieldOfView(83.0f);
    ChaseCamera->SetAutoActivate(true);

    for (UCameraComponent* Camera : {FpvCamera.Get(), ChaseCamera.Get()})
    {
        Camera->bConstrainAspectRatio = false;
        Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
        Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
        Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
        Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
        Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
        Camera->PostProcessSettings.AutoExposureBias = -4.0f;
        Camera->PostProcessSettings.bOverride_MotionBlurAmount = true;
        Camera->PostProcessSettings.MotionBlurAmount = 0.0f;
    }

    // The GameMode explicitly possesses the selected pilot; fleet drones must not
    // repeatedly steal Player 0 when several instances are spawned.
    AutoPossessPlayer = EAutoReceiveInput::Disabled;
}

void ASoccerDronePawn::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    BuildCageVisuals();
    UpdateRoleTag();
}

void ASoccerDronePawn::BuildCageVisuals()
{
    if (!CageRods || !TeamLedElements || !RoleLedElements) return;
    // FIDA Class 40 section 2: nominal 40 cm rounded cage and <=150 cm^2
    // openings. Six great-circle planes yield 30-degree longitude spacing.
    // Latitude hoops at 0, +/-30 and +/-60 degrees bound the largest ideal
    // spherical patch by R^2*(pi/6)*(sin(30)-sin(0)) ~=101.1 cm^2.
    // This visual geometry is not a certification or an impact-strength test.
    constexpr double RodCenterRadiusCm = 19.65;
    constexpr double RodDiameterCm = 0.7; // Center radius + half rod thickness = 20 cm.
    constexpr int32 CurveSegments = 72;
    TArray<FTransform> Rods;
    Rods.Reserve(11 * CurveSegments);
    const auto AddRod = [&Rods](const FVector& A, const FVector& B)
    {
        const FVector Direction = B - A;
        Rods.Emplace(FRotationMatrix::MakeFromZ(Direction).ToQuat(), (A + B) * 0.5,
                     FVector(RodDiameterCm / 100.0, RodDiameterCm / 100.0, Direction.Size() / 100.0));
    };
    for (int32 Latitude = -60; Latitude <= 60; Latitude += 30)
    {
        const double Phi = FMath::DegreesToRadians(static_cast<double>(Latitude));
        const double Radius = RodCenterRadiusCm * FMath::Cos(Phi);
        const double Height = RodCenterRadiusCm * FMath::Sin(Phi);
        for (int32 Segment = 0; Segment < CurveSegments; ++Segment)
        {
            const double A = 2.0 * PI * Segment / CurveSegments;
            const double B = 2.0 * PI * (Segment + 1) / CurveSegments;
            AddRod(FVector(Radius * FMath::Cos(A), Radius * FMath::Sin(A), Height),
                   FVector(Radius * FMath::Cos(B), Radius * FMath::Sin(B), Height));
        }
    }
    for (int32 Meridian = 0; Meridian < 6; ++Meridian)
    {
        const double Heading = PI * Meridian / 6.0;
        const FVector Horizontal(FMath::Cos(Heading), FMath::Sin(Heading), 0);
        const auto Point = [Horizontal](double Angle)
        {
            return RodCenterRadiusCm * (Horizontal * FMath::Cos(Angle) + FVector::UpVector * FMath::Sin(Angle));
        };
        for (int32 Segment = 0; Segment < CurveSegments; ++Segment)
        {
            AddRod(Point(2.0 * PI * Segment / CurveSegments), Point(2.0 * PI * (Segment + 1) / CurveSegments));
        }
    }
    // Additional rounded modular apertures use a fivefold arrangement inspired by
    // the public assembly photos; this is original geometry, not copied CAD.
    for(int32 Hemisphere:{-1,1})for(int32 Band=0;Band<2;++Band)for(int32 Cell=0;Cell<5;++Cell)
    {
        double Phi=FMath::DegreesToRadians((Band?62.:28.)*Hemisphere),Theta=(Cell+(Band?.5:0))*2*PI/5;
        FVector Center(FMath::Cos(Phi)*FMath::Cos(Theta),FMath::Cos(Phi)*FMath::Sin(Theta),FMath::Sin(Phi));FVector U=FVector::CrossProduct(Center,FVector::UpVector).GetSafeNormal(),V=FVector::CrossProduct(Center,U).GetSafeNormal();
        auto P=[&](double A){return (Center*FMath::Cos(.22)+(U*FMath::Cos(A)+V*FMath::Sin(A))*FMath::Sin(.22))*RodCenterRadiusCm;};
        for(int32 S=0;S<32;++S)AddRod(P(S*2*PI/32),P((S+1)*2*PI/32));
    }
    // Authored original geodesic topology: 12 pentagons + 30 hexagons. Curved
    // edges and rounded junctions replace the previous overlapping latitude hoops.
    // Retain the old closed cage as a load-error fallback, never an invisible body.
    FString GeometryText;TSharedPtr<FJsonObject> Geometry;
    const TArray<TSharedPtr<FJsonValue>>* NodesJson=nullptr;const TArray<TSharedPtr<FJsonValue>>* EdgesJson=nullptr;
    CageJoints->ClearInstances();
    if(FFileHelper::LoadFileToString(GeometryText,*FPaths::Combine(FPaths::ProjectContentDir(),TEXT("Data/cage-geodesic.json")))&&
       FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(GeometryText),Geometry)&&Geometry.IsValid()&&
       Geometry->TryGetArrayField(TEXT("nodes"),NodesJson)&&NodesJson&&NodesJson->Num()==80&&
       Geometry->TryGetArrayField(TEXT("edges"),EdgesJson)&&EdgesJson&&EdgesJson->Num()==120)
    {
        TArray<FVector> Nodes;bool Valid=true;
        for(const auto& Value:*NodesJson)
        {
            const TArray<TSharedPtr<FJsonValue>>* V=nullptr;
            if(!Value->TryGetArray(V)||!V||V->Num()!=3){Valid=false;break;}
            const FVector P((*V)[0]->AsNumber(),(*V)[1]->AsNumber(),(*V)[2]->AsNumber());
            if(P.ContainsNaN()||FMath::Abs(P.Size()-RodCenterRadiusCm)>.001){Valid=false;break;}Nodes.Add(P);
        }
        TArray<TPair<int32,int32>> Edges;
        for(const auto& Value:*EdgesJson)
        {
            const TArray<TSharedPtr<FJsonValue>>* E=nullptr;
            if(!Value->TryGetArray(E)||!E||E->Num()!=2){Valid=false;break;}
            const int32 A=(*E)[0]->AsNumber(),B=(*E)[1]->AsNumber();
            if(!Nodes.IsValidIndex(A)||!Nodes.IsValidIndex(B)||A==B){Valid=false;break;}Edges.Emplace(A,B);
        }
        if(Valid)
        {
            Rods.Reset();TArray<FTransform> Joints;
            for(const auto& Edge:Edges)for(int32 S=0;S<6;++S)
                AddRod(FMath::Lerp(Nodes[Edge.Key],Nodes[Edge.Value],S/6.).GetSafeNormal()*RodCenterRadiusCm,
                       FMath::Lerp(Nodes[Edge.Key],Nodes[Edge.Value],(S+1)/6.).GetSafeNormal()*RodCenterRadiusCm);
            for(const FVector& P:Nodes)Joints.Emplace(FQuat::Identity,P,FVector(.0066));
            CageJoints->AddInstances(Joints,false,false,false);
        }
    }
    CageRods->ClearInstances();
    CageRods->AddInstances(Rods, false, false, false);
    TArray<FTransform> Equator;for(int32 I=0;I<72;++I){double A=I*2*PI/72;Equator.Emplace(FRotator(0,FMath::RadiansToDegrees(A)+90,0),FVector(19.1*FMath::Cos(A),19.1*FMath::Sin(A),0),FVector(.018,.006,.015));}EquatorFrame->ClearInstances();EquatorFrame->AddInstances(Equator,false,false,false);
    // Decorative hardware and harnesses, all instanced and without collision.
    // Their dimensions are visual assumptions, not production STRIKER measurements.
    TArray<FTransform> Hardware,Vents,RedWires,BlackWires,Clamps;
    const auto Wire=[](TArray<FTransform>& Into,const FVector& A,const FVector& B,double Diameter)
    {const FVector D=B-A;Into.Emplace(FRotationMatrix::MakeFromZ(D).ToQuat(),(A+B)*.5,FVector(Diameter/100,Diameter/100,D.Size()/100));};
    for(int32 I=0;I<5;++I)
    {
        const double A=I*2*PI/5;const FVector P(19*FMath::Cos(A),19*FMath::Sin(A),0);
        Clamps.Emplace(FRotator(0,FMath::RadiansToDegrees(A),0),P,FVector(.014,.016,.011));
        Hardware.Emplace(FQuat::Identity,P+FVector(0,0,.7),FVector(.0035,.0035,.002));
    }
    for(int32 I=0;I<4;++I)
    {
        const FVector M((I<2?1:-1)*ArmLengthM*100,(I==0||I==3?-1:1)*ArmLengthM*100,0);
        Hardware.Emplace(FQuat::Identity,M+FVector(0,0,2.9),FVector(.006,.006,.0035));
        for(int32 J=0;J<12;++J)
        {
            const double A=J*2*PI/12;
            Vents.Emplace(FRotator(0,FMath::RadiansToDegrees(A),0),M+FVector(2.23*FMath::Cos(A),2.23*FMath::Sin(A),.8),FVector(.001,.0038,.017));
        }
        for(int32 Bolt=0;Bolt<4;++Bolt)
            Hardware.Emplace(FQuat::Identity,M+FVector(Bolt%2?.65:-.65,Bolt/2?.65:-.65,-.85),FVector(.0028,.0028,.002));
        for(int32 Cable=0;Cable<3;++Cable)
            Wire(BlackWires,M*.25+FVector(0,(Cable-1)*.23,1.2),M*.82+FVector(0,(Cable-1)*.23,1.2),.16);
    }
    for(int32 Sign:{-1,1})
    {
        const FVector Points[]={FVector(-4,Sign*2.4,-2.3),FVector(-6.5,Sign*2.4,-2.3),FVector(-7.2,Sign*2.4,-.5),FVector(-6.2,Sign*2.4,2.8),FVector(-2.5,Sign*2.4,3.6)};
        for(int32 I=0;I<4;++I)Wire(Sign<0?RedWires:BlackWires,Points[I],Points[I+1],.25);
    }
    for(int32 X:{-1,1})for(int32 Y:{-1,1})Hardware.Emplace(FQuat::Identity,FVector(X*3.5,Y*2.0,4.0),FVector(.003,.003,.002));
    for(auto* Component:{MetalHardware.Get(),MotorVents.Get(),PowerWires.Get(),GroundWires.Get(),CageClips.Get()})Component->ClearInstances();
    MetalHardware->AddInstances(Hardware,false,false,false);MotorVents->AddInstances(Vents,false,false,false);
    PowerWires->AddInstances(RedWires,false,false,false);GroundWires->AddInstances(BlackWires,false,false,false);CageClips->AddInstances(Clamps,false,false,false);

    const auto LedRing = [](int32 Count, double RadiusCm, double HeightCm, double ElementLengthCm)
    {
        TArray<FTransform> Elements;
        Elements.Reserve(Count);
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const double Angle = 2.0 * PI * Index / Count;
            Elements.Emplace(FRotator(0, FMath::RadiansToDegrees(Angle) + 90.0, 0),
                FVector(RadiusCm * FMath::Cos(Angle), RadiusCm * FMath::Sin(Angle), HeightCm),
                FVector(ElementLengthCm / 100.0, 0.0055, 0.0100));
        }
        return Elements;
    };
    // Team ring: 39 cm centerline diameter, 96 individually modeled LEDs.
    // 96/(pi*39/10) ~=7.84 LEDs per 10 cm, above the rule's minimum of 6.
    TeamLedElements->ClearInstances();
    TeamLedElements->AddInstances(LedRing(96, 19.5, 0, 1.0), false, false, false);
    RoleLedElements->ClearInstances();
    RoleLedElements->AddInstances(LedRing(80, 19.5 * FMath::Cos(PI / 6.0), 9.75, 1.0), false, false, false);
    // Original orientation layout, informed by Betaflight's white-front/red-rear
    // palette. Separate lower arcs preserve the full team and upper role rings.
    TArray<FTransform> Front,Rear;
    for(int32 I=0;I<12;++I)
    {
        const double A=FMath::DegreesToRadians(-27.+I*54./11.);
        const double Radius=FMath::Sqrt(19.3*19.3-6.*6.);
        Front.Emplace(FRotator(0,FMath::RadiansToDegrees(A)+90.,0),FVector(Radius*FMath::Cos(A),Radius*FMath::Sin(A),-6),FVector(.014,.007,.008));
        for(double Z:{-6.,-9.5}){const double R=FMath::Sqrt(19.3*19.3-Z*Z);const double B=PI+A;Rear.Emplace(FRotator(0,FMath::RadiansToDegrees(B)+90.,0),FVector(R*FMath::Cos(B),R*FMath::Sin(B),Z),FVector(.014,.007,.008));}
    }
    FrontLedElements->ClearInstances();FrontLedElements->AddInstances(Front,false,false,false);
    RearLedElements->ClearInstances();RearLedElements->AddInstances(Rear,false,false,false);
}

void ASoccerDronePawn::BeginPlay()
{
    Super::BeginPlay();
    if (CageRods->GetInstanceCount() == 0) BuildCageVisuals();
    Body->SetMassOverrideInKg(NAME_None, MassKg, true);
    InitializePhysicalModel();
    Body->SetPhysicsMaxAngularVelocityInRadians(30.0f);
    SpawnPosition = Body->GetComponentLocation();
    SpawnRotation = Body->GetComponentQuat();
    AutonomousTarget = SpawnPosition;
    SetChaseView(bChaseView);
    ApplyVisualMaterials();
    BuildDetailedEquipment();
    if(auto* Scene=GetWorld()->GetPhysicsScene())Scene->OnPhysScenePreTick.AddUObject(this,&ASoccerDronePawn::PhysicsStep);
}

void ASoccerDronePawn::EndPlay(const EEndPlayReason::Type Reason)
{
    if(auto* Scene=GetWorld()->GetPhysicsScene())Scene->OnPhysScenePreTick.RemoveAll(this);
    Super::EndPlay(Reason);
}

bool ASoccerDronePawn::VerifyDetailedAirframe() const
{
    for(const auto* Component:{CageRods.Get(),CageJoints.Get(),MetalHardware.Get(),MotorVents.Get(),PowerWires.Get(),GroundWires.Get(),CageClips.Get()})
        if(!Component||Component->GetCollisionEnabled()!=ECollisionEnabled::NoCollision||Component->IsSimulatingPhysics())return false;
    return CageRods->GetInstanceCount()==720&&CageJoints->GetInstanceCount()==80&&MetalHardware->GetInstanceCount()==29&&
        MotorVents->GetInstanceCount()==48&&PowerWires->GetInstanceCount()==4&&GroundWires->GetInstanceCount()==16&&CageClips->GetInstanceCount()==5&&
        TeamLedElements->GetInstanceCount()==96&&RoleLedElements->GetInstanceCount()==80&&FrontLedElements->GetInstanceCount()==12&&RearLedElements->GetInstanceCount()==24&&
        FMath::Abs(Body->GetMass()-.9f)<.001f;
}

void ASoccerDronePawn::BuildDetailedEquipment()
{
    auto* Assembly=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Airframe/SM_EquipmentAssembly.SM_EquipmentAssembly"));
    auto* CW=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Airframe/SM_PropellerCW.SM_PropellerCW"));
    auto* CCW=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Airframe/SM_PropellerCCW.SM_PropellerCCW"));
    if(!Assembly||!CW||!CCW)return;
    EquipmentAssembly->SetStaticMesh(Assembly);EquipmentAssembly->SetVisibility(true);
    for(int32 I=0;I<DetailedPropellers.Num();++I){DetailedPropellers[I]->SetStaticMesh(I%2?CCW:CW);DetailedPropellers[I]->SetVisibility(true);}
    for(auto* Group:{&FrameVisuals,&DarkVisuals,&TeamHighlights,&PropellerBlades})for(auto& Part:*Group)if(Part)Part->SetVisibility(false);
    for(auto* Part:{MetalHardware.Get(),MotorVents.Get(),PowerWires.Get(),GroundWires.Get()})Part->SetVisibility(false);
    bPhotorealEquipment=true;
}
bool ASoccerDronePawn::HasPhotorealEquipment() const
{
    if(!bPhotorealEquipment||!EquipmentAssembly->GetStaticMesh()||EquipmentAssembly->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)return false;
    const FVector Size=EquipmentAssembly->GetStaticMesh()->GetBoundingBox().GetSize();
    if(Size.X<20||Size.X>26||Size.Y<20||Size.Y>26||Size.Z>13||EquipmentAssembly->GetNumMaterials()<10)return false;
    for(const auto& Prop:DetailedPropellers)if(!Prop||!Prop->GetStaticMesh()||Prop->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)return false;
    return true;
}
void ASoccerDronePawn::SetEquipmentInspection(bool bEnabled)
{
    if(bEnabled&&IsArmed())return;
    bEquipmentInspection=bEnabled;
    for(auto* Part:{CageRods.Get(),CageJoints.Get(),CageClips.Get(),GuardScuffs.Get(),EquatorFrame.Get(),TeamLedElements.Get(),RoleLedElements.Get(),FrontLedElements.Get(),RearLedElements.Get()})Part->SetVisibility(!bEnabled);
    RoleTagPlate->SetVisibility(!bEnabled);RoleTagLetter->SetVisibility(!bEnabled);
}

void ASoccerDronePawn::SetupPlayerInputComponent(UInputComponent* IC)
{
    Super::SetupPlayerInputComponent(IC);
    check(IC);
    IC->BindAxis(TEXT("RollKey"), this, &ASoccerDronePawn::ReadKeyboardRoll);
    IC->BindAxis(TEXT("PitchKey"), this, &ASoccerDronePawn::ReadKeyboardPitch);
    IC->BindAxis(TEXT("YawKey"), this, &ASoccerDronePawn::ReadKeyboardYaw);
    IC->BindAxis(TEXT("ThrottleKey"), this, &ASoccerDronePawn::ReadKeyboardThrottle);
    IC->BindAxis(TEXT("BoxerRoll"), this, &ASoccerDronePawn::ReadBoxerRoll);
    IC->BindAxis(TEXT("BoxerPitch"), this, &ASoccerDronePawn::ReadBoxerPitch);
    IC->BindAxis(TEXT("BoxerYaw"), this, &ASoccerDronePawn::ReadBoxerYaw);
    IC->BindAxis(TEXT("BoxerThrottle"), this, &ASoccerDronePawn::ReadBoxerThrottle);
    IC->BindAction(TEXT("Arm"), IE_Pressed, this, &ASoccerDronePawn::ToggleArm);
    IC->BindAction(TEXT("ResetDrone"), IE_Pressed, this, &ASoccerDronePawn::ResetFromPilotInput);
    IC->BindKey(EKeys::V, IE_Pressed, this, &ASoccerDronePawn::ToggleCamera);
}

void ASoccerDronePawn::ClearManualPilotInput(const FString& Reason)
{
    const bool HadManualInput=KeyboardThrottle>0||KeyboardThrottleStep!=0||KeyRoll!=0||KeyPitch!=0||KeyYaw!=0||(!bAutonomousEnabled&&bArmed);
    KeyboardThrottle=KeyboardThrottleStep=KeyRoll=KeyPitch=KeyYaw=0;
    // Raw controller samples/calibration are never fabricated or overwritten.
    if(!bAutonomousEnabled){SetArmed(false);if(HadManualInput)LastSafetyMessage=Reason;}
}
void ASoccerDronePawn::UnPossessed()
{
    Super::UnPossessed();
    KeyboardThrottleStep = 0.0f;
    KeyRoll = KeyPitch = KeyYaw = 0.0f;
    if (!bAutonomousEnabled)
    {
        KeyboardThrottle = 0.0f;
        SetArmed(false);
    }
}

float ASoccerDronePawn::NormalizeUsb(float Value, bool bInvert) const
{
    const float Normalized = bUsbAxisZeroToOne ? (Value * 2.0f - 1.0f) : Value;
    return FMath::Clamp(bInvert ? -Normalized : Normalized, -1.0f, 1.0f);
}

float ASoccerDronePawn::GetThrottle() const
{
    if(bAngleVerificationInput)return AngleVerificationThrottle;
    if (bAutonomousEnabled)
    {
        return AutonomousThrottle;
    }
    if(const auto* Pilot=Cast<AImperialPlayerController>(GetController());Pilot&&Pilot->IsReadOnlyView())return 0;
    if (auto* Radio = GetRadio(); IsLocallyControlled() && Radio && Radio->IsEnabled())
    {
        return Radio->GetThrottle();
    }
    return bUseBoxer
        ? FMath::Clamp((NormalizeUsb(UsbThrottle, bInvertThrottle) + 1.0f) * 0.5f, 0.0f, 1.0f)
        : KeyboardThrottle;
}

UImperialRadioSubsystem* ASoccerDronePawn::GetRadio() const
{
    if (auto* World = GetWorld()) if (auto* Instance = World->GetGameInstance())
        return Instance->GetSubsystem<UImperialRadioSubsystem>();
    return nullptr;
}

float ASoccerDronePawn::GetThrottleCommand() const
{
    const float Raw=GetThrottle();
    if(!bAutonomousEnabled)if(const auto* Radio=GetRadio())return Radio->ApplyManualThrottle(Raw);
    return Raw;
}

bool ASoccerDronePawn::UsesPrecisionThrottle() const
{
    const auto* Radio=GetRadio();return !bAutonomousEnabled&&Radio&&Radio->IsPrecisionThrottleEnabled();
}

FString ASoccerDronePawn::GetInputSource() const
{
    if(bAngleVerificationInput)return TEXT("VERIFICATION_FIXTURE");
    if (bAutonomousEnabled) return TEXT("AUTO");
    if(const auto* Pilot=Cast<AImperialPlayerController>(GetController());Pilot&&Pilot->IsReadOnlyView())return TEXT("VIEW ONLY");
    if (auto* Radio = GetRadio(); IsLocallyControlled() && Radio && Radio->IsEnabled()) return TEXT("USB");
    return TEXT("KEYBOARD");
}

float ASoccerDronePawn::GetFlightSpeed() const
{
    return Body ? static_cast<float>(Body->GetPhysicsLinearVelocity().Size() * 0.01) : 0.0f;
}

float ASoccerDronePawn::GetAltitudeMeters() const
{
    return static_cast<float>(GetActorLocation().Z * 0.01);
}

float ASoccerDronePawn::GetTotalThrustNewtons() const
{
    return MotorThrustN[0] + MotorThrustN[1] + MotorThrustN[2] + MotorThrustN[3];
}

double ASoccerDronePawn::GetMotorElectricalPowerW() const
{
    double Watts=0;
    for(float Thrust:MotorThrustN)Watts+=ImperialPhysics::EstimateRotorPowerW(Thrust,PhysicalParams.Motor,PhysicalParams.Aero.DensityKgM3);
    return Watts;
}

void ASoccerDronePawn::SetPowerVerificationConditions(double MaximumCurrentA,double Soc)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("ImperialPowerSmoke")))return;
    PhysicalParams.Battery.MaximumCurrentA=FMath::Clamp(MaximumCurrentA,0.,200.);
    ++BatteryResetEpoch;
    BatteryState=ImperialPhysics::BatteryState{};BatteryState.Soc=FMath::Clamp(Soc,0.,1.);
    BatteryState.VoltageV=FMath::Lerp(PhysicalParams.Battery.EmptyVoltageV,PhysicalParams.Battery.FullVoltageV,BatteryState.Soc);
}

FString ASoccerDronePawn::GetPilotStatus() const
{
    if (!LastSafetyMessage.IsEmpty())
    {
        return LastSafetyMessage;
    }
    return bArmed ? (bAutonomousEnabled ? TEXT("AUTONOMOUS / ANGLE") : TEXT("ANGLE / MANUAL")) : TEXT("ANGLE / DISARMED");
}

float ASoccerDronePawn::GetTiltDegrees() const
{
    return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Body->GetUpVector().Z,-1.,1.)));
}
void ASoccerDronePawn::SetAngleVerificationInput(FVector Stick,float Throttle)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("ImperialAngleSmoke"))&&!FParse::Param(FCommandLine::Get(),TEXT("ImperialPowerSmoke"))&&!FParse::Param(FCommandLine::Get(),TEXT("ImperialThrottleSmoke"))&&!FParse::Param(FCommandLine::Get(),TEXT("ImperialResponseSmoke")))return;
    bAngleVerificationInput=true;
    AngleVerificationStick=Stick.GetClampedToMaxSize(1);
    AngleVerificationThrottle=FMath::Clamp(Throttle,0.f,1.f);
}

bool ASoccerDronePawn::SetArmed(bool bShouldArm)
{
    if(bShouldArm&&!bAutonomousEnabled)if(const auto* Pilot=Cast<AImperialPlayerController>(GetController());Pilot&&Pilot->IsReadOnlyView()){LastSafetyMessage=TEXT("Switch to PILOT view before arming.");return false;}
    if(bShouldArm&&bEquipmentInspection){LastSafetyMessage=TEXT("Close equipment inspection before arming.");return false;}
    if (!bShouldArm)
    {
        bArmed = false;
        MotorStates={};
        bRadioFlight = false;
        bLastArmed = false;
        ClearController();
        for (float& Force : MotorThrustN)
        {
            Force = 0.0f;
        }
        LastSafetyMessage.Empty();
        return true;
    }
    if (bArmed)
    {
        return true;
    }
    if (auto* Match = Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AImperialMatchManager::StaticClass()));
        Match && !Match->IsArmingAllowed())
    {
        LastSafetyMessage = TEXT("ARM BLOCKED: WAIT FOR SET START");
        return false;
    }
    auto* Radio = GetRadio();
    const bool NativeRadio = !bAutonomousEnabled && IsLocallyControlled() && Radio && Radio->IsEnabled();
    if (NativeRadio && (!Radio->IsConnected() || !Radio->IsCalibrated()))
    {
        LastSafetyMessage = TEXT("ARM BLOCKED: USB CONNECTION / CALIBRATION");
        return false;
    }
    if (!bAutonomousEnabled && !NativeRadio && bUseBoxer && !bBoxerCalibrated)
    {
        LastSafetyMessage = TEXT("ARM BLOCKED: CALIBRATE BOXER");
        UE_LOG(LogTemp, Warning, TEXT("%s: %s"), *GetName(), *LastSafetyMessage);
        return false;
    }
    if (!bAutonomousEnabled && GetThrottle() > 0.05f)
    {
        LastSafetyMessage = TEXT("ARM BLOCKED: LOWER THROTTLE");
        UE_LOG(LogTemp, Warning, TEXT("%s: %s"), *GetName(), *LastSafetyMessage);
        return false;
    }
    LastSafetyMessage.Empty();
    bArmed = true;
    bRadioFlight = NativeRadio;
    Body->WakeAllRigidBodies();
    return true;
}

void ASoccerDronePawn::ToggleArm()
{
    if(const auto* Pilot=Cast<AImperialPlayerController>(GetController());Pilot&&Pilot->IsInputResetThisFrame())return;
    if (const auto* Pilot=Cast<APlayerController>(GetController()))
        if (const auto* HUD=Cast<AImperialHUD>(Pilot->GetHUD()); HUD&&HUD->IsPilotMenuOpen()) return;
    if (bAutonomousEnabled)
    {
        SetAutonomousEnabled(false);
        return;
    }
    SetArmed(!bArmed);
}

void ASoccerDronePawn::SetAutonomousTarget(const FVector& WorldPosition)
{
    if (!WorldPosition.ContainsNaN())
    {
        AutonomousTarget = WorldPosition;
    }
}

bool ASoccerDronePawn::SetAutonomousHorizontalLimits(float SpeedMps,float AccelerationMps2)
{
    if(!FMath::IsFinite(SpeedMps)||!FMath::IsFinite(AccelerationMps2)||
       SpeedMps<.1f||SpeedMps>3.f||AccelerationMps2<.1f||AccelerationMps2>4.f)return false;
    AutonomousHorizontalSpeedLimitMps=SpeedMps;
    AutonomousHorizontalAccelerationLimitMps2=AccelerationMps2;
    return true;
}

void ASoccerDronePawn::ResetAutonomousHorizontalLimits()
{
    AutonomousHorizontalSpeedLimitMps=2.2f;
    AutonomousHorizontalAccelerationLimitMps2=3.2f;
}

void ASoccerDronePawn::SetAutonomousEnabled(bool bEnabled)
{
    // Also restore on an idempotent disable so a later SDACS/HOLD launch cannot inherit AI limits.
    if(!bEnabled)ResetAutonomousHorizontalLimits();
    if(bEnabled)if(const auto* Match=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));Match&&!Match->IsAutonomyAllowed())
    {LastSafetyMessage=TEXT("AUTONOMY BLOCKED: PRACTICE ONLY");return;}
    if (bAutonomousEnabled == bEnabled)
    {
        return;
    }
    bAutonomousEnabled = bEnabled;
    AutonomousThrottle = 0.0f;
    KeyboardThrottle = 0.0f;
    ClearController();
    if (bEnabled)
    {
        AutonomousTarget = GetActorLocation();
        AutonomousHeading = GetActorRotation().Yaw;
        SetArmed(true);
    }
    else
    {
        SetArmed(false);
    }
}

void ASoccerDronePawn::SetChaseView(bool bEnabled)
{
    bChaseView = bEnabled;
    FpvCamera->SetActive(!bEnabled);
    ChaseCamera->SetActive(bEnabled);
}

void ASoccerDronePawn::ToggleCamera()
{
    SetChaseView(!bChaseView);
}

void ASoccerDronePawn::SetTeamColor(FLinearColor Color)
{
    TeamColor = Color;
    if (HasActorBegunPlay())
    {
        ApplyVisualMaterials();
    }
}

void ASoccerDronePawn::SetPlayerRole(int32 PlayerRoleValue)
{
    PlayerRole = FMath::Clamp(PlayerRoleValue, 0, 4);
    UpdateRoleTag();
    SetRoleColor(ImperialSoccerLayout::RoleColor(PlayerRole));
}

void ASoccerDronePawn::SetRoleColor(FLinearColor Color)
{
    RoleColor = Color;
    if (HasActorBegunPlay()) ApplyVisualMaterials();
}

void ASoccerDronePawn::UpdateRoleTag()
{
    if (!RoleTagPlate || !RoleTagLetter) return;
    const bool bRequiresTag = PlayerRole <= 1;
    RoleTagPlate->SetVisibility(bRequiresTag);
    RoleTagLetter->SetVisibility(bRequiresTag);
    RoleTagLetter->SetText(FText::FromString(PlayerRole == 0 ? TEXT("S") : TEXT("G")));
}

void ASoccerDronePawn::ApplyVisualMaterials()
{
    UMaterialInterface* Surface = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Surface.M_Surface"));
    if (!Surface)
    {
        Surface = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    }
    if (!Surface)
    {
        return;
    }
    const auto Paint = [this, Surface](const TArray<TObjectPtr<UStaticMeshComponent>>& Parts,
                                      FLinearColor Color, float Glow)
    {
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Surface, this);
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Material->SetScalarParameterValue(TEXT("Glow"), Glow);
        for (UStaticMeshComponent* Part : Parts)
        {
            if (Part)
            {
                Part->SetMaterial(0, Material);
            }
        }
    };
    Paint(TeamHighlights, FLinearColor(0.11f, 0.13f, 0.16f), 0.0f);
    Paint(FrameVisuals, FLinearColor(0.018f, 0.022f, 0.03f), 0.0f);
    Paint(DarkVisuals, FLinearColor(0.028f, 0.035f, 0.045f), 0.0f);
    Paint(PropellerBlades,FLinearColor(.42f,.018f,.025f),0);
    if(auto* Carbon=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Carbon.M_Carbon"))){auto* C=UMaterialInstanceDynamic::Create(Carbon,this);C->SetVectorParameterValue(TEXT("Color"),FLinearColor(.035f,.038f,.04f));EquatorFrame->SetMaterial(0,C);for(auto& Part:FrameVisuals)Part->SetMaterial(0,C);}
    const auto Material = [this, Surface](FLinearColor Color, float Glow)
    {
        UMaterialInstanceDynamic* Result = UMaterialInstanceDynamic::Create(Surface, this);
        Result->SetVectorParameterValue(TEXT("Color"), Color);
        Result->SetScalarParameterValue(TEXT("Glow"), Glow);
        return Result;
    };
    GuardMaterial=Material(FLinearColor(.65f,.67f,.63f),0);
    CageRods->SetMaterial(0,GuardMaterial);
    GuardScuffs->SetMaterial(0,Material(FLinearColor(.22f,.23f,.22f),0));
    CageJoints->SetMaterial(0,Material(FLinearColor(.54f,.56f,.53f),0));
    CageClips->SetMaterial(0,Material(FLinearColor(.055f,.063f,.07f),0));
    MotorVents->SetMaterial(0,Material(FLinearColor(.006f,.009f,.013f),0));
    PowerWires->SetMaterial(0,Material(FLinearColor(.38f,.012f,.008f),0));
    GroundWires->SetMaterial(0,Material(FLinearColor(.009f,.011f,.014f),0));
    if(auto* Metal=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Metal.M_Metal")))
    {auto* M=UMaterialInstanceDynamic::Create(Metal,this);M->SetVectorParameterValue(TEXT("Color"),FLinearColor(.32f,.36f,.40f));M->SetScalarParameterValue(TEXT("Roughness"),.3f);M->SetScalarParameterValue(TEXT("Metallic"),.8f);MetalHardware->SetMaterial(0,M);}
    TeamLedElements->SetMaterial(0, Material(TeamColor, 8.0f));
    RoleLedElements->SetMaterial(0, Material(RoleColor, 8.0f));
    FrontLedElements->SetMaterial(0,Material(FLinearColor(.92f,.97f,1.f),6.f));
    RearLedElements->SetMaterial(0,Material(FLinearColor(1.f,.012f,.008f),5.f));
    RoleTagPlate->SetMaterial(0, Material(RoleColor, 0.12f));
    UpdateRoleTag();
    UpdateGuardMaterial();
}

void ASoccerDronePawn::UpdateGuardMaterial()
{
    const float Damage=1.f-static_cast<float>(GuardState.Integrity);
    if(GuardMaterial)GuardMaterial->SetVectorParameterValue(TEXT("Color"),FMath::Lerp(FLinearColor(.65f,.67f,.63f),FLinearColor(.38f,.36f,.30f),Damage));
    if(CagePhysicalMaterial)
    {
        CagePhysicalMaterial->Restitution=.25f-.15f*Damage;
        CagePhysicalMaterial->Friction=.4f+.15f*Damage;CagePhysicalMaterial->StaticFriction=.55f+.2f*Damage;
        Body->SetPhysMaterialOverride(CagePhysicalMaterial);
    }
}

void ASoccerDronePawn::ClearController()
{
    IntegralError = FVector::ZeroVector;
    AutonomousVelocityIntegral = FVector::ZeroVector;
    PreviousBodyRate = FVector::ZeroVector;
    FilteredBodyAcceleration = FVector::ZeroVector;
}

void ASoccerDronePawn::ResetDrone()
{
    ResetAutonomousHorizontalLimits();
    bAngleVerificationInput=false;AngleVerificationThrottle=0;AngleVerificationStick=FVector::ZeroVector;
    bAutonomousEnabled = false;
    SetArmed(false);
    AutonomousThrottle = 0.0f;
    KeyboardThrottle = 0.0f;
    KeyboardThrottleStep = 0.0f;
    Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
    ++BatteryResetEpoch;
    BatteryState=ImperialPhysics::BatteryState{};PowerBudget={};MotorStates={};NetStates={};Sensors.Reset(FCrc::StrCrc32(*GetName()));PhysicalTime=0;PreviousVelocityMS=FVector::ZeroVector;CageCompression=0;
    GuardState={};LastWearOther.Reset();LastWearContactTime=-100;GuardScuffs->ClearInstances();UpdateGuardMaterial();SetEquipmentInspection(false);
    Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    // Reset is the sole teleport operation. All flight motion comes from Chaos forces.
    Body->SetWorldLocationAndRotation(SpawnPosition, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
    AutonomousTarget = SpawnPosition;
    Body->WakeAllRigidBodies();
}
void ASoccerDronePawn::ResetFromPilotInput()
{
    if(const auto* Pilot=Cast<AImperialPlayerController>(GetController());Pilot&&(Pilot->IsReadOnlyView()||Pilot->IsEquipmentInspection()))return;
    if(const auto* Pilot=Cast<APlayerController>(GetController()))if(const auto* HUD=Cast<AImperialHUD>(Pilot->GetHUD());HUD&&HUD->IsPilotMenuOpen())return;
    ResetDrone();
    if(auto* Pilot=Cast<AImperialPlayerController>(GetController()))Pilot->FlushPilotContextKeys();
}

void ASoccerDronePawn::Tick(float Dt)
{
    Super::Tick(Dt);
    CageCompression=FMath::FInterpTo(CageCompression,0,Dt,10);
    const float Compression=FMath::Max(CageCompression,(1.f-static_cast<float>(GuardState.Integrity))*.035f);
    FVector VisualScale(1+Compression*.25);VisualScale[CageCompressionAxis]=1-Compression;
    CageRods->SetRelativeScale3D(VisualScale);TeamLedElements->SetRelativeScale3D(VisualScale);RoleLedElements->SetRelativeScale3D(VisualScale);
    CageJoints->SetRelativeScale3D(VisualScale);
    GuardScuffs->SetRelativeScale3D(VisualScale);
    if (bArmed && bRadioFlight)
    {
        auto* Radio = GetRadio();
        if (!IsLocallyControlled() || !Radio || !Radio->IsEnabled() || !Radio->IsConnected() || !Radio->IsCalibrated() || !FApp::HasFocus())
        {
            SetArmed(false);
            if (Radio) Radio->DisableRadio();
            LastSafetyMessage = TEXT("USB INPUT LOST / DISARMED");
        }
    }
    if (!bAutonomousEnabled)
    {
        bool Modal=false;
        if(const auto* Pilot=Cast<APlayerController>(GetController()))
            if(const auto* HUD=Cast<AImperialHUD>(Pilot->GetHUD()))Modal=HUD->IsPilotMenuOpen();
        const auto* RolePilot=Cast<AImperialPlayerController>(GetController());
        // EvaluateInputDelegates may already have cached axes when a reset action
        // flushes keys mid-frame. Discard that frame's cached axis callbacks too.
        if(Modal||(RolePilot&&(RolePilot->IsInputResetThisFrame()||RolePilot->IsReadOnlyView()||RolePilot->IsEquipmentInspection()))){KeyboardThrottle=KeyboardThrottleStep=KeyRoll=KeyPitch=KeyYaw=0;}
        else KeyboardThrottle = FMath::Clamp(KeyboardThrottle + KeyboardThrottleStep * Dt * 0.35f, 0.0f, 1.0f);
    }
    RotorAngle = FMath::Fmod(RotorAngle + (bArmed ? 1400.0f + 5000.0f * GetThrottle() : 0.0f) * Dt, 360.0f);
    for (int32 Index = 0; Index < RotorPivots.Num(); ++Index)
    {
        RotorPivots[Index]->SetRelativeRotation(FRotator(0, (Index % 2 ? 1 : -1) * RotorAngle + Index * 45.0, 0));
    }
}

void ASoccerDronePawn::PhysicsStep(FPhysScene_Chaos* Scene,float Dt)
{
    // Scene supplies the actual synchronous Chaos frame budget after its clamp.
    // One control update per physics frame, NOT a PID update per Chaos substep.
    if(Dt<=KINDA_SMALL_NUMBER||!Body->IsSimulatingPhysics())return;
    ++PhysicsStepSequence;
    const FVector VelocityMS=Body->GetPhysicsLinearVelocity()/100.;const FQuat Rotation=Body->GetComponentQuat();
    const float SampleSeconds=PreviousPhysicsSeconds>0?PreviousPhysicsSeconds:Dt;
    const FVector Specific=Rotation.UnrotateVector((VelocityMS-PreviousVelocityMS)/FMath::Max(.001f,SampleSeconds)-FVector(0,0,GetWorld()->GetGravityZ()/100.));PreviousVelocityMS=VelocityMS;
    const FVector Rate=Rotation.UnrotateVector(Body->GetPhysicsAngularVelocityInRadians());
    SensorElapsedSeconds+=Dt;
    SensorSample=Sensors.Step(PhysicalTime,{Rate.X,Rate.Y,Rate.Z},{Specific.X,Specific.Y,Specific.Z},PhysicalParams.Sensors,bSensorModel);
    ApplyNetForces(Dt);ApplyFlightForces(Dt);PhysicalTime+=Dt;PreviousPhysicsSeconds=Dt;
}

FVector ASoccerDronePawn::CalculateAutonomousRate(float& OutCollectiveThrustN,float Step)
{
    const FVector PositionErrorM = (AutonomousTarget - Body->GetComponentLocation()) * 0.01;
    const FVector VelocityMS = Body->GetPhysicsLinearVelocity() * 0.01;
    // Position -> velocity -> acceleration cascade with speed/tilt limits. This keeps
    // planner waypoint changes physically bounded instead of snapping the aircraft.
    FVector DesiredVelocity = PositionErrorM * 0.8;
    // Preserve the original double-valued defaults exactly for all non-transit control.
    const double HorizontalSpeedLimit = AutonomousHorizontalSpeedLimitMps==2.2f?2.2:static_cast<double>(AutonomousHorizontalSpeedLimitMps);
    const FVector Horizontal = FVector(DesiredVelocity.X, DesiredVelocity.Y, 0).GetClampedToMaxSize(HorizontalSpeedLimit);
    DesiredVelocity.X = Horizontal.X;
    DesiredVelocity.Y = Horizontal.Y;
    DesiredVelocity.Z = FMath::Clamp(DesiredVelocity.Z, -1.2, 1.5);
    const FVector VelocityError=DesiredVelocity-VelocityMS;
    // Bounded outer-loop integral removes residual thrust/surface-effect bias.
    // Without it a return could hover ~20cm off its corridor indefinitely.
    AutonomousVelocityIntegral+=VelocityError*(Step*.25);
    for(int32 Axis=0;Axis<3;++Axis)AutonomousVelocityIntegral[Axis]=FMath::Clamp(AutonomousVelocityIntegral[Axis],-.8,.8);
    FVector DesiredAcceleration = VelocityError * 1.6 + AutonomousVelocityIntegral;
    const double HorizontalAccelerationLimit = AutonomousHorizontalAccelerationLimitMps2==3.2f?3.2:static_cast<double>(AutonomousHorizontalAccelerationLimitMps2);
    const FVector HorizontalAcceleration = FVector(DesiredAcceleration.X, DesiredAcceleration.Y, 0).GetClampedToMaxSize(HorizontalAccelerationLimit);
    DesiredAcceleration.X = HorizontalAcceleration.X;
    DesiredAcceleration.Y = HorizontalAcceleration.Y;
    DesiredAcceleration.Z = FMath::Clamp(DesiredAcceleration.Z, -3.0, 4.0);

    const float GravityMS = GetWorld() ? -GetWorld()->GetGravityZ() * 0.01f : 9.81f;
    const FVector DesiredForceN = (DesiredAcceleration + FVector(0, 0, GravityMS)) * MassKg;
    const FVector DesiredUp = DesiredForceN.GetSafeNormal();
    const FQuat Rotation = Body->GetComponentQuat();
    const FVector CurrentUp = Rotation.GetUpVector();
    const FVector TiltErrorLocal = Rotation.UnrotateVector(FVector::CrossProduct(CurrentUp, DesiredUp));
    const float HeadingError = FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, AutonomousHeading));
    OutCollectiveThrustN = FMath::Clamp(static_cast<float>(DesiredForceN.Z / FMath::Max(0.45, CurrentUp.Z)) * 0.25f,
                                       0.0f, MaxMotorThrustN);
    return FVector(FMath::Clamp(TiltErrorLocal.X * 4.0, -2.2, 2.2),
                   FMath::Clamp(TiltErrorLocal.Y * 4.0, -2.2, 2.2),
                   FMath::Clamp(static_cast<double>(HeadingError) * 2.0, -1.8, 1.8));
}

void ASoccerDronePawn::InitializePhysicalModel()
{
    PhysicalParams.Motor.MaxThrustN=MaxMotorThrustN;
    PhysicalParams.Motor.IdleThrustN=ArmedIdleThrustN;
    PhysicalParams.Motor.ResponseSeconds=MotorResponseSeconds;
    PhysicalParams.Contact.StiffnessNPerM=300;
    PhysicalParams.Contact.DampingNsPerM=22;
    PhysicalParams.Contact.MaximumForceN=180;
    FString Text;TSharedPtr<FJsonObject> Settings;
    if(FFileHelper::LoadFileToString(Text,*FPaths::Combine(FPaths::ProjectContentDir(),TEXT("Data/physics-defaults.json")))&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Settings)&&Settings.IsValid())
    {
        auto Number=[&](const TCHAR* Key,double& V,double Lo,double Hi){double N;if(Settings->TryGetNumberField(Key,N)&&FMath::IsFinite(N))V=FMath::Clamp(N,Lo,Hi);};
        Number(TEXT("motor_pwm_exponent"),PhysicalParams.Motor.PwmExponent,1,3);
        Number(TEXT("motor_response_s"),PhysicalParams.Motor.ResponseSeconds,.005,.2);
        Number(TEXT("battery_capacity_ah"),PhysicalParams.Battery.CapacityAh,.1,10);
        Number(TEXT("battery_internal_resistance_ohm"),PhysicalParams.Battery.InternalResistanceOhm,.001,.3);
        Number(TEXT("battery_maximum_current_a"),PhysicalParams.Battery.MaximumCurrentA,0,200);
        Number(TEXT("drag_effective_cd_area_m2"),PhysicalParams.Aero.EffectiveCdAreaM2,.001,.2);
        Number(TEXT("sensor_latency_s"),PhysicalParams.Sensors.LatencySeconds,0,.03);
        Number(TEXT("gyro_noise_rad_s"),PhysicalParams.Sensors.GyroNoiseStdRadS,0,.02);
        Number(TEXT("wake_coupling"),PhysicalParams.Downwash.Coupling,0,.5);
        Number(TEXT("net_stiffness_n_per_m"),PhysicalParams.Contact.StiffnessNPerM,60,1500);
        Number(TEXT("net_damping_ns_per_m"),PhysicalParams.Contact.DampingNsPerM,1,80);
        Number(TEXT("guard_energy_capacity_j"),GuardParameters.EnergyCapacityJ,1,10000);
        Number(TEXT("guard_damage_threshold_j"),GuardParameters.DamageThresholdJ,0,100);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialBaselinePhysics")))bBatteryModel=bWakeModel=bSurfaceModel=bSensorModel=bNonlinearMotors=false;
    Sensors.Reset(FCrc::StrCrc32(*GetName()));
    CagePhysicalMaterial=NewObject<UPhysicalMaterial>(this);
    CagePhysicalMaterial->Friction=.4f;CagePhysicalMaterial->StaticFriction=.55f;CagePhysicalMaterial->Restitution=.25f;
    Body->SetPhysMaterialOverride(CagePhysicalMaterial);
    // Component masses are an explicit prototype distribution, not manufacturer data.
    const double S=MassKg/.9;
    std::vector<ImperialPhysics::MassPoint> Masses={
        {.25*S,{0,0,0},{.006667*S,.006667*S,.006667*S}},
        {.25*S,{-.015,0,-.035},{.000085*S,.000242*S,.00026*S}},
        {.14*S,{0,0,0},{.0004*S,.0004*S,.0006*S}},
        {.10*S,{.0375,0,.02},{.00002*S,.00002*S,.00002*S}}};
    for(int I=0;I<4;++I)Masses.push_back({.04*S,{I<2?ArmLengthM:-ArmLengthM,I==0||I==3?-ArmLengthM:ArmLengthM,0},{.000005*S,.000005*S,.000005*S}});
    ImperialPhysics::Vec3 COM;double Total=0;for(const auto& M:Masses){COM+=M.PositionM*M.MassKg;Total+=M.MassKg;}COM=COM/Total;
    CenterOfMassLocalM=FVector(COM.X,COM.Y,COM.Z);
    // UE applies component scale to COMNudge; supply the inverse-scaled offset.
    Body->SetCenterOfMass(CenterOfMassLocalM*100/Body->GetComponentScale());
    auto Desired=ImperialPhysics::InertiaDiagonal(Masses,COM);
    auto* BI=Body->GetBodyInstance();
    // Chaos defaults this off. Enable the rigid body's Euler gyroscopic coupling;
    // spinning rotor angular momentum is an additional, separately applied term.
    BI->SetGyroscopicTorqueEnabled(true);
    // InertiaTensorScale geometrically rescales an equivalent box. Set the desired
    // diagonal directly in Chaos instead. Off-diagonal inertia is approximated.
    FPhysicsCommand::ExecuteWrite(BI->ActorHandle,[&](const FPhysicsActorHandle& Actor){FPhysicsInterface::SetMassSpaceInertiaTensor_AssumesLocked(Actor,FVector(Desired.X,Desired.Y,Desired.Z)*10000);});
    const FVector ActualCOM=Body->GetComponentQuat().UnrotateVector(Body->GetCenterOfMass()-Body->GetComponentLocation())/100.;
    const FVector ActualInertia=BI->GetBodyInertiaTensor()*.0001;
    UE_LOG(LogTemp,Display,TEXT("IMPERIAL_MASS %s mass_kg=%.4f com_m=%s inertia_kg_m2=%s"),*GetName(),Body->GetMass(),*ActualCOM.ToString(),*ActualInertia.ToString());
}

void ASoccerDronePawn::SetWakeSources(const TArray<FVector>& PositionsM,const TArray<float>& ThrustsN)
{
    WakePositionsM=PositionsM;WakeThrustsN=ThrustsN;
}
FVector ASoccerDronePawn::AirVelocityAt(const FVector& PointM) const
{
    ImperialPhysics::Vec3 Air;
    if(bWakeModel)for(int32 I=0;I<FMath::Min(WakePositionsM.Num(),WakeThrustsN.Num());++I)
    {
        const auto& P=WakePositionsM[I];Air+=ImperialPhysics::DownwashVelocity({P.X,P.Y,P.Z},{PointM.X,PointM.Y,PointM.Z},WakeThrustsN[I],PhysicalParams.Downwash);
    }
    Air=ImperialPhysics::ClampMagnitude(Air,3);return FVector(Air.X,Air.Y,Air.Z);
}
float ASoccerDronePawn::GetNetDeflection() const
{
    double Maximum=0;for(const auto& N:NetStates)Maximum=FMath::Max(Maximum,N.DeflectionM);return static_cast<float>(Maximum);
}
void ASoccerDronePawn::OnBodyHit(UPrimitiveComponent* HitComponent,AActor* OtherActor,UPrimitiveComponent* OtherComponent,FVector NormalImpulse,const FHitResult& Hit)
{
    const float ImpulseNs=NormalImpulse.Size()*.01f;
    CageCompression=FMath::Max(CageCompression,FMath::Clamp(ImpulseNs*.015f,0.f,.08f));
    const FVector N=Body->GetComponentQuat().UnrotateVector(Hit.ImpactNormal).GetAbs();CageCompressionAxis=N.X>N.Y?(N.X>N.Z?0:2):(N.Y>N.Z?1:2);
    if(OtherActor&&LastWearOther.Get()==OtherActor&&PhysicalTime-LastWearContactTime<.08)return;
    const double OtherMass=OtherComponent&&OtherComponent->IsSimulatingPhysics()?OtherComponent->GetMass():0;
    const auto Result=GuardState.Apply(ImpulseNs,MassKg,OtherMass,CagePhysicalMaterial?CagePhysicalMaterial->Restitution:.25,OtherActor&&OtherActor->IsA<ASoccerDronePawn>()?.5:1.,GuardParameters);
    if(!Result.Significant)return;
    LastWearOther=OtherActor;LastWearContactTime=PhysicalTime;UpdateGuardMaterial();
    if(GuardScuffs->GetInstanceCount()<36)
    {
        const FVector Local=Body->GetComponentQuat().UnrotateVector(Hit.ImpactPoint-Body->GetComponentLocation());
        double Best=TNumericLimits<double>::Max();FTransform Closest;
        for(int32 I=0;I<CageRods->GetInstanceCount();++I)
        {FTransform T;CageRods->GetInstanceTransform(I,T,false);const double D=FVector::DistSquared(Local,T.GetLocation());if(D<Best){Best=D;Closest=T;}}
        const FVector Point=Closest.GetLocation()+Closest.GetLocation().GetSafeNormal()*.28f;
        GuardScuffs->AddInstance(FTransform(Closest.GetRotation(),Point,FVector(.0035,.0012,.014)),false);
    }
    if(auto* Match=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass())))
        Match->RecordGuardImpact(this,static_cast<float>(Result.DissipatedJ));
}
void ASoccerDronePawn::ApplyNetForces(float Dt)
{
    const FVector P=GetActorLocation()/100.,V=Body->GetPhysicsLinearVelocity()/100.;
    const FVector Normals[5]={FVector(1,0,0),FVector(-1,0,0),FVector(0,1,0),FVector(0,-1,0),FVector(0,0,-1)};
    const double D[5]={P.X+8,8-P.X,P.Y+3.5,3.5-P.Y,4.5-P.Z};
    auto* Net=Cast<AImperialElasticNet>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialElasticNet::StaticClass()));
    for(int I=0;I<5;++I)
    {
        const double Penetration=.2-D[I];const double NormalVelocity=FVector::DotProduct(V,Normals[I]);
        const double Force=ImperialPhysics::NetContactStep(NetStates[I],Penetration,NormalVelocity,Dt,PhysicalParams.Contact);
        if(Force>0)
        {
            const FVector Contact=(P-Normals[I]*D[I])*100;
            Body->AddForceAtLocation(Normals[I]*Force*100,Contact);
            const FVector Tangent=V-Normals[I]*NormalVelocity;
            Body->AddForce(-Tangent.GetSafeNormal()*FMath::Min(Tangent.Size()*2.,Force*.12)*100);
            CageCompression=FMath::Max(CageCompression,static_cast<float>(NetStates[I].CageCompression));
            if(Net)Net->SubmitImpact(Contact,Normals[I],NetStates[I].DeflectionM);
        }
    }
    if(P.Z<=.215&&V.Size()<4){const FVector W=Body->GetPhysicsAngularVelocityInRadians();Body->AddTorqueInRadians(-W.GetClampedToMaxSize(15)*.002*10000);}
}
void ASoccerDronePawn::ApplyFlightForces(float Dt)
{
    if(Dt<=KINDA_SMALL_NUMBER||!Body->IsSimulatingPhysics())return;
    const float Step=Dt;
    FlightModelSeconds+=Step;
    if(!bArmed)
    {
        bLastArmed=false;MotorStates={};PowerBudget={};for(float& F:MotorThrustN)F=0;
        ImperialPhysics::BatteryStep(BatteryState,0,Step,PhysicalParams.Battery,bBatteryModel);ApplyAerodynamicDrag();return;
    }
    // A hovering body can satisfy Chaos sleep thresholds even though its motors
    // are active. Sleeping then freezes the residual position/altitude error.
    // Keep powered aircraft awake; disarmed floor bodies may still sleep.
    Body->WakeAllRigidBodies();
    const FQuat R=Body->GetComponentQuat();const FVector IdealRate=R.UnrotateVector(Body->GetPhysicsAngularVelocityInRadians());
    const FVector Rate=SensorSample.Valid?FVector(SensorSample.GyroRadS.X,SensorSample.GyroRadS.Y,SensorSample.GyroRadS.Z):IdealRate;
    if(!bLastArmed){ClearController();PreviousBodyRate=Rate;}bLastArmed=true;
    float Collective=FMath::Lerp(ArmedIdleThrustN,MaxMotorThrustN,GetThrottle());FVector TargetRate;
    if(bAutonomousEnabled){TargetRate=CalculateAutonomousRate(Collective,Step);AutonomousThrottle=FMath::Clamp((Collective-ArmedIdleThrustN)/(MaxMotorThrustN-ArmedIdleThrustN),0.f,1.f);}
    else
    {
        auto* Radio=GetRadio();const bool USB=IsLocallyControlled()&&Radio&&Radio->IsEnabled();
        const float Roll=bAngleVerificationInput?AngleVerificationStick.X:(USB?Radio->GetNormalizedRoll():(bUseBoxer?NormalizeUsb(UsbRoll,bInvertRoll):KeyRoll));
        const float Pitch=bAngleVerificationInput?AngleVerificationStick.Y:(USB?Radio->GetNormalizedPitch():(bUseBoxer?NormalizeUsb(UsbPitch,bInvertPitch):KeyPitch));
        const float Yaw=bAngleVerificationInput?AngleVerificationStick.Z:(USB?Radio->GetNormalizedYaw():(bUseBoxer?NormalizeUsb(UsbYaw,bInvertYaw):KeyYaw));
        const ImperialInput::PilotResponse Response=Radio?Radio->GetPilotResponse():ImperialInput::PilotResponse{};
        const auto Desired=ImperialPhysics::AngleDesiredUp(Response.Shape(0,Roll),Response.Shape(1,Pitch),MaximumTiltDegrees);
        const FQuat Heading(FRotator(0,Body->GetComponentRotation().Yaw,0));
        const FVector DesiredUpBody=R.UnrotateVector(Heading.RotateVector(FVector(Desired.X,Desired.Y,Desired.Z)));
        const auto TiltRate=ImperialPhysics::AngleBodyRate({DesiredUpBody.X,DesiredUpBody.Y,DesiredUpBody.Z},AngleGainPerSecond,FMath::DegreesToRadians(360.));
        const auto Limited=Response.LimitTiltRates(TiltRate.X,TiltRate.Y);
        TargetRate=FVector(Limited[0],Limited[1],Response.Shape(2,Yaw)*Response.RateDegrees[2]*(PI/180.));
        LastManualTargetRateDegrees=TargetRate*(180./PI);
    }
    const FVector Error=TargetRate-Rate;
    const FVector Derivative=(Rate-PreviousBodyRate)/(PreviousPhysicsSeconds>0?PreviousPhysicsSeconds:Step);PreviousBodyRate=Rate;
    FilteredBodyAcceleration=FMath::Lerp(FilteredBodyAcceleration,Derivative,1.f-FMath::Exp(-Step/.035f));
    ImperialPhysics::PidState Pid[3]={{IntegralError.X},{IntegralError.Y},{IntegralError.Z}};FVector Torque;
    for(int I=0;I<3;++I)Torque[I]=ImperialPhysics::PidStep(Pid[I],Error[I],FilteredBodyAcceleration[I],Step,RateKp[I],RateKi[I],RateKd[I],I==2?-.18:-.5,I==2?.18:.5);
    IntegralError=FVector(Pid[0].Integral,Pid[1].Integral,Pid[2].Integral);
    const double Voltage=ImperialPhysics::BatteryThrustFactor(BatteryState,PhysicalParams.Battery,bBatteryModel);
    const double ManualCollective=(ArmedIdleThrustN+(MaxMotorThrustN-ArmedIdleThrustN)*FMath::Pow((double)GetThrottleCommand(),bNonlinearMotors?PhysicalParams.Motor.PwmExponent:1.))*Voltage;
    Torque+=FVector::CrossProduct(CenterOfMassLocalM,FVector(0,0,(bAutonomousEnabled?Collective:ManualCollective)*4));
    const float SignX[4]={1,1,-1,-1},SignY[4]={-1,1,1,-1},Spin[4]={1,-1,1,-1};
    bool Saturated=false;
    const FVector Up=R.GetUpVector();
    for(int I=0;I<4;++I)
    {
        const double Mix=SignY[I]*Torque.X/(4*ArmLengthM)-SignX[I]*Torque.Y/(4*ArmLengthM)+Spin[I]*Torque.Z/(4*YawReactionArmM);
        const double Target=(bAutonomousEnabled?Collective:ManualCollective)+Mix;
        const double PWM=ImperialPhysics::PwmForThrust(FMath::Max(.00001,Target),PhysicalParams.Motor,Voltage,bNonlinearMotors);
        Saturated|=Target<0||Target>MaxMotorThrustN*Voltage;
        ImperialPhysics::MotorStep(MotorStates[I],PWM,Step,PhysicalParams.Motor,Voltage,bNonlinearMotors);
    }
    // Resolve the same-step electrical budget before any rotor force/torque is
    // applied. This advances the battery exactly once and scales all rotors.
    PowerBudget=ImperialPhysics::ApplyBatteryPowerBudget(MotorStates,BatteryState,Step,PhysicalParams.Motor,PhysicalParams.Battery,PhysicalParams.Aero.DensityKgM3,bBatteryModel);
    Saturated|=PowerBudget.Limited;
    for(int I=0;I<4;++I)
    {
        MotorThrustN[I]=static_cast<float>(MotorStates[I].ThrustN);
        const FVector Rotor=Body->GetComponentLocation()+R.RotateVector(FVector(SignX[I]*ArmLengthM*100,SignY[I]*ArmLengthM*100,2.5));
        const FVector Position=Rotor/100.;
        const double Surface=ImperialPhysics::SurfaceLiftFactor(Position.Z,4.5-Position.Z,FMath::Min(8-FMath::Abs(Position.X),3.5-FMath::Abs(Position.Y)),PhysicalParams.Surface,bSurfaceModel);
        const double Wake=1.-.04*FMath::Clamp(-FVector::DotProduct(AirVelocityAt(Position),Up),0.,2.);
        Body->AddForceAtLocation(Up*MotorThrustN[I]*Surface*Wake*100,Rotor);
        Body->AddTorqueInRadians(Up*Spin[I]*YawReactionArmM*MotorThrustN[I]*10000);
    }
    if(Saturated)IntegralError*=FMath::Exp(-Step*5);
    if(PowerBudget.Limited)AutonomousVelocityIntegral*=FMath::Exp(-Step*5);
    std::array<double,4> Omega;for(int I=0;I<4;++I)Omega[I]=MotorStates[I].OmegaRadS;
    const auto Gyro=ImperialPhysics::GyroTorque({IdealRate.X,IdealRate.Y,IdealRate.Z},Omega,{-1,1,-1,1},PhysicalParams.Gyro);
    Body->AddTorqueInRadians(R.RotateVector(FVector(Gyro.X,Gyro.Y,Gyro.Z))*10000);
    ApplyAerodynamicDrag();
}
void ASoccerDronePawn::ApplyAerodynamicDrag()
{
    const FVector Relative=Body->GetPhysicsLinearVelocity()/100.-AirVelocityAt(GetActorLocation()/100.);
    const auto Force=ImperialPhysics::DragForce({Relative.X,Relative.Y,Relative.Z},PhysicalParams.Aero);
    Body->AddForce(FVector(Force.X,Force.Y,Force.Z)*100);
}
