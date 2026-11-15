// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DeadEasyIncPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;

/**
 *  Simple first person Player Controller
 *  Manages the input mapping context.
 *  Overrides the Player Camera Manager class.
 */
UCLASS(abstract, config="Game")
class DEADEASYINC_API ADeadEasyIncPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	ADeadEasyIncPlayerController();
    UPROPERTY(Transient,BlueprintReadOnly,Category="Pause") bool bPauseMenuOpen=false;
    UPROPERTY(Config,BlueprintReadOnly,Category="Settings") float MasterVolume=1.f;
    UPROPERTY(Config,BlueprintReadOnly,Category="Settings") float MouseSensitivity=1.f;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;
    UFUNCTION(BlueprintCallable,Category="Pause") void TogglePauseMenu();
    void ResumeGame();
    void AdjustVolume(float Delta);
    void AdjustSensitivity(float Delta);
    void QuitGame();
private:
    void ApplyVolume();


protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;
};
