#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BorderNavigationTools.generated.h"

UCLASS()
class DEADEASYINC_API UBorderNavigationTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Editor|Navigation", meta=(DevelopmentOnly))
    static bool BuildStaticNavigation(class ANavMeshBoundsVolume* Bounds,FVector Dimensions);
};
