#pragma once
#include "CoreMinimal.h"
#include "Buildings/Data/BuildingSettings.h"
#include "BoilerSettings.generated.h"

UCLASS(BlueprintType)
class DEADEASYINC_API UBoilerSettings : public UBuildingSettings
{
    GENERATED_BODY()
public:
    UBoilerSettings() { MaxHealth = 3000.f; DisplayName = FText::FromString(TEXT("Central Boiler")); ZombieTargetPriority = -1000; }
};
