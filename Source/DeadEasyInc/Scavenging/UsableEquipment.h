#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UsableEquipment.generated.h"

class APawn;
class UBoxComponent;

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UUsableEquipment : public UInterface
{
    GENERATED_BODY()
};

/** Native contract; existing Blueprint equipment inherits its C++ implementation. */
class DEADEASYINC_API IUsableEquipment
{
    GENERATED_BODY()
public:
    virtual bool CanInteract(APawn* User) const = 0;
    virtual bool BeginUse(APawn* User) = 0;
    // Normal exits can fail if blocked; forced exits are reserved for teardown/death.
    virtual bool EndUse(APawn* User, bool bForce = false) = 0;
    virtual bool IsUsedBy(const APawn* User) const = 0;
    virtual bool IsDirectlyDriven() const = 0;
    virtual void SetDriveInput(float Forward, float Right) = 0;
    virtual bool ExecutePrimaryAction() = 0;
    virtual void ExecuteSecondaryAction() {}
    virtual FString GetUseHint() const { return FString(); }
    virtual UBoxComponent* GetInteractionArea() const = 0;
};
