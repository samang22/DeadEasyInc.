#include "Buildings/Heating/CentralBoiler.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Components/StaticMeshComponent.h"

ACentralBoiler::ACentralBoiler()
{
    Tags.AddUnique(TEXT("DefenseCore"));
    CombatHealth->MaxHealth = 3000.f;
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
