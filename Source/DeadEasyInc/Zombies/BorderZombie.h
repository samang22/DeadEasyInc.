#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BorderZombie.generated.h"

class UAnimSequence;
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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie|Corpse Pushing")
    float CorpsePushReach = 35.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie|Corpse Pushing")
    float CorpsePushAcceleration = 350.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie|Corpse Pushing")
    float CorpsePushMaxSpeed = 180.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie|Corpse Pushing")
    FName CorpsePushBone = TEXT("pelvis");
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void Think(float Now);
    bool BuildSegment(const FVector& Start,const FVector& End,TArray<FVector>& Points) const;
    bool PlanRoute(AActor* Goal,const FVector& Destination,TArray<FVector>& Points,class ABuilding*& Blocker) const;
    bool InspectRoute(const TArray<FVector>& Points,class ABuilding*& Blocker) const;
    void ChooseBuildingGoal();
    void FollowRoute(float DeltaSeconds);
    UPROPERTY(VisibleAnywhere, Category="Zombie|AI") TObjectPtr<AActor> CurrentObjective;
    UPROPERTY(VisibleAnywhere, Category="Zombie|AI") TObjectPtr<AActor> CurrentTarget;
    TArray<FVector> Route;
    int32 Waypoint = 1;
    int32 PlanRevision = INDEX_NONE;
    float NextDecisionTime = 0.f;
    float NextPathTime = 0.f;
    float LastProgressTime = 0.f;
    FVector LastProgressLocation;
    FVector TargetPoint(AActor* Target) const;
    bool CanReach(AActor* Target, float Range) const;
    void PlayAnimation(UAnimSequence* Animation, bool bLoop);
    void PushCorpses(const FVector& Direction, float DeltaSeconds);
    TWeakObjectPtr<AActor> AttackTarget;
    TObjectPtr<UAnimSequence> CurrentAnimation;
    float AttackTime = 0.f;
    float NextAttackTime = 0.f;
    bool bAttackPending = false;
    uint64 LastPushFrame = MAX_uint64;
    float PushForceThisFrame = 0.f;
};
