#pragma once
#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "Building.generated.h"

class UBuildingSettings;
class UCombatHealthComponent;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EBuildingCondition : uint8 { Healthy, Damaged, Critical, Destroyed };

UCLASS()
class DEADEASYINC_API ABuilding : public AStaticMeshActor
{
    GENERATED_BODY()
public:
    ABuilding();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building")
    TObjectPtr<UBuildingSettings> Settings;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Building")
    TObjectPtr<UCombatHealthComponent> CombatHealth;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Appearance")
    TObjectPtr<UMaterialInterface> HealthyMaterial;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Appearance")
    TObjectPtr<UMaterialInterface> DamagedMaterial;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Appearance")
    TObjectPtr<UMaterialInterface> CriticalMaterial;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Building")
    EBuildingCondition Condition = EBuildingCondition::Healthy;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building|Repair")
    bool bRepairAllowed = true;
    UFUNCTION(BlueprintCallable, Category="Building") float Repair(float Amount);
    UFUNCTION(BlueprintPure, Category="Building") bool CanRepair() const;
    UFUNCTION(BlueprintPure, Category="Building") FText GetBuildingName() const;
    UFUNCTION(BlueprintImplementableEvent, Category="Building") void OnConditionChanged(EBuildingCondition NewCondition);
    UFUNCTION(BlueprintImplementableEvent, Category="Building") void OnBuildingDestroyed();
protected:
    virtual void BeginPlay() override;
    // Also runs when a dawn checkpoint restores health, not only on destruction.
    virtual void ApplyConditionState();
private:
    UFUNCTION() void UpdateCondition(float Health, float MaxHealth);
    UFUNCTION() void HandleBuildingDestroyed();
};
