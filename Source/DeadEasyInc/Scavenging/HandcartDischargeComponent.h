#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HandcartDischargeComponent.generated.h"

/** Owns the tailgate/bed discharge sequence and its obstruction check. */
UCLASS(ClassGroup=(Scavenging))
class DEADEASYINC_API UHandcartDischargeComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHandcartDischargeComponent();
    void Configure(class UScavengingSettings* InSettings,class USceneComponent* InBedPivot,class USceneComponent* InTailgatePivot,class UBoxComponent* InClosingArea);
    void Start();
    void RequestClose();
    void Reset();
    void Advance(float Dt);
    bool IsActive() const { return bDischarging; }
private:
    bool CanCloseTailgate() const;
    UPROPERTY(Transient) TObjectPtr<class UScavengingSettings> Settings;
    UPROPERTY(Transient) TObjectPtr<class USceneComponent> BedPivot;
    UPROPERTY(Transient) TObjectPtr<class USceneComponent> TailgatePivot;
    UPROPERTY(Transient) TObjectPtr<class UBoxComponent> TailgateClosingArea;
    bool bDischarging=false;
    bool bClosingTailgate=false;
    float TailgateAlpha=0.f;
    float BedTipAlpha=0.f;
    float SpillTime=0.f;
    FQuat ClosedBedRotation=FQuat::Identity;
    FQuat ClosedGateRotation=FQuat::Identity;
};
