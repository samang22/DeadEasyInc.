#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BorderZombie.generated.h"

class UAnimSequence;
class UZombieCorpseComponent;
class UZombieNavigationComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBorderZombieDied);

UCLASS()
class DEADEASYINC_API ABorderZombie : public ACharacter
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable) FBorderZombieDied OnDied;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings")
    TObjectPtr<class UZombieSettings> Settings;
    ABorderZombie();
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float Amount, const FDamageEvent& Event, AController* InstigatorController, AActor* Causer) override;
    bool IsHeadshot(const FHitResult& Hit) const;
    UFUNCTION(BlueprintPure,Category="Combat") TArray<FName> GetHeadshotPhysicsBodies() const;

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Zombie")
    TObjectPtr<AActor> WallTarget;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie")
    float Health = 100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie")
    float DetectionRadius = 700.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie")
    float AttackReach = 100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie")
    float AttackDamage = 10.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie")
    float AttackInterval = 1.5f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Zombie|Animation")
    TObjectPtr<UAnimSequence> WalkAnimation;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Zombie|Animation")
    TObjectPtr<UAnimSequence> IdleAnimation;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Zombie|Animation")
    TObjectPtr<UAnimSequence> AttackAnimation;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie")
    bool bDead = false;
    TWeakObjectPtr<class AHandcart> Carrier;
    TWeakObjectPtr<class UTractorIntakeComponent> IntakeOwner; // Reservation while pulling or stored.
    TWeakObjectPtr<class ACorpseProcessor> ProcessingOwner;
    TWeakObjectPtr<class UCorpseHandlingComponent> HandlingOwner;
    int32 TransportSlot=INDEX_NONE;
    void ApplyCorpseCollision();
    void TickCorpseInfluences(float DeltaSeconds);
    UZombieCorpseComponent* GetCorpsePhysics() const { return CorpsePhysics.Get(); }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie|Corpse Pushing")
    float CorpsePushReach = 35.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie|Corpse Pushing")
    float CorpsePushAcceleration = 350.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie|Corpse Pushing")
    float CorpsePushMaxSpeed = 180.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie|Corpse Pushing")
    FName CorpsePushBone = TEXT("pelvis");
    
public:
    void ApplyDayReinforcement(float Multiplier);
    
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(VisibleAnywhere, Category="Zombie|AI") TObjectPtr<AActor> CurrentObjective;
    UPROPERTY(VisibleAnywhere, Category="Zombie|AI") TObjectPtr<AActor> CurrentTarget;
    void PlayAnimation(UAnimSequence* Animation, bool bLoop);
    TWeakObjectPtr<AActor> AttackTarget;
    TObjectPtr<UAnimSequence> CurrentAnimation;
    float AttackTime = 0.f;
    float NextAttackTime = 0.f;
    bool bAttackPending = false;
    UPROPERTY() TObjectPtr<UZombieCorpseComponent> CorpsePhysics;
    UPROPERTY() TObjectPtr<UZombieNavigationComponent> Navigation;
};
