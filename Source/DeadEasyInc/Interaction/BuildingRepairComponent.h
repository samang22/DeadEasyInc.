#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BuildingRepairComponent.generated.h"

class ABuilding;

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class DEADEASYINC_API UBuildingRepairComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings")
    TObjectPtr<class URepairSettings> Settings;
    UBuildingRepairComponent();
    // Temporary local supply until the shared resource inventory is implemented.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Repair")
    int32 Wood = 50;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Repair")
    float RepairPerStroke = 50.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Repair")
    float StrokeInterval = 0.5f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Repair")
    float Reach = 300.f;
    UFUNCTION(BlueprintCallable, Category="Repair")
    void StartRepair();
    UFUNCTION(BlueprintCallable, Category="Repair")
    void StopRepair();
    UFUNCTION(BlueprintPure, Category="Repair")
    ABuilding* GetFocusedBuilding() const;
    UFUNCTION(BlueprintPure, Category="Repair")
    int32 GetWoodPerStroke() const;
    UFUNCTION(BlueprintImplementableEvent, Category="Repair")
    void OnRepairStroke(ABuilding* Wall, float RestoredHealth);
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
    virtual void BeginPlay() override;
    bool bRepairHeld = false;
    float NextStrokeTime = 0.f;
};
