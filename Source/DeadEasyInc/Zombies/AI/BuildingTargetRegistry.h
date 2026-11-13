#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BuildingTargetRegistry.generated.h"

class ABuilding;
class ABorderZombie;
struct FBorderOpening { FVector Center; FVector Normal; };

UCLASS()
class DEADEASYINC_API UBuildingTargetRegistry : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    void Register(ABuilding* Building);
    void Unregister(ABuilding* Building);
    void GeometryChanged() { ++Revision; }
    int32 GetRevision() const { return Revision; }
    TArray<ABuilding*> GetTargets() const;
    FVector ClaimApproach(ABuilding* Building, ABorderZombie* Zombie);
    void Release(ABorderZombie* Zombie);
    const TArray<FBorderOpening>& GetOpenings();
private:
    TSet<TWeakObjectPtr<ABuilding>> Buildings;
    TMap<TWeakObjectPtr<ABuilding>,TMap<TWeakObjectPtr<ABorderZombie>,int32>> Reservations;
    TArray<FBorderOpening> Openings;
    int32 Revision = 0;
    int32 CachedRevision = INDEX_NONE;
};
