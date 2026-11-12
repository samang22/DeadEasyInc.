#include "Shared/Health/CombatHealthComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UCombatHealthComponent::BeginPlay()
{
    Super::BeginPlay();
    Health = MaxHealth;
    GetOwner()->OnTakeAnyDamage.AddDynamic(this, &UCombatHealthComponent::ReceiveDamage);
}

float UCombatHealthComponent::RestoreHealth(float Amount)
{
    // Repair is deliberately separate from resurrection.
    if (Amount <= 0.f || Health <= 0.f) return 0.f;
    const float Previous = Health;
    Health = FMath::Min(MaxHealth, Health + Amount);
    if (Health > Previous) OnHealthChanged.Broadcast(Health, MaxHealth);
    return Health - Previous;
}

void UCombatHealthComponent::ReceiveDamage(AActor* Actor, float Damage, const UDamageType* Type, AController* InstigatorController, AActor* Causer)
{
    if (Damage <= 0.f || Health <= 0.f) return;
    Health = FMath::Max(0.f, Health - Damage);
    OnHealthChanged.Broadcast(Health, MaxHealth);
    UE_LOG(LogTemp, Display, TEXT("%s Health: %.0f / %.0f"), *Actor->GetName(), Health, MaxHealth);
    if (Health <= 0.f)
    {
        Actor->Tags.AddUnique(TEXT("Dead"));
        if (ACharacter* Character = Cast<ACharacter>(Actor)) Character->GetCharacterMovement()->DisableMovement();
        OnDepleted.Broadcast();
    }
}
