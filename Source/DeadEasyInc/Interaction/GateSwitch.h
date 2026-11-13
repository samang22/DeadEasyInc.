#pragma once
#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "GateSwitch.generated.h"
UCLASS()
class DEADEASYINC_API AGateSwitch : public AStaticMeshActor
{
    GENERATED_BODY()
public:
    AGateSwitch();
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite) TObjectPtr<class ABorderGate> Gate;
};
