#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EquipmentMountComponent.generated.h"

class ACharacter;
class USceneComponent;

/** Owns attachment and character restoration only. Vehicle movement stays on equipment. */
UCLASS(ClassGroup=(Scavenging))
class DEADEASYINC_API UEquipmentMountComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UEquipmentMountComponent();
    bool Mount(ACharacter* Character, USceneComponent* Position);
    bool Dismount(bool bForce = false);
    ACharacter* GetDriver() const { return Driver.Get(); }
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool FindExit(FVector& OutLocation) const;
    UPROPERTY(Transient) TWeakObjectPtr<ACharacter> Driver;
    FVector EntryOffsetLocal = FVector::ZeroVector;
    EMovementMode PreviousMode = MOVE_Walking;
    uint8 PreviousCustomMode = 0;
    ECollisionEnabled::Type PreviousCollision = ECollisionEnabled::QueryAndPhysics;
};
