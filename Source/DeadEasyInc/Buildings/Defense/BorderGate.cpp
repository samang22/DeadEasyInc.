#include "Buildings/Defense/BorderGate.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Zombies/AI/BuildingTargetRegistry.h"

ABorderGate::ABorderGate()
{
    GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    PassageBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("PassageBlocker"));
    PassageBlocker->SetupAttachment(GetStaticMeshComponent());
    PassageBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PassageBlocker->SetCollisionResponseToAllChannels(ECR_Ignore);
    PassageBlocker->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
    PassageBlocker->SetCollisionResponseToChannel(ECC_PhysicsBody,ECR_Block);
    PassageBlocker->SetCanEverAffectNavigation(false);
}

void ABorderGate::BeginPlay()
{
    ClosedLocation = GetActorLocation();
    ClosedRubbleLocation = Rubble->GetComponentLocation();
    Super::BeginPlay();
    ClosedRepairLocation = RepairTarget->GetComponentLocation();
    bGateInitialized = true;
    PassageBlocker->SetBoxExtent(RepairTarget->GetUnscaledBoxExtent());
    SetOpenFraction(1.f);
}

void ABorderGate::SetOpenFraction(float Fraction)
{
    const bool bWasClosed=OpenFraction<=KINDA_SMALL_NUMBER;
    OpenFraction = FMath::Clamp(Fraction,0.f,1.f);
    if (bWasClosed!=(OpenFraction<=KINDA_SMALL_NUMBER))
        GetWorld()->GetSubsystem<UBuildingTargetRegistry>()->GeometryChanged();
    const bool bBroken = Condition == EBuildingCondition::Destroyed;
    SetActorLocation(ClosedLocation+FVector(0,0,bBroken ? 0.f : OpenHeight*OpenFraction),false,nullptr,ETeleportType::TeleportPhysics);
    Rubble->SetWorldLocation(ClosedRubbleLocation);
    RepairTarget->SetWorldLocation(ClosedRepairLocation);
    PassageBlocker->SetWorldLocation(ClosedRepairLocation);
    GetStaticMeshComponent()->SetCollisionEnabled(!bBroken && OpenFraction <= KINDA_SMALL_NUMBER ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    RepairTarget->SetCollisionEnabled(bBroken || OpenFraction > 0.f ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void ABorderGate::ApplyConditionState()
{
    Super::ApplyConditionState();
    if (bGateInitialized) SetOpenFraction(OpenFraction);
}

bool ABorderGate::IsPassageClear() const { return CanRestoreCollision(); }
void ABorderGate::SetTransitionBarrier(bool bEnabled)
{
    PassageBlocker->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}
