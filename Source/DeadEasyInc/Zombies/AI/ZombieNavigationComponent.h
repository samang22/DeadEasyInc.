#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZombieNavigationComponent.generated.h"

class ABorderZombie;
class ABuilding;
class ACharacter;
class UBuildingTargetRegistry;

/** Selects reachable objectives and tracks route progress. The character applies movement and attacks. */
UCLASS(ClassGroup=(Zombies))
class DEADEASYINC_API UZombieNavigationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UZombieNavigationComponent();
    void InitializeRoute(const ABorderZombie& Zombie);
    void UpdateTarget(ABorderZombie& Zombie, float Now, TObjectPtr<AActor>& CurrentObjective, TObjectPtr<AActor>& CurrentTarget);
    bool TryGetMoveDirection(const AActor& Mover, FVector& Direction);
    static FVector GetTargetPoint(const AActor& Seeker, const AActor& Target);
    static bool CanReach(const ACharacter& Seeker, AActor* Target, float Range);

private:
    bool BuildSegment(ABorderZombie& Zombie, const FVector& Start, const FVector& End, TArray<FVector>& Points, bool bAllowPartial = false) const;
    bool PlanRoute(ABorderZombie& Zombie, UBuildingTargetRegistry& Registry, const FVector& Destination, TArray<FVector>& Points, ABuilding*& Blocker) const;
    bool InspectRoute(const ABorderZombie& Zombie, const TArray<FVector>& Points, ABuilding*& Blocker) const;
    void ChooseBuildingGoal(ABorderZombie& Zombie, UBuildingTargetRegistry& Registry, TObjectPtr<AActor>& CurrentObjective, TObjectPtr<AActor>& CurrentTarget);

    TArray<FVector> Route;
    int32 Waypoint = 1;
    bool bUsingFallback = false;
    int32 PlanRevision = INDEX_NONE;
    float NextDecisionTime = 0.f;
    float NextPathTime = 0.f;
    float LastProgressTime = 0.f;
    FVector LastProgressLocation = FVector::ZeroVector;
};
