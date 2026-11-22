#include "Buildings/Building.h"
#include "Buildings/BuildingDamageDecalComponent.h"
#include "Engine/DamageEvents.h"
#include "Buildings/Data/BuildingSettings.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Zombies/AI/BuildingTargetRegistry.h"
#include "Zombies/BorderZombie.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"

ABuilding::ABuilding()
{
    DamageDecals=CreateDefaultSubobject<UBuildingDamageDecalComponent>(TEXT("DamageDecals"));
    CombatHealth = CreateDefaultSubobject<UCombatHealthComponent>(TEXT("CombatHealth"));
    GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    GetStaticMeshComponent()->SetCanEverAffectNavigation(false);
    TierTwoVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TierTwoVisual"));
    TierThreeVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TierThreeVisual"));
    for (auto* Visual : {TierTwoVisual.Get(),TierThreeVisual.Get()})
    { Visual->SetupAttachment(GetStaticMeshComponent());Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);Visual->SetCanEverAffectNavigation(false);Visual->SetVisibility(false); }
    BuildingRepairAnchor=CreateDefaultSubobject<UBoxComponent>(TEXT("BuildingRepairAnchor"));
    BuildingRepairAnchor->SetupAttachment(GetStaticMeshComponent());BuildingRepairAnchor->SetCanEverAffectNavigation(false);
    BuildingRepairAnchor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BuildingRepairAnchor->SetCollisionResponseToAllChannels(ECR_Ignore);
    BuildingRepairAnchor->SetCollisionResponseToChannel(ECC_GameTraceChannel2,ECR_Block);
}

float ABuilding::TakeDamage(float Amount,const FDamageEvent& Event,AController* InstigatorController,AActor* Causer)
{
    const float PreviousHealth=CombatHealth->Health;
    const float Applied=Super::TakeDamage(Amount,Event,InstigatorController,Causer);
    if (Applied <= 0.f || CombatHealth->Health >= PreviousHealth || IsActorBeingDestroyed())
        return Applied;

    const FHitResult* PointHit = Event.IsOfType(FPointDamageEvent::ClassID)
        ? &static_cast<const FPointDamageEvent&>(Event).HitInfo : nullptr;
    if (const ABorderZombie* Zombie = Cast<ABorderZombie>(Causer))
        DamageDecals->AddSurfaceHit(*GetStaticMeshComponent(), Zombie->GetActorLocation(), PointHit);
    else if (PointHit)
        DamageDecals->AddHit(*PointHit);
    return Applied;
}

void ABuilding::BeginPlay()
{
    Super::BeginPlay();
    FVector Min,Max;GetStaticMeshComponent()->GetLocalBounds(Min,Max);
    BuildingRepairAnchor->SetRelativeLocation((Min+Max)*.5f);BuildingRepairAnchor->SetBoxExtent((Max-Min)*.5f+FVector(1));
    if (!DamageDecals->DecalMaterial)
    {
        const UBuildingSettings* DecalSettings=Settings ? Settings.Get() : GetClass()->GetDefaultObject<ABuilding>()->Settings.Get();
        if (DecalSettings) DamageDecals->DecalMaterial=DecalSettings->DamageDecalMaterial;
    }
    if (Settings)
    {
        CombatHealth->MaxHealth = FMath::Max(1.f, Settings->MaxHealth);
        CombatHealth->Health = CombatHealth->MaxHealth;
    }
    CombatHealth->OnHealthChanged.AddDynamic(this, &ABuilding::UpdateCondition);
    CombatHealth->OnDepleted.AddDynamic(this, &ABuilding::HandleBuildingDestroyed);
    UpdateCondition(CombatHealth->Health, CombatHealth->MaxHealth);
    RestoreUpgrade(UpgradeLevel);
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
    // Preserve imported mesh materials; surface damage is an optional decal layer.
    const bool bChanged = NewCondition != Condition;
    if ((NewCondition==EBuildingCondition::Destroyed)!=(Condition==EBuildingCondition::Destroyed))
        GetWorld()->GetSubsystem<UBuildingTargetRegistry>()->GeometryChanged();
    Condition = NewCondition;
    TierTwoVisual->SetVisibility(UpgradeLevel>=2 && Condition!=EBuildingCondition::Destroyed);
    TierThreeVisual->SetVisibility(UpgradeLevel>=3 && Condition!=EBuildingCondition::Destroyed);
    ApplyConditionState();
    if (bChanged) OnConditionChanged(Condition);
}

void ABuilding::ApplyConditionState()
{
    BuildingRepairAnchor->SetCollisionEnabled(Condition==EBuildingCondition::Destroyed ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
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
