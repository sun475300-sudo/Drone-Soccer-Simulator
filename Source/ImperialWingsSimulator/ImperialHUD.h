#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PilotResponse.h"
#include "ImperialHUD.generated.h"
class AImperialMatchManager;
class UImperialRadioSubsystem;
class AImperialPresentationEffects;

UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    virtual void NotifyHitBoxClick(FName BoxName) override;
    bool IsPointerOverUI(FVector2D Position) const;
    void ToggleRadioPanel();
    void ToggleFirstFlightGuide();
    bool IsFirstFlightGuideVisible() const { return bFirstFlightVisible; }
    void AdvanceRadioSetup();
    void ClickPointer();
    void ToggleCompactView() { bCompactView=!bCompactView; }
    bool IsRadioPanelOpen() const { return bRadioPanelOpen; }
    bool IsResponseAdvanced() const { return bResponsePanel && bResponseAdvanced; }
    bool IsPilotMenuOpen() const { return bRadioPanelOpen||bAIPracticePanel; }
    bool IsAIPracticePanelOpen() const { return bAIPracticePanel; }
    void ToggleAIPracticePanel();
private:
    UPROPERTY() TWeakObjectPtr<AImperialMatchManager> Match;
    float Scale = 1;
    float ButtonTextSize = 13;
    FVector2D Origin = FVector2D::ZeroVector;
    FVector2D Pointer = FVector2D(-1000, -1000);
    TArray<FBox2D> Bounds;
    bool bRadioPanelOpen = false;
    bool bFirstFlightVisible = false;
    bool bAIPracticePanel=false;
    bool bAIResultPage=false,bAIDraftDuel=false;
    int32 AIDraftCount=3,AIDraftDifficulty=0;
    uint32 AIDraftSeed=1;
    void DrawAIPracticePanel(AImperialMatchManager& Manager);
    void DrawAIResultPanel(AImperialMatchManager& Manager);
    bool bRadioAdvanced = false;
    bool bRadioConnectionHelp = false;
    FString LanguageMessage;
    bool bResponsePanel=false,bResponseAdvanced=false,bLearnArm=false,bDraftArmToggle=true;
    ImperialInput::ButtonLearner ArmLearner;
    FString ArmLearningDevice;
    int32 DraftArmButton=INDEX_NONE;
    FString DraftArmDevice,ResponseMessage;
    ImperialInput::PilotResponse DraftResponse;
    bool bCompactView = false;
    bool bCheckedFirstRun = false;
    struct FButtonTarget { FName Name; FBox2D Bounds; int32 Priority; };
    TArray<FButtonTarget> ButtonTargets;
    int32 ButtonPriority = 10;
    AImperialMatchManager* FindMatch();
    UImperialRadioSubsystem* GetRadio() const;
    AImperialPresentationEffects* GetPresentation();
    UPROPERTY() TWeakObjectPtr<AImperialPresentationEffects> Presentation;
    void DrawRadioPanel(AImperialMatchManager& Manager);
    void DrawFirstFlightGuide();
    void DrawResponsePanel(UImperialRadioSubsystem& Radio);
    void BeginResponsePanel();
    FVector2D Screen(float X, float Y) const;
    void Rect(float X, float Y, float W, float H, FLinearColor Color);
    void Text(const FString& Value, float X, float Y, float Size, FLinearColor Color, bool Right = false);
    FString Fit(const FString& Value, float Size, float Width) const;
    void StatusText(const FString& Value, float X, float Y, float Width, FLinearColor Color);
    void Panel(float X, float Y, float W, float H);
    void Button(FName Name, const FString& Label, float X, float Y, float W, float H = 36, bool Active = false, bool Warning = false);
};
