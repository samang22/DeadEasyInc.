#include "Buildings/Defense/BorderWall.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Components/StaticMeshComponent.h"

ABorderWall::ABorderWall()
{
    Tags.AddUnique(TEXT("BorderWall"));
    CombatHealth->MaxHealth = 1000.f;
    // Walls block physically; AI handles the obstruction without rebuilding NavMesh.
    GetStaticMeshComponent()->SetCanEverAffectNavigation(false);
    Rubble = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rubble"));
    Rubble->SetupAttachment(GetStaticMeshComponent());
    Rubble->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Rubble->SetCanEverAffectNavigation(false);
    Rubble->SetVisibility(false);
}

void ABorderWall::BeginPlay()
{
    IntactCollision = GetStaticMeshComponent()->GetCollisionEnabled();
    Super::BeginPlay();
}

void ABorderWall::ApplyConditionState()
{
    const bool bBroken = Condition == EBuildingCondition::Destroyed;
    // Keep the actor and its health for checkpoint restoration and future rebuilding.
    GetStaticMeshComponent()->SetVisibility(!bBroken, false);
    GetStaticMeshComponent()->SetCollisionEnabled(bBroken ? ECollisionEnabled::NoCollision : IntactCollision);
    Rubble->SetVisibility(bBroken);
}
