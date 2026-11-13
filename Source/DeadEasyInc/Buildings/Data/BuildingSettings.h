#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BuildingSettings.generated.h"

UCLASS(BlueprintType)
class DEADEASYINC_API UBuildingSettings : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Building")
    FText DisplayName = FText::FromString(TEXT("Building"));
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Building", meta=(ClampMin="1"))
    float MaxHealth = 1000.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Building")
    bool bCanRepair = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Zombie AI") bool bZombieTarget = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Zombie AI") int32 ZombieTargetPriority = 100;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Building", meta=(ClampMin="0.01",ClampMax="1"))
    float RestorationHealthRatio = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Building", meta=(ClampMin="0",ClampMax="1"))
    float DamagedThreshold = 0.75f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Building", meta=(ClampMin="0",ClampMax="1"))
    float CriticalThreshold = 0.35f;
};
