// Copyright Epic Games, Inc. All Rights Reserved.


#include "Player/DeadEasyIncPlayerController.h"
#include "Progression/PlayerLoadoutComponent.h"
#include "Interaction/BuildingRepairComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AudioDevice.h"
#include "InputKeyEventArgs.h"
#include "Engine/World.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Player/DeadEasyIncCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "DeadEasyInc.h"
#include "Widgets/Input/SVirtualJoystick.h"

ADeadEasyIncPlayerController::ADeadEasyIncPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = ADeadEasyIncCameraManager::StaticClass();
}

void ADeadEasyIncPlayerController::BeginPlay()
{
	Super::BeginPlay();
    MasterVolume=FMath::Clamp(MasterVolume,0.f,1.f);
    MouseSensitivity=FMath::Clamp(MouseSensitivity,.1f,3.f);
    ApplyVolume();

	
	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogDeadEasyInc, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void ADeadEasyIncPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	
}

bool ADeadEasyIncPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

bool ADeadEasyIncPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
    // Handle ESC before the pawn's enhanced input, including while the world is paused.
    if (Params.Key==EKeys::Escape)
    {
        if (Params.Event==IE_Pressed) TogglePauseMenu();
        return true;
    }
    return Super::InputKey(Params);
}

void ADeadEasyIncPlayerController::TogglePauseMenu()
{
    if (bPauseMenuOpen) { ResumeGame();return; }
    APawn* ControlledPawn=GetPawn();
    if (auto* Loadout=ControlledPawn ? ControlledPawn->FindComponentByClass<UPlayerLoadoutComponent>() : nullptr)
    {
        if (Loadout->bMenuOpen) { Loadout->CloseMenu();return; }
        Loadout->StopActions();
    }
    if (auto* Repair=ControlledPawn ? ControlledPawn->FindComponentByClass<UBuildingRepairComponent>() : nullptr) Repair->StopRepair();
    if (!SetPause(true)) return;
    bPauseMenuOpen=true;
    SetIgnoreMoveInput(true);SetIgnoreLookInput(true);
    bShowMouseCursor=true;bEnableClickEvents=true;
    FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);
    SetInputMode(Mode);
}
void ADeadEasyIncPlayerController::ResumeGame()
{
    if (!bPauseMenuOpen) return;
    SetPause(false);bPauseMenuOpen=false;
    SetIgnoreMoveInput(false);SetIgnoreLookInput(false);
    bShowMouseCursor=false;bEnableClickEvents=false;
    SetInputMode(FInputModeGameOnly());
}
void ADeadEasyIncPlayerController::ApplyVolume()
{
    if (FAudioDeviceHandle Device=GetWorld()->GetAudioDevice()) Device->SetTransientPrimaryVolume(MasterVolume);
}
void ADeadEasyIncPlayerController::AdjustVolume(float Delta)
{
    MasterVolume=FMath::Clamp(MasterVolume+Delta,0.f,1.f);ApplyVolume();SaveConfig();
}
void ADeadEasyIncPlayerController::AdjustSensitivity(float Delta)
{
    MouseSensitivity=FMath::Clamp(MouseSensitivity+Delta,.1f,3.f);SaveConfig();
}
void ADeadEasyIncPlayerController::QuitGame()
{
    UKismetSystemLibrary::QuitGame(this,this,EQuitPreference::Quit,false);
}
