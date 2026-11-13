#include "Buildings/Defense/BorderPillar.h"
#include "Components/StaticMeshComponent.h"

ABorderPillar::ABorderPillar()
{
    SetCanBeDamaged(false);
    Tags.AddUnique(TEXT("BorderPillar"));
    GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
    GetStaticMeshComponent()->SetCanEverAffectNavigation(true);
}

void ABorderPillar::BeginPlay()
{
    Super::BeginPlay();
    SetCanBeDamaged(false);
    Tags.Remove(TEXT("BorderWall"));
    Tags.Remove(TEXT("DefenseCore"));
    Tags.AddUnique(TEXT("BorderPillar"));
}

float ABorderPillar::TakeDamage(float Amount, const FDamageEvent& Event, AController* InstigatorController, AActor* Causer)
{
    return 0.f;
}
