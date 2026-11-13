#pragma once
#include "CoreMinimal.h"
#include "Buildings/Defense/BorderWall.h"
#include "BorderGate.generated.h"

UCLASS()
class DEADEASYINC_API ABorderGate : public ABorderWall
{
    GENERATED_BODY()
public:
    ABorderGate();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gate") float OpenHeight = 600.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gate") float OpenFraction = 1.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gate") TObjectPtr<class UBoxComponent> PassageBlocker;
    UFUNCTION(BlueprintCallable) void SetOpenFraction(float Fraction);
    bool IsPassageClear() const;
    void SetTransitionBarrier(bool bEnabled);
protected:
    virtual void BeginPlay() override;
    virtual void ApplyConditionState() override;
private:
    FVector ClosedLocation;
    FVector ClosedRubbleLocation;
    FVector ClosedRepairLocation;
    bool bGateInitialized = false;
};
