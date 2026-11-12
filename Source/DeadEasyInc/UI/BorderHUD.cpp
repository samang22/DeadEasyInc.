#include "UI/BorderHUD.h"
#include "Buildings/Building.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Interaction/BuildingRepairComponent.h"
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Core/BorderGameMode.h"

void ABorderHUD::DrawHUD()
{
    Super::DrawHUD();
    APawn* Pawn = PlayerOwner ? PlayerOwner->GetPawn() : nullptr;
    UBuildingRepairComponent* Repair = Pawn ? Pawn->FindComponentByClass<UBuildingRepairComponent>() : nullptr;
    if (!Canvas || !Repair) return;
    if (const ABorderGameMode* Mode = GetWorld()->GetAuthGameMode<ABorderGameMode>())
    {
        DrawText(FString::Printf(TEXT("Day %d - %s"), Mode->DayNumber, Mode->bIsNight ? TEXT("NIGHT") : TEXT("DAY")),
            FLinearColor::White, 30, 30);
        if (Mode->bGameOver)
        {
            DrawText(TEXT("GAME OVER - Returning to dawn..."), FLinearColor::Red,
                Canvas->SizeX * 0.5f - 180, Canvas->SizeY * 0.5f - 60);
            return;
        }
    }
    const float X = Canvas->SizeX * 0.5f;
    const float Y = Canvas->SizeY * 0.5f;
    DrawLine(X - 4, Y, X + 4, Y, FLinearColor::White);
    DrawLine(X, Y - 4, X, Y + 4, FLinearColor::White);
    DrawText(FString::Printf(TEXT("Wood: %d"), Repair->Wood), FLinearColor::White, 30, Canvas->SizeY - 50);
    ABuilding* Wall = Repair->GetFocusedBuilding();
    if (!Wall) return;
    DrawText(FString::Printf(TEXT("%s: %.0f / %.0f"), *Wall->GetBuildingName().ToString(), Wall->CombatHealth->Health, Wall->CombatHealth->MaxHealth),
        FLinearColor::White, X - 100, Y + 30);
    const TCHAR* Hint = Wall->Condition == EBuildingCondition::Destroyed ? TEXT("Destroyed - rebuilding not available") :
        !Wall->bRepairAllowed ? TEXT("Repairs unavailable at night") :
        !Wall->CanRepair() ? TEXT("Fully repaired") : Repair->Wood < Repair->GetWoodPerStroke() ? TEXT("Not enough wood") : TEXT("");
    const FString RepairHint = *Hint ? FString(Hint) : FString::Printf(TEXT("Hold E to repair (%d wood / stroke)"), Repair->GetWoodPerStroke());
    DrawText(RepairHint, FLinearColor::White, X - 100, Y + 55);
}
