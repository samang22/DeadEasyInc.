#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ScavengingData.generated.h"

UENUM(BlueprintType)
enum class EWorkTool : uint8 { Fork, Repair UMETA(DisplayName="Hammer"), Cart };

USTRUCT(BlueprintType)
struct FCorpseCargo
{
    GENERATED_BODY()
    UPROPERTY() TSoftObjectPtr<class USkeletalMesh> Mesh;
    UPROPERTY() TArray<FName> Bones;
    UPROPERTY() TArray<FTransform> Pose;
    UPROPERTY() int32 Wood = 2;
    UPROPERTY() int32 Iron = 1;
    UPROPERTY() float Fuel = 20.f;
};

UCLASS(BlueprintType)
class DEADEASYINC_API UScavengingSettings : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere) float Reach = 350.f;
    UPROPERTY(EditAnywhere) float HoldDistance = 180.f;
    UPROPERTY(EditAnywhere) FName GrabBone = TEXT("pelvis");
    UPROPERTY(EditAnywhere) int32 BackpackCapacity = 3;
    UPROPERTY(EditAnywhere) int32 ExpandedCapacity = 6;
    UPROPERTY(EditAnywhere,meta=(DisplayName="Legacy Cart Capacity (unused)")) int32 CartCapacity = 12;
    UPROPERTY(EditAnywhere) float CartSpeed = 350.f;
    UPROPERTY(EditAnywhere,meta=(ClampMin="1")) float CartAcceleration=800.f;
    UPROPERTY(EditAnywhere,meta=(ClampMin="1")) float CartDeceleration=1200.f;
    UPROPERTY(EditAnywhere,meta=(ClampMin="1")) float CartTurnAcceleration=240.f;
    UPROPERTY(EditAnywhere,meta=(ClampMin="0.05")) float CargoScanInterval=.2f;
    UPROPERTY(EditAnywhere,meta=(ClampMin="0")) float CargoExitMargin=20.f;
    UPROPERTY(EditAnywhere) int32 CorpseWood = 2;
    UPROPERTY(EditAnywhere) int32 CorpseIron = 1;
    UPROPERTY(EditAnywhere) float CorpseFuel = 20.f;
    UPROPERTY(EditAnywhere) TSubclassOf<class AWorkEquipment> EquipmentClass;
    UPROPERTY(EditAnywhere) TSubclassOf<class AHandcart> HandcartClass;
    UPROPERTY(EditAnywhere) float CartPushDistance=200.f;
    UPROPERTY(EditAnywhere) float CartLoadReach=350.f;
    UPROPERTY(EditAnywhere) float CartTurnSpeed=90.f;
    UPROPERTY(EditAnywhere) float CartDumpReach=300.f;
    UPROPERTY(EditAnywhere,meta=(ClampMin="1")) float CartWheelRadius=51.f;
    UPROPERTY(EditAnywhere,meta=(ClampMin="1")) float CartInteractionReach=350.f;
    UPROPERTY(EditAnywhere,meta=(ClampMin="0.1")) float TailgateSeconds=.7f;
    UPROPERTY(EditAnywhere,meta=(ClampMin="0",ClampMax="140")) float TailgateOpenAngle=110.f;
    UPROPERTY(EditAnywhere,meta=(ClampMin="0",ClampMax="35")) float CartTipAngle=25.f;
    UPROPERTY(EditAnywhere,meta=(ClampMin="0.5")) float CartSpillSeconds=3.f;
    UPROPERTY(EditAnywhere,meta=(ClampMin="0")) float CartSpillAcceleration=150.f;
    UPROPERTY(EditAnywhere) TSubclassOf<class ABorderZombie> CorpseClass;
    UPROPERTY(EditAnywhere,meta=(ClampMin="0.5")) float ProcessingSeconds=1.f;
};
