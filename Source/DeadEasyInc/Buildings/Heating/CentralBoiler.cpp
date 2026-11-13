#include "Buildings/Heating/CentralBoiler.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Components/StaticMeshComponent.h"

ACentralBoiler::ACentralBoiler()
{
    Tags.AddUnique(TEXT("DefenseCore"));
    CombatHealth->MaxHealth = 3000.f;
    // The core is never a temporary breach target, so paths should go around it.
    GetStaticMeshComponent()->SetCanEverAffectNavigation(true);
}

void ACentralBoiler::BeginPlay()
{
    Super::BeginPlay();
    CombatHealth->OnDepleted.AddDynamic(this, &ACentralBoiler::HandleDestroyed);
}

void ACentralBoiler::HandleDestroyed()
{
    if (bDestroyed) return;
    bDestroyed = true;
    OnBoilerDestroyed();
}
