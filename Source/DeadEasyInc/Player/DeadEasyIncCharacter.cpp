// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/DeadEasyIncCharacter.h"
#include "Progression/PlayerLoadoutComponent.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DeadEasyInc.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Player/Data/PlayerSettings.h"
#include "Interaction/BuildingRepairComponent.h"

ADeadEasyIncCharacter::ADeadEasyIncCharacter()
{
    Loadout = CreateDefaultSubobject<UPlayerLoadoutComponent>(TEXT("Loadout"));
    CreateDefaultSubobject<UCombatHealthComponent>(TEXT("CombatHealth"));
    WallRepair = CreateDefaultSubobject<UBuildingRepairComponent>(TEXT("WallRepair"));
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
}

void ADeadEasyIncCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (Settings)
    {
        if (UCombatHealthComponent* HealthComponent = FindComponentByClass<UCombatHealthComponent>())
        {
            HealthComponent->MaxHealth = FMath::Max(1.f, Settings->MaxHealth);
            HealthComponent->Health = HealthComponent->MaxHealth;
        }
    }
}

void ADeadEasyIncCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
        Loadout->BindInput(EnhancedInputComponent);
		// Jumping
        if (RepairAction)
        {
            EnhancedInputComponent->BindAction(RepairAction, ETriggerEvent::Started, WallRepair.Get(), &UBuildingRepairComponent::StartRepair);
            EnhancedInputComponent->BindAction(RepairAction, ETriggerEvent::Completed, WallRepair.Get(), &UBuildingRepairComponent::StopRepair);
            EnhancedInputComponent->BindAction(RepairAction, ETriggerEvent::Canceled, WallRepair.Get(), &UBuildingRepairComponent::StopRepair);
        }
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ADeadEasyIncCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ADeadEasyIncCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADeadEasyIncCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADeadEasyIncCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ADeadEasyIncCharacter::LookInput);
	}
	else
	{
		UE_LOG(LogDeadEasyInc, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void ADeadEasyIncCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void ADeadEasyIncCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void ADeadEasyIncCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ADeadEasyIncCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void ADeadEasyIncCharacter::DoJumpStart()
{
    if (GetController() && GetController()->IsMoveInputIgnored()) return;
	// pass Jump to the character
	Jump();
}

void ADeadEasyIncCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}
