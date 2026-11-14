#include "Buildings/Heating/CentralBoiler.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Buildings/Data/BoilerSettings.h"
#include "Core/BorderGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

ACentralBoiler::ACentralBoiler()
{
    Tags.AddUnique(TEXT("DefenseCore"));
    PrimaryActorTick.bCanEverTick=true;
    CombatHealth->MaxHealth = 3000.f;
    // The core is never a temporary breach target, so paths should go around it.
    GetStaticMeshComponent()->SetCanEverAffectNavigation(true);
}

void ACentralBoiler::BeginPlay()
{
    Super::BeginPlay();
    if (const auto* Data=Cast<UBoilerSettings>(Settings)) Fuel=FMath::Clamp(Data->StartingFuel,0.f,Capacity());
    CombatHealth->OnDepleted.AddDynamic(this, &ACentralBoiler::HandleDestroyed);
}

float ACentralBoiler::Capacity() const
{
    return Settings && Settings->Levels.IsValidIndex(UpgradeLevel-1) ? FMath::Max(1.f,Settings->Levels[UpgradeLevel-1].FuelCapacity) : 200.f;
}
float ACentralBoiler::Efficiency() const
{
    return Settings && Settings->Levels.IsValidIndex(UpgradeLevel-1) ? FMath::Max(.1f,Settings->Levels[UpgradeLevel-1].FuelEfficiency) : 1.f;
}
float ACentralBoiler::AddFuel(float Amount)
{
    const float Added=FMath::Clamp(Amount,0.f,FMath::Max(0.f,Capacity()-Fuel));Fuel+=Added;return Added;
}
void ACentralBoiler::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const auto* Mode=GetWorld()->GetAuthGameMode<ABorderGameMode>();
    if (!Mode || !Mode->bIsNight || Mode->bGameOver || Mode->bTransitioning || bDestroyed) return;
    const auto* Data=Cast<UBoilerSettings>(Settings);
    const float Consumption=(Data ? Data->FuelPerSecond : .5f)/Efficiency();
    const float WarmTime=Consumption>0.f ? FMath::Min(DeltaSeconds,Fuel/Consumption) : DeltaSeconds;
    Fuel=FMath::Max(0.f,Fuel-Consumption*DeltaSeconds);
    if (WarmTime<DeltaSeconds)
        UGameplayStatics::ApplyDamage(UGameplayStatics::GetPlayerPawn(this,0),(DeltaSeconds-WarmTime)*(Data ? Data->ColdDamagePerSecond : 2.f),nullptr,this,nullptr);
}

void ACentralBoiler::HandleDestroyed()
{
    if (bDestroyed) return;
    bDestroyed = true;
    OnBoilerDestroyed();
}
