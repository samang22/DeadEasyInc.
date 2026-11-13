#pragma once
#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "BorderPillar.generated.h"

/** Permanent border structure. Deliberately has no building health or repair component. */
UCLASS()
class DEADEASYINC_API ABorderPillar : public AStaticMeshActor
{
    GENERATED_BODY()
public:
    ABorderPillar();
    virtual float TakeDamage(float Amount, const FDamageEvent& Event, AController* InstigatorController, AActor* Causer) override;
protected:
    virtual void BeginPlay() override;
};
