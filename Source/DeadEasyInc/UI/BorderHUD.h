#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BorderHUD.generated.h"

UCLASS()
class DEADEASYINC_API ABorderHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    virtual void NotifyHitBoxClick(FName BoxName) override;
private:
    void DrawPauseMenu(class ADeadEasyIncPlayerController* Player);
    void DrawLoadout(class UPlayerLoadoutComponent* Loadout);
    void DrawStationMenu(class UPlayerLoadoutComponent* Loadout);
    void DrawNightConfirmation(class UPlayerLoadoutComponent* Loadout);
    void DrawUpgrade(class UPlayerLoadoutComponent* Loadout);
    void DrawResearchTree(class UPlayerLoadoutComponent* Loadout);
    void DrawTransactionConfirmation(class UPlayerLoadoutComponent* Loadout);
    void DrawNotifications(class UPlayerLoadoutComponent* Loadout);
    FName SelectedResearch;
    FVector2D TreePan=FVector2D::ZeroVector;
    FVector2D LastMouse=FVector2D::ZeroVector;
    float TreeZoom=1.f;
    bool bDraggingTree=false;
};
