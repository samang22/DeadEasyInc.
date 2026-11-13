#include "Buildings/Building.h"
#include "Buildings/Data/BuildingSettings.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Zombies/AI/BuildingTargetRegistry.h"
#include "Engine/World.h"

ABuilding::ABuilding()
{
    CombatHealth = CreateDefaultSubobject<UCombatHealthComponent>(TEXT("CombatHealth"));
    GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    GetStaticMeshComponent()->SetCanEverAffectNavigation(false);
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
    GetWorld()->GetSubsystem<UBuildingTargetRegistry>()->Register(this);
}

void ABuilding::EndPlay(const EEndPlayReason::Type Reason)
{
    if (UBuildingTargetRegistry* Registry=GetWorld()->GetSubsystem<UBuildingTargetRegistry>()) Registry->Unregister(this);
    Super::EndPlay(Reason);
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
    if ((NewCondition==EBuildingCondition::Destroyed)!=(Condition==EBuildingCondition::Destroyed))
        GetWorld()->GetSubsystem<UBuildingTargetRegistry>()->GeometryChanged();
    Condition = NewCondition;
    ApplyConditionState();
    if (bChanged) OnConditionChanged(Condition);
}

void ABuilding::ApplyConditionState()
{
    GetStaticMeshComponent()->SetCollisionEnabled(Condition==EBuildingCondition::Destroyed ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
}

bool ABuilding::CanRepair() const
{
    return bRepairAllowed && (!Settings || Settings->bCanRepair) && CombatHealth->Health < CombatHealth->MaxHealth &&
        (CombatHealth->Health > 0.f || CanRestoreCollision());
}

float ABuilding::Repair(float Amount)
{
    if (!CanRepair() || Amount <= 0.f) return 0.f;
    if (CombatHealth->Health > 0.f) return CombatHealth->RestoreHealth(Amount);
    const float Work = FMath::Min(Amount, GetRestorationTarget()-RestorationProgress);
    RestorationProgress += Work;
    if (RestorationProgress >= GetRestorationTarget())
    {
        CombatHealth->RestoreHealth(RestorationProgress,true);
        RestorationProgress = 0.f;
    }
    return Work;
}

bool ABuilding::CanRestoreCollision() const { return true; }
float ABuilding::GetRestorationTarget() const
{
    return CombatHealth->MaxHealth * FMath::Clamp(Settings ? Settings->RestorationHealthRatio : .25f,.01f,1.f);
}

FText ABuilding::GetBuildingName() const
{
    return Settings ? Settings->DisplayName : FText::FromString(TEXT("Building"));
}

void ABuilding::HandleBuildingDestroyed() { OnBuildingDestroyed(); }
