#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/SaveGame.h"
#include "Scavenging/ScavengingData.h"
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
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 RequiredWorkbenchLevel = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSubclassOf<AEquippedWeapon> WeaponClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float Damage = 25.f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="1")) float HeadshotMultiplier=3.f;
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
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 RequiredResearchLevel = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D TreePosition;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Description;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bImplemented=true;
};

UCLASS(BlueprintType)
class DEADEASYINC_API ULoadoutSettings : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<TObjectPtr<class UToolDefinition>> Tools;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UWeaponDefinition>> Weapons;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UResearchDefinition>> Research;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, int32> StartingResources;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> StartingWeapons;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> StartingTools={TEXT("Handcart")};
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString SaveSlot = TEXT("DeadEasyInc_Dawn");
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Save",meta=(DisplayName="Enable PIE Disk Saves")) bool bEnablePIESaves=false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) float InteractionReach = 300.f;
};

UCLASS(BlueprintType)
class DEADEASYINC_API UToolDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FName Id;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FName RequiredResearch;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) int32 RequiredWorkbenchLevel=1;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TMap<FName,int32> Cost;
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> OwnedTools;
    UPROPERTY() TArray<FCorpseCargo> Backpack;
    UPROPERTY() TArray<FCorpseCargo> Cart;
};

USTRUCT()
struct FDawnWorldRecord
{
    GENERATED_BODY()
    UPROPERTY() FName ActorName;
    UPROPERTY() TSoftClassPtr<AActor> ActorClass;
    UPROPERTY() FTransform Transform;
    UPROPERTY() float Health=0.f;
    UPROPERTY() float MaxHealth=0.f;
    UPROPERTY() float Restoration=0.f;
    UPROPERTY() int32 Level=1;
    UPROPERTY() float Fuel=0.f;
    UPROPERTY() bool bZombie=false;
    UPROPERTY() bool bCorpse=false;
    UPROPERTY() TMap<FName,FTransform> Bodies;
    UPROPERTY() int32 TransportSlot=INDEX_NONE;
    UPROPERTY() FName TransportCart;
    UPROPERTY() bool bHandcart=false;
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
    UPROPERTY() bool bHasWorld=false;
    UPROPERTY() TArray<FDawnWorldRecord> World;
};
