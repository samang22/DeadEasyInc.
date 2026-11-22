#include "Player/DeadEasyIncCharacter.h"
#include "Player/DeadEasyIncPlayerController.h"
// Copyright Epic Games, Inc. All Rights Reserved.
#include "Progression/PlayerLoadoutComponent.h"
#include "Scavenging/CorpseHandlingComponent.h"
#include "Scavenging/UsableEquipment.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DeadEasyInc.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Player/Data/PlayerSettings.h"
#include "Interaction/BuildingRepairComponent.h"
#include "NiagaraComponent.h"
#include "Zombies/BorderZombie.h"

ADeadEasyIncCharacter::ADeadEasyIncCharacter()
{
    Loadout = CreateDefaultSubobject<UPlayerLoadoutComponent>(TEXT("Loadout"));
    CorpseHandling = CreateDefaultSubobject<UCorpseHandlingComponent>(TEXT("CorpseHandling"));
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

    WeaponMount = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponMount"));
    WeaponMount->SetupAttachment(FirstPersonMesh, TEXT("hand_r"));

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
	
	
	SnowComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SnowComponent"));

	SnowComponent->SetupAttachment(RootComponent);
	SnowComponent->SetAutoActivate(true);
	
}

#if WITH_EDITOR
void ADeadEasyIncCharacter::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (!GetWorld() || GetWorld()->WorldType != EWorldType::EditorPreview) return;

    TArray<USkeletalMesh*> PreviewMeshes;
    for (USkeletalMesh* PartMesh : FirstPersonAppearance)
        if (PartMesh) PreviewMeshes.Add(PartMesh);

    const bool bHasFirstPersonParts = !PreviewMeshes.IsEmpty();
    if (const ULoadoutSettings* LoadoutSettings = Loadout->Settings)
    {
        const EArmorSlot Slots[] = { EArmorSlot::Head, EArmorSlot::Legs, EArmorSlot::Feet, EArmorSlot::Chest, EArmorSlot::Hands };
        for (EArmorSlot Slot : Slots)
        {
            if (bHasFirstPersonParts && (Slot == EArmorSlot::Chest || Slot == EArmorSlot::Hands)) continue;
            if (USkeletalMesh* PartMesh = LoadoutSettings->ArmorBaseMeshes.FindRef(Slot).Get())
                PreviewMeshes.Add(PartMesh);
        }
    }

    const auto* Defaults = GetClass()->GetDefaultObject<ADeadEasyIncCharacter>();
    const bool bHasPreview = !PreviewMeshes.IsEmpty();
    GetMesh()->SetVisibility(!bHasPreview && Defaults->GetMesh()->GetVisibleFlag());
    FirstPersonMesh->SetVisibility(!bHasPreview && Defaults->FirstPersonMesh->GetVisibleFlag());
    for (USkeletalMesh* PartMesh : PreviewMeshes)
        AddAppearancePart(*FirstPersonMesh, PartMesh, true);
}
#endif

void ADeadEasyIncCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (FirstPersonAppearance.ContainsByPredicate([](const USkeletalMesh* PartMesh) { return PartMesh != nullptr; }))
    {
        FirstPersonMesh->SetVisibility(false);
        FirstPersonMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        for (USkeletalMesh* PartMesh : FirstPersonAppearance)
            if (PartMesh) AddAppearancePart(*FirstPersonMesh, PartMesh);
    }
    if (Settings)
    {
        if (UCombatHealthComponent* HealthComponent = FindComponentByClass<UCombatHealthComponent>())
        {
            HealthComponent->MaxHealth = FMath::Max(1.f, Settings->MaxHealth);
            HealthComponent->Health = HealthComponent->MaxHealth;
        }
    }
}

float ADeadEasyIncCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    if (Loadout && Cast<ABorderZombie>(DamageCauser))
        DamageAmount *= 1.f - Loadout->GetZombieDamageReduction();
    return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

