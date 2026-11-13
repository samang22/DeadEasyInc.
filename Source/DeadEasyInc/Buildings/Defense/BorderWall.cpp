#include "Buildings/Defense/BorderWall.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"

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
    RepairTarget = CreateDefaultSubobject<UBoxComponent>(TEXT("RepairTarget"));
    RepairTarget->SetupAttachment(GetStaticMeshComponent());
    RepairTarget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RepairTarget->SetCollisionResponseToAllChannels(ECR_Ignore);
    RepairTarget->SetCollisionResponseToChannel(ECC_GameTraceChannel2,ECR_Block);
    RepairTarget->SetCanEverAffectNavigation(false);
}

void ABorderWall::BeginPlay()
{
    IntactCollision = GetStaticMeshComponent()->GetCollisionEnabled();
    FVector Min,Max;
    GetStaticMeshComponent()->GetLocalBounds(Min,Max);
    RepairTarget->SetRelativeLocation((Min+Max)*.5f);
    RepairTarget->SetBoxExtent((Max-Min)*.5f+FVector(1));
    Super::BeginPlay();
}

void ABorderWall::ApplyConditionState()
{
    const bool bBroken = Condition == EBuildingCondition::Destroyed;
    // Keep the actor and its health for checkpoint restoration and future rebuilding.
    GetStaticMeshComponent()->SetVisibility(!bBroken, false);
    GetStaticMeshComponent()->SetCollisionEnabled(bBroken ? ECollisionEnabled::NoCollision : IntactCollision);
    Rubble->SetVisibility(bBroken);
    RepairTarget->SetCollisionEnabled(bBroken ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

bool ABorderWall::CanRestoreCollision() const
{
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(WallRestoration),false,this);
    TArray<FOverlapResult> Overlaps;
    GetWorld()->OverlapMultiByObjectType(Overlaps,RepairTarget->GetComponentLocation(),RepairTarget->GetComponentQuat(),Objects,
        FCollisionShape::MakeBox(RepairTarget->GetScaledBoxExtent()),Params);
    for (const FOverlapResult& Hit : Overlaps)
        if (Cast<APawn>(Hit.GetActor()) || (Hit.GetComponent() && Hit.GetComponent()->IsSimulatingPhysics())) return false;
    return true;
}
