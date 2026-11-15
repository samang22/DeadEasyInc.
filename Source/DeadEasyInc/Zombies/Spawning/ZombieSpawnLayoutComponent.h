#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZombieSpawnLayoutComponent.generated.h"

/** Deterministic line-slot sampling and physical spawn clearance, without wave scheduling. */
UCLASS(ClassGroup=(Zombies))
class DEADEASYINC_API UZombieSpawnLayoutComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UZombieSpawnLayoutComponent();
    void Prepare(const class UZombieWaveSettings* Settings,int32 Seed);
    bool FindSpawnPoint(const FTransform& Transform,const class UZombieWaveSettings* Settings,TSubclassOf<class ABorderZombie> ZombieClass,FVector& Position);
private:
    void ShuffleSlots();
    FRandomStream Random;
    TArray<int32> Slots;
    int32 Cursor=0;
};
