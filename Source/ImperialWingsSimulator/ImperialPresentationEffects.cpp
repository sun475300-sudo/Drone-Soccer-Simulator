#include "ImperialPresentationEffects.h"
#include "ImperialUserData.h"
#include "SoccerDronePawn.h"
#include "ImperialBuildInfo.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"

AImperialPresentationEffects::AImperialPresentationEffects()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    auto* Scene = CreateDefaultSubobject<USceneComponent>(TEXT("PresentationRoot"));
    SetRootComponent(Scene);
    Dust = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ContactDust"));
    Dust->SetupAttachment(Scene);
    Dust->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Dust->SetGenerateOverlapEvents(false);
    Dust->SetCastShadow(false);
    Dust->SetCanEverAffectNavigation(false);
    Dust->SetReceivesDecals(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Sphere.Succeeded()) Dust->SetStaticMesh(Sphere.Object);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    GuardChips=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GuardWearFragments"));GuardChips->SetupAttachment(Scene);
    GuardChips->SetStaticMesh(Cube.Object);GuardChips->SetCollisionEnabled(ECollisionEnabled::NoCollision);GuardChips->SetCastShadow(false);
    for(int32 I=0;I<32;++I)GuardChips->AddInstance(FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector));
    Grade = CreateDefaultSubobject<UPostProcessComponent>(TEXT("ArenaPhotographicGrade"));
    Grade->SetupAttachment(Scene);
    Grade->bUnbound = true;
    Grade->Priority = 5;
}

