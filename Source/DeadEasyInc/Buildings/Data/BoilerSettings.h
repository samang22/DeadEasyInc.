#pragma once
#include "CoreMinimal.h"
#include "Buildings/Data/BuildingSettings.h"
#include "BoilerSettings.generated.h"

UCLASS(BlueprintType)
class DEADEASYINC_API UBoilerSettings : public UBuildingSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,BlueprintReadOnly) float StartingFuel=100.f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) float FuelPerSecond=0.5f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) float ColdDamagePerSecond=2.f;
    UBoilerSettings() { MaxHealth = 3000.f; DisplayName = FText::FromString(TEXT("Central Boiler")); ZombieTargetPriority = -1000; }
};
