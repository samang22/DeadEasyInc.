#include "Buildings/Building.h"
#include "Buildings/Data/BuildingSettings.h"
#include "Progression/PlayerLoadoutComponent.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Components/StaticMeshComponent.h"

FString ABuilding::UpgradeStatus(UPlayerLoadoutComponent* Player) const
{
    if (!Settings || !Settings->Levels.IsValidIndex(UpgradeLevel)) return TEXT("Maximum level");
    if (!bRepairAllowed || Condition==EBuildingCondition::Destroyed) return TEXT("Repair first / day only");
    const auto& Next=Settings->Levels[UpgradeLevel];
    if (!Player || (!Next.RequiredResearch.IsNone() && !Player->State.Research.Contains(Next.RequiredResearch))) return TEXT("Research required");
    for (const auto& Cost : Next.Cost) if (Cost.Value<0 || Player->GetResource(Cost.Key)<Cost.Value) return TEXT("Not enough materials");
    return TEXT("Upgrade");
}
bool ABuilding::Upgrade(UPlayerLoadoutComponent* Player)
{
    if (UpgradeStatus(Player)!=TEXT("Upgrade")) return false;
    for (const auto& Cost : Settings->Levels[UpgradeLevel].Cost) Player->SpendResource(Cost.Key,Cost.Value);
    RestoreUpgrade(UpgradeLevel+1);
    return true;
}
void ABuilding::RestoreUpgrade(int32 Level)
{
    UpgradeLevel=FMath::Clamp(Level,1,Settings ? FMath::Max(1,Settings->Levels.Num()) : 1);
    TierTwoVisual->SetVisibility(UpgradeLevel>=2 && Condition!=EBuildingCondition::Destroyed);
    TierThreeVisual->SetVisibility(UpgradeLevel>=3 && Condition!=EBuildingCondition::Destroyed);
    OnUpgradeApplied(UpgradeLevel);
}
