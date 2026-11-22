#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EquippedWeapon.generated.h"

class ADeadEasyIncCharacter;
class UStaticMeshComponent;
class USoundBase;
class UWeaponDefinition;
class UAnimSequence;
class UAnimMontage;
class UAnimInstance;

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
    // Camera fallback pose, including the Blueprint-authored weapon scale.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FTransform ViewTransform = FTransform(FRotator::ZeroRotator, FVector(45,18,-18));
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grip") FVector HandGripLocation = FVector::ZeroVector;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grip") FRotator HandGripRotation = FRotator::ZeroRotator;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient) TObjectPtr<UWeaponDefinition> WeaponDefinition;
    void AttachToPlayer(ADeadEasyIncCharacter& Character);
    void ConfigureAudio(UWeaponDefinition* Definition);
    void PlayEquip();
    void PlayUnequip();
    void PlayDryFire();
    void CancelReloadAudio();
    bool Attack(const UWeaponDefinition* Definition);
    bool PlayMeleeSwing(UAnimSequence* Sequence, float AttackInterval);
    void StopMeleeSwing();
    void PlayReload();
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TObjectPtr<class UWeaponAudioComponent> WeaponAudio;
    UAnimMontage* PlayUpperBodyAnimation(UAnimSequence* Sequence, FName Slot, float BlendIn, float BlendOut, float Rate);
    UPROPERTY(Transient) TObjectPtr<UAnimMontage> HoldMontage;
    UPROPERTY(Transient) TObjectPtr<UAnimMontage> MeleeMontage;
    TWeakObjectPtr<UAnimInstance> BodyAnimInstance;
};
