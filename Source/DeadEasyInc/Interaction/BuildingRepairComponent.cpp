#include "Interaction/BuildingRepairComponent.h"
#include "Interaction/Data/RepairSettings.h"
#include "Buildings/Building.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UBuildingRepairComponent::UBuildingRepairComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UBuildingRepairComponent::BeginPlay()
{
    Super::BeginPlay();
    if (Settings)
    {
        Wood = FMath::Max(0, Settings->StartingWood);
        RepairPerStroke = Settings->RepairPerStroke;
        StrokeInterval = Settings->StrokeInterval;
        Reach = Settings->Reach;
    }
}

void UBuildingRepairComponent::StartRepair() { bRepairHeld = true; }
int32 UBuildingRepairComponent::GetWoodPerStroke() const { return Settings ? FMath::Max(1, Settings->WoodPerStroke) : 1; }
void UBuildingRepairComponent::StopRepair() { bRepairHeld = false; }

ABuilding* UBuildingRepairComponent::GetFocusedBuilding() const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    const APlayerController* Player = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!Player || !Player->IsLocalController()) return nullptr;
    FVector Eye;
    FRotator Rotation;
    Player->GetPlayerViewPoint(Eye, Rotation);
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(WallRepair), false, GetOwner());
    GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Rotation.Vector() * Reach, ECC_Visibility, Params);
    return Cast<ABuilding>(Hit.GetActor());
}

void UBuildingRepairComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bRepairHeld || Wood < GetWoodPerStroke() || GetOwner()->ActorHasTag(TEXT("Dead"))) return;
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now < NextStrokeTime) return;
    ABuilding* Wall = GetFocusedBuilding();
    if (!Wall || !Wall->CanRepair()) return;
    const float Restored = Wall->Repair(FMath::Max(0.f, RepairPerStroke));
    if (Restored > 0.f)
    {
        Wood -= GetWoodPerStroke();
        NextStrokeTime = Now + FMath::Max(0.1f, StrokeInterval);
        OnRepairStroke(Wall, Restored);
    }
}
