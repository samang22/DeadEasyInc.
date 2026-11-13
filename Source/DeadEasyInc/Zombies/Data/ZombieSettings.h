#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ZombieSettings.generated.h"

UCLASS(BlueprintType)
class DEADEASYINC_API UZombieSettings : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(ClampMin="0.2")) float DecisionInterval = .65f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(ClampMin="1")) float PathRefreshInterval = 2.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(ClampMin="1",ClampMax="8")) int32 PortalSearchCount = 4;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(ClampMin="0")) float MaxObjectiveDistance = 6000.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI", meta=(ClampMin="0")) float MaxPortalDetour = 2500.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings", meta=(ClampMin="1"))
    float MaxHealth = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings", meta=(ClampMin="0"))
    float WalkSpeed = 140.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings", meta=(ClampMin="0"))
    float DetectionRadius = 700.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings", meta=(ClampMin="0"))
    float AttackReach = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings", meta=(ClampMin="0"))
    float AttackDamage = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings", meta=(ClampMin="0.1"))
    float AttackInterval = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings", meta=(ClampMin="0"))
    float AttackWindup = 0.4f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings", meta=(ClampMin="0"))
    float CorpsePushReach = 35.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings", meta=(ClampMin="0"))
    float CorpsePushAcceleration = 350.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings", meta=(ClampMin="0"))
    float CorpsePushMaxSpeed = 180.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings")
    FName CorpsePushBone = TEXT("pelvis");
};
