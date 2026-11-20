#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZombieCorpseComponent.generated.h"

class ABorderZombie;
class USkeletalMeshComponent;
class UZombieSettings;

/** Owns ragdoll collision and one-way contact forces. The character schedules its updates. */
UCLASS(ClassGroup=(Zombies))
class DEADEASYINC_API UZombieCorpseComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UZombieCorpseComponent();
    void ActivateRagdoll(USkeletalMeshComponent& Mesh, const UZombieSettings* Settings);
    void ApplyCollision(USkeletalMeshComponent& Mesh, const UZombieSettings* Settings);
    void CapturePose(USkeletalMeshComponent& Mesh, TMap<FName, FTransform>& OutBodies) const;
    void RestorePose(USkeletalMeshComponent& Mesh, const TMap<FName, FTransform>& Bodies) const;
    void ApplyContactInfluences(const ABorderZombie& Corpse);
    void PushNearbyCorpses(const ABorderZombie& Mover, const FVector& Direction, float DeltaSeconds);

private:
    float ConsumePushForce(float RequestedForce, float Budget);

    uint64 LastPushFrame = MAX_uint64;
    float PushForceThisFrame = 0.f;
    float NextCorpseContactTime = 0.f;
};
