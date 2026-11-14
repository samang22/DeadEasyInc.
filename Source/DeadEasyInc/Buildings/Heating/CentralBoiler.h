#pragma once

#include "CoreMinimal.h"
#include "Buildings/Building.h"
#include "CentralBoiler.generated.h"

class UCombatHealthComponent;

/** The base's final defense objective. Visual assets belong to its Blueprint. */
UCLASS()
class DEADEASYINC_API ACentralBoiler : public ABuilding
{
    GENERATED_BODY()
public:
    ACentralBoiler();
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float Fuel=100.f;
    float Capacity() const;
    float Efficiency() const;
    float AddFuel(float Amount);
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boiler")
    bool bDestroyed = false;
    UFUNCTION(BlueprintImplementableEvent, Category="Boiler")
    void OnBoilerDestroyed();
protected:
    virtual void BeginPlay() override;
private:
    UFUNCTION()
    void HandleDestroyed();
};
