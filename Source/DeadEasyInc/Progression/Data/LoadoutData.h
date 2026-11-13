#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/SaveGame.h"
#include "LoadoutData.generated.h"

class AEquippedWeapon;
UENUM(BlueprintType)
enum class EWeaponCategory : uint8 { Ranged, Melee };

UCLASS(BlueprintType)
class DEADEASYINC_API UWeaponDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EWeaponCategory Category = EWeaponCategory::Ranged;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName RequiredResearch;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, int32> CraftCost;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSubclassOf<AEquippedWeapon> WeaponClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float Damage = 25.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) float Range = 10000.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.05")) float AttackInterval = 0.4f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 MagazineSize = 12;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.1")) float ReloadTime = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bAutomatic = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1",ClampMax="32")) int32 Pellets = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0",ClampMax="45")) float SpreadDegrees = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float MeleeRadius = 25.f;
};

UCLASS(BlueprintType)
class DEADEASYINC_API UResearchDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> Prerequisites;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, int32> Cost;
};

UCLASS(BlueprintType)
class DEADEASYINC_API ULoadoutSettings : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UWeaponDefinition>> Weapons;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UResearchDefinition>> Research;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, int32> StartingResources;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> StartingWeapons;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString SaveSlot = TEXT("DeadEasyInc_Dawn");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) float InteractionReach = 300.f;
};

USTRUCT(BlueprintType)
struct FLoadoutState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, int32> Resources;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> Research;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> OwnedWeapons;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, int32> Magazines;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Ranged;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Melee;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EWeaponCategory ActiveCategory = EWeaponCategory::Melee;
};

UCLASS()
class DEADEASYINC_API ULoadoutSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Version = 1;
    UPROPERTY() FString MapName;
    UPROPERTY() int32 Day = 1;
    UPROPERTY() FLoadoutState State;
};
