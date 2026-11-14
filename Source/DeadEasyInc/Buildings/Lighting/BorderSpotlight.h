#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BorderSpotlight.generated.h"

/** Wall-mounted light controlled by the existing day/night director. */
UCLASS()
class DEADEASYINC_API ABorderSpotlight : public AActor
{
    GENERATED_BODY()
public:
    ABorderSpotlight();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Lighting")
    TObjectPtr<class UStaticMeshComponent> Fixture;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Lighting")
    TObjectPtr<class USpotLightComponent> Spotlight;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    TWeakObjectPtr<class ADayNightDirector> Director;
    TWeakObjectPtr<class ABorderGameMode> Mode;
    UFUNCTION() void HandlePhase(bool IsNight, int32 Day);
};
