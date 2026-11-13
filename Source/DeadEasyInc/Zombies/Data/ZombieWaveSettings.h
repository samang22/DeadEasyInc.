#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ZombieWaveSettings.generated.h"

UCLASS(BlueprintType)
class DEADEASYINC_API UZombieWaveSettings : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="100")) float LineLength = 4000.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="70")) float SlotSpacing = 120.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float LateralJitter = 20.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float DepthJitter = 80.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0",ClampMax="10000")) int32 TotalZombies = 50;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 GroupSize = 10;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 MaxAlive = 30;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.05")) float SpawnInterval = .1f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.05")) float GroupInterval = 5.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.1")) float RetryInterval = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1",ClampMax="32")) int32 AttemptsPerStep = 8;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) float GroundSearchUp = 1500.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) float GroundSearchDown = 5000.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0",ClampMax="1")) float MinGroundNormalZ = .65f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 RandomSeed = 1337;
};
