#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EquippedWeapon.generated.h"

class UStaticMeshComponent;
class USoundBase;
class UWeaponDefinition;

UCLASS()
class DEADEASYINC_API AEquippedWeapon : public AActor
{
    GENERATED_BODY()
public:
    AEquippedWeapon();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> WeaponMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> AttachmentMesh;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<USoundBase> AttackSound;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<USoundBase> ReloadSound;
    // Asset-specific pose is set on each weapon Blueprint.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FTransform ViewTransform = FTransform(FRotator::ZeroRotator, FVector(45,18,-18));
    bool Attack(const UWeaponDefinition* Definition);
    void PlayReload();
    virtual void Tick(float DeltaSeconds) override;
};
