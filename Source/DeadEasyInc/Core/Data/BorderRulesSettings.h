#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BorderRulesSettings.generated.h"

UCLASS(BlueprintType)
class DEADEASYINC_API UBorderRulesSettings : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings", meta=(ClampMin="1"))
    float TransitionDuration = 5.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings", meta=(ClampMin="1"))
    float DawnDelay = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings", meta=(ClampMin="0.1"))
    float RetryDelay = 2.f;
};
