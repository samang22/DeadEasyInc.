#pragma once
#include "CoreMinimal.h"
#include "Buildings/Building.h"
#include "BorderWall.generated.h"

UCLASS()
class DEADEASYINC_API ABorderWall : public ABuilding
{
    GENERATED_BODY()
public:
    ABorderWall();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wall|Appearance")
    TObjectPtr<class UStaticMeshComponent> Rubble;
protected:
    virtual void BeginPlay() override;
    virtual void ApplyConditionState() override;
private:
    ECollisionEnabled::Type IntactCollision = ECollisionEnabled::QueryAndPhysics;
};