void AImperialPresentationEffects::ApplyQuality()
{
    const bool bBalanced = QualityName.Equals(TEXT("Balanced"), ESearchCase::IgnoreCase);
    const bool bCinematic = QualityName.Equals(TEXT("Cinematic"), ESearchCase::IgnoreCase);
    QualityName = bBalanced ? TEXT("Balanced") : bCinematic ? TEXT("Cinematic") : TEXT("High");
    Capacity = bBalanced ? 96 : bCinematic ? 256 : 192;
    auto Set = [](const TCHAR* Name, float Value)
    {
        if (auto* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
            Variable->Set(Value, ECVF_SetByGameSetting);
    };
    // Native-resolution High by default. No forced window-size or saved settings changes.
    Set(TEXT("r.ScreenPercentage"), bBalanced ? 83.f : 100.f);
    Set(TEXT("r.MaxAnisotropy"), 16);
    Set(TEXT("r.Tonemapper.Sharpen"), .25f);
    auto& P = Grade->Settings;
    P.bOverride_BloomIntensity = true; P.BloomIntensity = .16f;
    P.bOverride_BloomThreshold = true; P.BloomThreshold = 2.f;
    P.bOverride_MotionBlurAmount = true; P.MotionBlurAmount = 0;
    P.bOverride_SceneFringeIntensity = true; P.SceneFringeIntensity = 0;
    P.bOverride_FilmGrainIntensity = true; P.FilmGrainIntensity = 0;
    P.bOverride_ColorContrast = true; P.ColorContrast = FVector4(1.025f, 1.025f, 1.025f, 1);
    P.bOverride_ColorSaturation = true; P.ColorSaturation = FVector4(.98f, .98f, .98f, 1);
    P.bOverride_LumenSceneLightingQuality = true;
    P.LumenSceneLightingQuality = bBalanced ? 1.f : bCinematic ? 2.f : 1.5f;
    P.bOverride_LumenFinalGatherQuality = true;
    P.LumenFinalGatherQuality = bBalanced ? 1.f : bCinematic ? 2.f : 1.5f;
    P.bOverride_LumenReflectionQuality = true;
    P.LumenReflectionQuality = bBalanced ? 1.f : bCinematic ? 2.f : 1.5f;

    // Broad, restrained fill improves cage/frame separation without point-light glare.
    if(FillLights.IsEmpty())for (int32 Side : {-1, 1})
    {
        auto* Fill = NewObject<URectLightComponent>(this);
        AddInstanceComponent(Fill);
        Fill->SetupAttachment(GetRootComponent());
        Fill->SetMobility(EComponentMobility::Movable);
        const FVector Position(Side * 850.f, 430.f, 510.f);
        Fill->SetRelativeLocation(Position);
        Fill->SetRelativeRotation((FVector(0, 0, 220) - Position).Rotation());
        Fill->SetIntensityUnits(ELightUnits::Lumens);
        Fill->SetIntensity(650);
        Fill->SetSourceWidth(180);
        Fill->SetSourceHeight(110);
        Fill->SetAttenuationRadius(1350);
        Fill->SetLightColor(Side < 0 ? FLinearColor(.89f, .94f, 1) : FLinearColor(1, .95f, .89f));
        Fill->SetCastShadows(false);
        Fill->RegisterComponent();
        FillLights.Add(Fill);
    }
    UE_LOG(LogTemp, Display, TEXT("IMPERIAL_PRESENTATION quality=%s dust_capacity=%d"), *QualityName, Capacity);
}
bool AImperialPresentationEffects::SetVisualQuality(const FString& Name,bool bSave)
{
    if(!Name.Equals(TEXT("Balanced"),ESearchCase::IgnoreCase)&&!Name.Equals(TEXT("High"),ESearchCase::IgnoreCase)&&!Name.Equals(TEXT("Cinematic"),ESearchCase::IgnoreCase))return false;
    QualityName=Name;ApplyQuality();
    if(Dust->GetInstanceCount()!=Capacity)
    {
        if(Motes.Num()>Capacity)Motes.SetNum(Capacity);
        Dust->ClearInstances();
        for(int32 I=0;I<Capacity;++I)Dust->AddInstance(FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector));
        PreviousVisible=0;
    }
    if(bSave)
    {
        const FString Directory=FPaths::Combine(ImperialUserData::Directory(),TEXT("Config"));
        IFileManager::Get().MakeDirectory(*Directory,true);
        if(!FFileHelper::SaveStringToFile(QualityName,*FPaths::Combine(Directory,TEXT("VisualQuality.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
            UE_LOG(LogTemp,Warning,TEXT("Could not save visual quality preference."));
    }
    return true;
}

void AImperialPresentationEffects::BeginPlay()
{
    Super::BeginPlay();
    if(auto* Surface=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Surface.M_Surface")))
    {auto* ChipMat=UMaterialInstanceDynamic::Create(Surface,this);ChipMat->SetVectorParameterValue(TEXT("Color"),FLinearColor(.64f,.65f,.60f));ChipMat->SetScalarParameterValue(TEXT("Glow"),0);GuardChips->SetMaterial(0,ChipMat);}
    bVerification = FParse::Param(FCommandLine::Get(), TEXT("ImperialEffectsSmoke"));
    if (FParse::Param(FCommandLine::Get(), TEXT("ImperialBaselineVisuals")))
    {
        Grade->bEnabled = false;
        SetActorTickEnabled(false);
        return;
    }
    FString Stored;
    if(FFileHelper::LoadFileToString(Stored,*FPaths::Combine(ImperialUserData::Directory(),TEXT("Config/VisualQuality.txt"))))QualityName=Stored.TrimStartAndEnd();
    FParse::Value(FCommandLine::Get(),TEXT("ImperialVisualQuality="),QualityName);
    ApplyQuality();
    bEffectsEnabled = !FParse::Param(FCommandLine::Get(), TEXT("ImperialNoDust"));
    if (auto* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Zone.M_Zone")))
    {
        DustMaterial = UMaterialInstanceDynamic::Create(Base, this);
        DustMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(.28f, .25f, .20f));
        DustMaterial->SetScalarParameterValue(TEXT("Glow"), 0);
        DustMaterial->SetScalarParameterValue(TEXT("Opacity"), .055f);
        Dust->SetMaterial(0, DustMaterial);
    }
    else bEffectsEnabled = false;
    Motes.Reserve(Capacity);
    // Fixed instance pool avoids allocating components or rebuilding meshes on collision.
    for (int32 I = 0; I < Capacity; ++I)
        Dust->AddInstance(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector));
    DiscoverAircraft();
}

void AImperialPresentationEffects::DiscoverAircraft()
{
    for (TActorIterator<ASoccerDronePawn> It(GetWorld()); It; ++It)
    {
        if (Aircraft.Contains(*It)) continue;
        Aircraft.Add(*It);
        It->OnActorHit.AddUniqueDynamic(this, &AImperialPresentationEffects::OnAircraftHit);
    }
    Aircraft.RemoveAll([](const auto& Item) { return !Item.IsValid(); });
    for (auto It = LastImpact.CreateIterator(); It; ++It)
        if (!It.Key().IsValid()) It.RemoveCurrent();
}

void AImperialPresentationEffects::AddMote(const FVector& Position, const FVector& Velocity, float Radius, float Lifetime)
{
    if (!bEffectsEnabled || Motes.Num() >= Capacity) return;
    FDustMote Mote;
    Mote.Position = Position; Mote.Velocity = Velocity;
    Mote.Radius = Radius; Mote.Lifetime = Lifetime;
    Motes.Add(Mote);
}

void AImperialPresentationEffects::EmitImpact(const FVector& Point, const FVector& Normal, float SpeedCmS)
{
    // Dust is confined to floor contact. No sparks, explosion or fictitious breakage.
    if (!bEffectsEnabled || Point.Z > 6 || Normal.Z < .5f || SpeedCmS < 100) return;
    const int32 Count = FMath::Clamp(FMath::RoundToInt(SpeedCmS / 25), 5, 24);
    for (int32 I = 0; I < Count; ++I)
    {
        const float Angle = Random.FRandRange(0, 2 * PI);
        const FVector Radial(FMath::Cos(Angle), FMath::Sin(Angle), 0);
        AddMote(Point + FVector(0, 0, 1.5f) + Radial * Random.FRandRange(0, 5),
            Radial * Random.FRandRange(15, 65) + FVector(0, 0, Random.FRandRange(5, 18)),
            Random.FRandRange(.35f, .9f), Random.FRandRange(.35f, .85f));
    }
    ++ImpactBursts;
}

void AImperialPresentationEffects::EmitGuardChips(const FVector& Point,const FVector& Normal,float ImpulseNs)
{
    if(!bEffectsEnabled||ImpulseNs<1.4f)return;
    const int32 Count=FMath::Clamp(FMath::RoundToInt(ImpulseNs*2),3,12);
    for(int32 I=0;I<Count&&Chips.Num()<32;++I)
    {FGuardChip Chip;Chip.Position=Point+Normal*.5f;Chip.Velocity=Normal*Random.FRandRange(35,110)+Random.VRand()*Random.FRandRange(15,65);Chip.Lifetime=Random.FRandRange(.22f,.65f);Chip.Size=Random.FRandRange(.08f,.24f);Chips.Add(Chip);}
    ++GuardChipBursts;
}
void AImperialPresentationEffects::OnAircraftHit(AActor* SelfActor, AActor* OtherActor,
    FVector NormalImpulse, const FHitResult& Hit)
{
    if (!bEffectsEnabled || !SelfActor || NormalImpulse.Size() < 100.f) return;
    const double* Last = LastImpact.Find(SelfActor);
    if (Last && Elapsed - *Last < .18) return;
    LastImpact.Add(SelfActor, Elapsed);
    EmitGuardChips(Hit.ImpactPoint,Hit.ImpactNormal,NormalImpulse.Size()*.01f);
    // UE impulse uses kg cm/s. Dividing by mass estimates contact delta-v in cm/s.
    const auto* Drone = Cast<ASoccerDronePawn>(SelfActor);
    const float Mass = Drone ? Drone->GetBodyMesh()->GetMass() : 1.f;
    EmitImpact(Hit.ImpactPoint, Hit.ImpactNormal, NormalImpulse.Size() / FMath::Max(.1f, Mass));
}

void AImperialPresentationEffects::Tick(float Dt)
{
    Super::Tick(Dt);
    Elapsed += Dt;
    DiscoveryClock += Dt;
    if (DiscoveryClock >= 1) { DiscoveryClock = 0; DiscoverAircraft(); }
    DownwashClock += Dt;
    if (DownwashClock >= .08f && bEffectsEnabled && !bVerification)
    {
        DownwashClock = 0;
        for (const auto& Item : Aircraft)
        {
            auto* Drone = Item.Get();
            if (!Drone || !Drone->IsArmed() || Drone->GetTotalThrustNewtons() < 3) continue;
            const FVector Position = Drone->GetActorLocation();
            if (Position.Z > 65 || Position.Z < 18) continue;
            FHitResult Ground;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(RotorDust), false, Drone);
            if (!GetWorld()->LineTraceSingleByChannel(Ground, Position,
                Position - FVector(0, 0, 80), ECC_WorldStatic, Query) || Ground.ImpactNormal.Z < .9) continue;
            if (Ground.ImpactPoint.Z > 3) continue;
            const float Angle = Random.FRandRange(0, 2 * PI);
            const FVector Radial(FMath::Cos(Angle), FMath::Sin(Angle), 0);
            AddMote(Ground.ImpactPoint + Radial * Random.FRandRange(12, 24) + FVector(0, 0, 1),
                Radial * Random.FRandRange(25, 65), .4f, .45f);
            ++RotorBursts;
        }
    }
    // Explicit cosmetic fixture: stress the pool, then verify all particles expire.
    // It does not arm aircraft or claim to validate real collisions.
    if (bVerification && !bFixtureRun && Elapsed >= 1)
    {
        for (int32 I = 0; I < 24; ++I) EmitImpact(FVector(0, 0, 0), FVector::UpVector, 500);
        FixturePeak = Motes.Num(); bFixtureRun = true;
        for (int32 I = 0; I < 3; ++I) EmitGuardChips(FVector(0, 0, 30), FVector::UpVector, 12.f);
        FixtureGuardPeak = Chips.Num();
        FixtureGuardStage = 1;
    }
    for (int32 I = Motes.Num() - 1; I >= 0; --I)
    {
        auto& M = Motes[I];
        M.Age += Dt;
        if (M.Age >= M.Lifetime) { Motes.RemoveAtSwap(I); continue; }
        const float Step = FMath::Min(Dt, .05f);
        M.Velocity *= FMath::Exp(-2.f * Step);
        M.Position += M.Velocity * Step;
    }
    const int32 Updates = FMath::Max(PreviousVisible, Motes.Num());
    for (int32 I = 0; I < Updates; ++I)
    {
        FTransform Transform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
        if (Motes.IsValidIndex(I))
        {
            const auto& M = Motes[I];
            const float Life = M.Age / M.Lifetime;
            const float Scale = M.Radius * .02f * FMath::Sin(PI * Life);
            Transform = FTransform(FQuat::Identity, M.Position, FVector(Scale, Scale, Scale * .45f));
        }
        Dust->UpdateInstanceTransform(I, Transform, false, I == Updates - 1, true);
    }
    PreviousVisible = Motes.Num();
    for(int32 I=Chips.Num()-1;I>=0;--I)
    {auto& Chip=Chips[I];Chip.Age+=Dt;if(Chip.Age>=Chip.Lifetime){Chips.RemoveAtSwap(I);continue;}const float Step=FMath::Min(Dt,.05f);Chip.Velocity.Z-=981*Step;Chip.Position+=Chip.Velocity*Step;}
    // Retire formerly visible slots once; an empty pool needs no instance updates.
    const int32 ChipUpdates = FMath::Max(PreviousVisibleChips, Chips.Num());
    for(int32 I=0;I<ChipUpdates;++I)
    {FTransform T(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector);if(Chips.IsValidIndex(I)){const auto& Chip=Chips[I];const float Scale=Chip.Size/100.f;T=FTransform(FRotator(Chip.Age*370,Chip.Age*290,Chip.Age*130),Chip.Position,FVector(Scale,Scale*.5f,Scale*.2f));}GuardChips->UpdateInstanceTransform(I,T,false,I==ChipUpdates-1,true);}
    PreviousVisibleChips = Chips.Num();
    if (bVerification && bFixtureRun)
    {
        FixtureGuardUpdateCalls += ChipUpdates;
        bFixtureGuardTransformsMatch &= GuardChips->GetInstanceCount() == 32;
        for (int32 I = 0; I < GuardChips->GetInstanceCount(); ++I)
        {
            FTransform Actual;
            const bool bRead = GuardChips->GetInstanceTransform(I, Actual, false);
            if (Chips.IsValidIndex(I))
            {
                const auto& Chip = Chips[I];
                const float Scale = Chip.Size / 100.f;
                bFixtureGuardTransformsMatch &= bRead && Actual.GetTranslation().Equals(Chip.Position, .01f)
                    && Actual.GetScale3D().Equals(FVector(Scale, Scale * .5f, Scale * .2f), 1.e-6f);
            }
            else bFixtureGuardTransformsMatch &= bRead && Actual.GetScale3D().IsNearlyZero(1.e-6f);
        }
        if (FixtureGuardStage == 1 && Chips.IsEmpty())
        {
            bFixtureGuardInitialCleared = true;
            FixtureGuardStage = 2;
        }
        else if (FixtureGuardStage == 2 && ChipUpdates == 0 && Motes.IsEmpty())
        {
            bFixtureGuardIdleZeroUpdates = true;
            EmitGuardChips(FVector(0, 0, 30), FVector::UpVector, 6.f);
            FixtureGuardReappearanceCount = Chips.Num();
            FixtureGuardStage = 3;
        }
        else if (FixtureGuardStage == 3)
        {
            if (!Chips.IsEmpty())
            {
                bFixtureGuardReappeared = true;
                if (!bFixtureGuardQualityChecked)
                {
                    bFixtureGuardQualityChecked = true;
                    const FString InitialQuality = QualityName;
                    const int32 InitialPrevious = PreviousVisibleChips;
                    const int32 InitialCount = Chips.Num();
                    TArray<FTransform> Before;
                    bool bQualityOk = true;
                    for (int32 I = 0; I < GuardChips->GetInstanceCount(); ++I)
                    {
                        FTransform Actual;
                        bQualityOk &= GuardChips->GetInstanceTransform(I, Actual, false);
                        Before.Add(Actual);
                    }
                    for (const TCHAR* Name : {TEXT("High"), TEXT("Balanced"), TEXT("Cinematic")})
                        bQualityOk &= SetVisualQuality(Name, false);
                    bQualityOk &= SetVisualQuality(InitialQuality, false);
                    bQualityOk &= Chips.Num() == InitialCount && PreviousVisibleChips == InitialPrevious
                        && GuardChips->GetInstanceCount() == Before.Num();
                    for (int32 I = 0; I < Before.Num(); ++I)
                    {
                        FTransform Actual;
                        bQualityOk &= GuardChips->GetInstanceTransform(I, Actual, false) && Actual.Equals(Before[I], .001f);
                    }
                    bFixtureGuardQualityPreserved = bQualityOk;
                }
            }
            else
            {
                bFixtureGuardFinalCleared = true;
                FixtureGuardStage = 4;
            }
        }
        else if (FixtureGuardStage == 4 && ChipUpdates == 0)
            bFixtureGuardFinalIdleZeroUpdates = true;
    }
    if (bVerification && !bFixtureReported && Elapsed >= 3)
    {
        bFixtureReported = true;
        const bool bGuardOk = FixtureGuardPeak == 32 && FixtureGuardReappearanceCount > 0
            && bFixtureGuardInitialCleared && bFixtureGuardIdleZeroUpdates && bFixtureGuardReappeared
            && bFixtureGuardFinalCleared && bFixtureGuardFinalIdleZeroUpdates
            && bFixtureGuardTransformsMatch && bFixtureGuardQualityPreserved;
        const bool bOk = bEffectsEnabled && FixturePeak == Capacity && Motes.IsEmpty()
            && Dust->GetInstanceCount() == Capacity && bGuardOk;
        auto Result=MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("ok"),bOk);Result->SetStringField(TEXT("fixture"),TEXT("cosmetic pool only; not flight physics"));
        Result->SetNumberField(TEXT("capacity"),Capacity);Result->SetNumberField(TEXT("peak"),FixturePeak);
        Result->SetNumberField(TEXT("remaining"),Motes.Num());Result->SetNumberField(TEXT("instances"),Dust->GetInstanceCount());
        Result->SetStringField(TEXT("quality"),QualityName);Result->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());
        Result->SetNumberField(TEXT("guard_chip_peak"), FixtureGuardPeak);
        Result->SetNumberField(TEXT("guard_chip_reappearance_count"), FixtureGuardReappearanceCount);
        Result->SetNumberField(TEXT("guard_chip_instances"), GuardChips->GetInstanceCount());
        Result->SetNumberField(TEXT("guard_chip_update_calls"), FixtureGuardUpdateCalls);
        Result->SetBoolField(TEXT("guard_chip_initial_cleared"), bFixtureGuardInitialCleared);
        Result->SetBoolField(TEXT("guard_chip_idle_zero_updates"), bFixtureGuardIdleZeroUpdates);
        Result->SetBoolField(TEXT("guard_chip_reappeared"), bFixtureGuardReappeared);
        Result->SetBoolField(TEXT("guard_chip_final_cleared"), bFixtureGuardFinalCleared);
        Result->SetBoolField(TEXT("guard_chip_final_idle_zero_updates"), bFixtureGuardFinalIdleZeroUpdates);
        Result->SetBoolField(TEXT("guard_chip_transforms_match"), bFixtureGuardTransformsMatch);
        Result->SetBoolField(TEXT("guard_chip_quality_preserved"), bFixtureGuardQualityPreserved);
        FString Report;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Report));
        FString ResultPath = FPaths::Combine(ImperialUserData::Directory(), TEXT("presentation-effects-smoke.json"));
        FParse::Value(FCommandLine::Get(), TEXT("ImperialEffectsResult="), ResultPath);
        FFileHelper::SaveStringToFile(Report, *ResultPath);
        UE_LOG(LogTemp, Display, TEXT("IMPERIAL_EFFECTS_SMOKE %s"), *Report);
        FPlatformMisc::RequestExit(false);
    }
}

void AImperialPresentationEffects::EndPlay(const EEndPlayReason::Type Reason)
{
    for (const auto& Item : Aircraft)
        if (auto* Drone = Item.Get()) Drone->OnActorHit.RemoveDynamic(this, &AImperialPresentationEffects::OnAircraftHit);
    Super::EndPlay(Reason);
}
