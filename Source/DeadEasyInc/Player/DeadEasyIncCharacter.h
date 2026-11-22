// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "DeadEasyIncCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class USceneComponent;
class USkeletalMesh;
enum class EVisibilityBasedAnimTickOption : uint8;
class UCameraComponent;
class UInputAction;
class UBuildingRepairComponent;
struct FInputActionValue;
class UNiagaraComponent;
class UNiagaraSystem;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character
 */
UCLASS(abstract)
class ADeadEasyIncCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    TObjectPtr<USceneComponent> WeaponMount;

protected:

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Repair")
    TObjectPtr<UBuildingRepairComponent> WallRepair;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
    TObjectPtr<UInputAction> RepairAction;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weather")
	TObjectPtr<UNiagaraComponent> SnowComponent;
public:
	ADeadEasyIncCharacter();
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UCorpseHandlingComponent> CorpseHandling;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Loadout")
    TObjectPtr<class UPlayerLoadoutComponent> Loadout;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings")
    TObjectPtr<class UPlayerSettings> Settings;
    virtual void BeginPlay() override;
#if WITH_EDITOR
    virtual void OnConstruction(const FTransform& Transform) override;
#endif
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;
    void SetArmorAppearance(const TArray<USkeletalMesh*>& Meshes);

private:
    UPROPERTY(EditDefaultsOnly, Category="Appearance")
    TArray<TObjectPtr<USkeletalMesh>> FirstPersonAppearance;

    UPROPERTY(Transient) TArray<TObjectPtr<USkeletalMeshComponent>> ArmorMeshes;
    USkeletalMeshComponent* AddAppearancePart(USkeletalMeshComponent& Leader, USkeletalMesh* PartMesh, bool bEditorPreview = false);
    EVisibilityBasedAnimTickOption SavedMeshTickOption{};
    bool bSavedMeshVisibility = true;

protected:

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);
    void StopVehicleMovementInput();

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);
    void MouseLookInput(const FInputActionValue& Value);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

    USceneComponent* GetWeaponMount() const { return WeaponMount; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};

