#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BorderHUD.generated.h"

class ABorderGameMode;
class ACorpseProcessor;
class AHandcart;
class UFont;
class UMaterialParameterCollection;

UCLASS()
class DEADEASYINC_API ABorderHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    virtual void NotifyHitBoxClick(FName BoxName) override;
private:
    UPROPERTY(EditDefaultsOnly, Category="Tutorial|Localization")
    TObjectPtr<UFont> ShiftGuideFont;

    void DrawPauseMenu(class ADeadEasyIncPlayerController* Player);
    AActor* DrawLoadout(class UPlayerLoadoutComponent* Loadout, bool& bUsingEquipment);
    void SetOutlineTarget(AActor* Target);
    void UpdateOutlineStrength(bool bTransitioning);
    UPROPERTY(EditDefaultsOnly, Category="Interaction|Outline")
    TObjectPtr<UMaterialParameterCollection> OutlineParameters;
    float OutlineStrength = 1.f;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    TWeakObjectPtr<AActor> OutlineActor;
    TWeakObjectPtr<class UInteractionOutlineComponent> OutlineComponent;
    void DrawStationMenu(class UPlayerLoadoutComponent* Loadout);
    void DrawNightConfirmation(class UPlayerLoadoutComponent* Loadout);
    void DrawUpgrade(class UPlayerLoadoutComponent* Loadout);
    void DrawResearchTree(class UPlayerLoadoutComponent* Loadout);
    void DrawTransactionConfirmation(class UPlayerLoadoutComponent* Loadout);
    void DrawNotifications(class UPlayerLoadoutComponent* Loadout);
    void DrawShiftGuide(const ABorderGameMode& Mode, const APawn& Pawn);
    TWeakObjectPtr<ACorpseProcessor> GuideSorter;
    TWeakObjectPtr<AHandcart> GuideCart;
    int32 GuideDay = 0;
    float GuidePhaseStart = 0.f;
    float GuideWaveStart = -1.f;
    bool bGuideNight = false;
    bool bReachedGuideSorter = false;
    bool bReachedGuideCart = false;
    FName SelectedResearch;
    FVector2D TreePan=FVector2D::ZeroVector;
    FVector2D LastMouse=FVector2D::ZeroVector;
    float TreeZoom=1.f;
    bool bDraggingTree=false;
};
