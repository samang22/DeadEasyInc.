#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RepairSettings.generated.h"

UCLASS(BlueprintType)
class DEADEASYINC_API URepairSettings : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings", meta=(ClampMin="0"))
    int32 StartingWood = 50;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings", meta=(ClampMin="1"))
    int32 WoodPerStroke = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings", meta=(ClampMin="0"))
    float RepairPerStroke = 50.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings", meta=(ClampMin="0.1"))
    float StrokeInterval = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings", meta=(ClampMin="0"))
    float Reach = 300.f;
};
