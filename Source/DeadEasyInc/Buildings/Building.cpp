#include "Buildings/Building.h"
#include "Buildings/Data/BuildingSettings.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Components/StaticMeshComponent.h"

ABuilding::ABuilding()
{
    CombatHealth = CreateDefaultSubobject<UCombatHealthComponent>(TEXT("CombatHealth"));
    GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
}

void ABuilding::BeginPlay()
{
    Super::BeginPlay();
    if (Settings)
    {
        CombatHealth->MaxHealth = FMath::Max(1.f, Settings->MaxHealth);
        CombatHealth->Health = CombatHealth->MaxHealth;
    }
    CombatHealth->OnHealthChanged.AddDynamic(this, &ABuilding::UpdateCondition);
    CombatHealth->OnDepleted.AddDynamic(this, &ABuilding::HandleBuildingDestroyed);
    UpdateCondition(CombatHealth->Health, CombatHealth->MaxHealth);
}

void ABuilding::UpdateCondition(float Health, float MaxHealth)
{
    const float Ratio = MaxHealth > 0.f ? Health / MaxHealth : 0.f;
    const EBuildingCondition NewCondition = Ratio <= 0.f ? EBuildingCondition::Destroyed :
        Ratio <= (Settings ? Settings->CriticalThreshold : 0.35f) ? EBuildingCondition::Critical : Ratio <= (Settings ? Settings->DamagedThreshold : 0.75f) ? EBuildingCondition::Damaged : EBuildingCondition::Healthy;
    UMaterialInterface* Material = NewCondition == EBuildingCondition::Healthy ? HealthyMaterial.Get() :
        NewCondition == EBuildingCondition::Damaged ? DamagedMaterial.Get() : CriticalMaterial.Get();
    if (Material)
        for (int32 Index = 0; Index < GetStaticMeshComponent()->GetNumMaterials(); ++Index)
            GetStaticMeshComponent()->SetMaterial(Index, Material);
    const bool bChanged = NewCondition != Condition;
    Condition = NewCondition;
    ApplyConditionState();
    if (bChanged) OnConditionChanged(Condition);
}

void ABuilding::ApplyConditionState() {}

bool ABuilding::CanRepair() const
{
    return bRepairAllowed && (!Settings || Settings->bCanRepair) && CombatHealth->Health > 0.f && CombatHealth->Health < CombatHealth->MaxHealth;
}

float ABuilding::Repair(float Amount)
{
    return CanRepair() ? CombatHealth->RestoreHealth(Amount) : 0.f;
}

FText ABuilding::GetBuildingName() const
{
    return Settings ? Settings->DisplayName : FText::FromString(TEXT("Building"));
}

void ABuilding::HandleBuildingDestroyed() { OnBuildingDestroyed(); }
