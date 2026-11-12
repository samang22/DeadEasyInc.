#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PlayerSettings.generated.h"

UCLASS(BlueprintType)
class DEADEASYINC_API UPlayerSettings : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings", meta=(ClampMin="1"))
    float MaxHealth = 100.f;
};
