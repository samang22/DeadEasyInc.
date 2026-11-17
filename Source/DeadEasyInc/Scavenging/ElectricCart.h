#pragma once

#include "CoreMinimal.h"
#include "Scavenging/Handcart.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ElectricCart.generated.h"

class UTrackedCartMovementComponent;
class UStaticMeshComponent;
class USceneComponent;
class ACharacter;

/** AHandcart-compatible electric cart: reuses cargo and dump logic, replaces follow with tank drive. */
UCLASS()
class DEADEASYINC_API AElectricCart : public AHandcart
{
    GENERATED_BODY()
public:
    AElectricCart();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electric Cart")
    TObjectPtr<UStaticMeshComponent> TrackedBase;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electric Cart")
    TObjectPtr<USceneComponent> DriverSeat;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Electric Cart")
    TObjectPtr<UTrackedCartMovementComponent> TrackedMovement;

    virtual bool StartPush(APawn* Player) override;
    virtual void StopPush() override;
    virtual bool Dump() override;
    virtual bool SnapToGround() override;
    virtual void AdvanceDriverMovement(float Dt) override;
    virtual void UpdateWheelVisuals() override {}

    virtual bool IsDirectlyDriven() const override { return true; }
    virtual bool EndUse(APawn* User, bool bForce = false) override;
    virtual void SetDriveInput(float Forward, float Turn) override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Electric Cart")
    TObjectPtr<class UEquipmentMountComponent> MountState;
    bool IsMountedBy(const AActor* Player) const { return Driver && Driver == Player; }

protected:
    virtual void BeginPlay() override;

};
