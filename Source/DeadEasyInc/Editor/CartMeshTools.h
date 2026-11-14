#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CartMeshTools.generated.h"

UCLASS()
class DEADEASYINC_API UCartMeshTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Editor|Cart",meta=(DevelopmentOnly))
    static TArray<FString> DescribeParts(class UStaticMesh* Source);
    UFUNCTION(BlueprintCallable,Category="Editor|Cart",meta=(DevelopmentOnly))
    static bool SplitParts(class UStaticMesh* Source,const TArray<int32>& SelectedParts,class UStaticMesh* Body,class UStaticMesh* Tailgate);
};
