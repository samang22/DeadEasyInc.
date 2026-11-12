#pragma once
#include "CoreMinimal.h"
#include "Buildings/Data/BuildingSettings.h"
#include "WallSettings.generated.h"

UCLASS(BlueprintType)
class DEADEASYINC_API UWallSettings : public UBuildingSettings
{
    GENERATED_BODY()
public:
    UWallSettings() { DisplayName = FText::FromString(TEXT("Wall")); }
};
