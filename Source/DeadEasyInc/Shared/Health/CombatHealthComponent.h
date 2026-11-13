#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCombatHealthChanged, float, Health, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCombatDepleted);

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class DEADEASYINC_API UCombatHealthComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health")
    float MaxHealth = 100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health")
    float Health = 100.f;
    UPROPERTY(BlueprintAssignable, Category="Health")
    FCombatHealthChanged OnHealthChanged;
    UPROPERTY(BlueprintAssignable, Category="Health")
    FCombatDepleted OnDepleted;
    UFUNCTION(BlueprintCallable, Category="Health")
    float RestoreHealth(float Amount, bool bAllowFromZero = false);
protected:
    virtual void BeginPlay() override;
private:
    UFUNCTION()
    void ReceiveDamage(AActor* Actor, float Damage, const class UDamageType* Type, AController* InstigatorController, AActor* Causer);
};