USkeletalMeshComponent* ADeadEasyIncCharacter::AddAppearancePart(USkeletalMeshComponent& Leader, USkeletalMesh* PartMesh, bool bEditorPreview)
{
    auto* Part = NewObject<USkeletalMeshComponent>(this, NAME_None, RF_Transient);
    Part->SetupAttachment(&Leader);
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Part->SetGenerateOverlapEvents(false);
    if (bEditorPreview)
    {
        Part->CreationMethod = EComponentCreationMethod::UserConstructionScript;
    }
    else
    {
        Part->SetOnlyOwnerSee(Leader.bOnlyOwnerSee);
        Part->SetOwnerNoSee(Leader.bOwnerNoSee);
        Part->FirstPersonPrimitiveType = Leader.FirstPersonPrimitiveType;
    }
    Part->bUseBoundsFromLeaderPoseComponent = true;
    Part->SetSkeletalMeshAsset(PartMesh);
    Part->RegisterComponent();
    Part->SetLeaderPoseComponent(&Leader, true);
    return Part;
}

void ADeadEasyIncCharacter::SetArmorAppearance(const TArray<USkeletalMesh*>& Meshes)
{
    USkeletalMeshComponent* Leader = GetMesh();
    if (ArmorMeshes.IsEmpty() && !Meshes.IsEmpty())
    {
        bSavedMeshVisibility = Leader->GetVisibleFlag();
        SavedMeshTickOption = Leader->VisibilityBasedAnimTickOption;
        Leader->SetVisibility(false);
        Leader->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    }
    if (!ArmorMeshes.IsEmpty() && Meshes.IsEmpty())
    {
        Leader->SetVisibility(bSavedMeshVisibility);
        Leader->VisibilityBasedAnimTickOption = SavedMeshTickOption;
    }
    while (ArmorMeshes.Num() > Meshes.Num())
    {
        USkeletalMeshComponent* Part = ArmorMeshes.Pop();
        Part->DestroyComponent();
    }
    for (int32 Index = 0; Index < Meshes.Num(); ++Index)
    {
        if (!ArmorMeshes.IsValidIndex(Index))
        {
            ArmorMeshes.Add(AddAppearancePart(*Leader, Meshes[Index]));
        }
        else if (ArmorMeshes[Index]->GetSkeletalMeshAsset() != Meshes[Index])
        {
            ArmorMeshes[Index]->SetSkeletalMeshAsset(Meshes[Index]);
            ArmorMeshes[Index]->SetLeaderPoseComponent(Leader, true);
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
        EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ADeadEasyIncCharacter::StopVehicleMovementInput);
        EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ADeadEasyIncCharacter::StopVehicleMovementInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADeadEasyIncCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ADeadEasyIncCharacter::MouseLookInput);
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

void ADeadEasyIncCharacter::MouseLookInput(const FInputActionValue& Value)
{
    const auto* Player=Cast<ADeadEasyIncPlayerController>(GetController());
    const FVector2D Axis=Value.Get<FVector2D>()*(Player ? Player->MouseSensitivity : 1.f);
    DoAim(Axis.X,Axis.Y);
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
    if (auto* Use=CorpseHandling ? CorpseHandling->GetUsedEquipment() : nullptr)
        if (Use->IsDirectlyDriven())
        {
            Use->SetDriveInput(Forward,Right);
            return;
        }
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void ADeadEasyIncCharacter::StopVehicleMovementInput()
{
    if (auto* Use=CorpseHandling ? CorpseHandling->GetUsedEquipment() : nullptr)
        if (Use->IsDirectlyDriven()) Use->SetDriveInput(0.f,0.f);
}

void ADeadEasyIncCharacter::DoJumpStart()
{
    if (auto* Use=CorpseHandling ? CorpseHandling->GetUsedEquipment() : nullptr)
        if (Use->IsDirectlyDriven()) return;
    if (GetController() && GetController()->IsMoveInputIgnored()) return;
	// pass Jump to the character
	Jump();
}

void ADeadEasyIncCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}
