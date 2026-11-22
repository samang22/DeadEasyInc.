#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BuildingDamageDecalComponent.generated.h"

class UMeshComponent;

/** Bounded, persistent surface damage visuals. Does not handle health or damage. */
UCLASS(ClassGroup=(Buildings),meta=(BlueprintSpawnableComponent))
class DEADEASYINC_API UBuildingDamageDecalComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UBuildingDamageDecalComponent();
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Damage Decals")
    TObjectPtr<class UMaterialInterface> DecalMaterial;
    // X is projection depth; runtime limits are 20 cm depth and 150 cm per surface axis.
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Damage Decals",meta=(ClampMin="1",ClampMax="150",Units="cm"))
    FVector DecalSize=FVector(8.f,30.f,30.f);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Damage Decals",meta=(ClampMin="1",Units="cm"))
    float MinimumDistance=40.f;
    void AddHit(const FHitResult& Hit);
    void AddSurfaceHit(UMeshComponent& Mesh, const FVector& Origin, const FHitResult* PointHit);
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void SpawnSurfaceDecal(UMeshComponent& Mesh, const FHitResult& Hit);
    UPROPERTY(Transient) TArray<TObjectPtr<class UDecalComponent>> Decals;
};
