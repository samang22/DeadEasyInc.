#include "Scavenging/ElectricCart.h"
#include "Scavenging/TrackedCartMovementComponent.h"
#include "Scavenging/HandcartDischargeComponent.h"
#include "Progression/PlayerLoadoutComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

AElectricCart::AElectricCart()
{
    TrackedMovement = CreateDefaultSubobject<UTrackedCartMovementComponent>(TEXT("TrackedMovement"));
    TrackedBase = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrackedBase"));
    TrackedBase->SetupAttachment(Collision);
    TrackedBase->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TrackedBase->SetCanEverAffectNavigation(false);

    DriverSeat = CreateDefaultSubobject<USceneComponent>(TEXT("DriverSeat"));
    DriverSeat->SetupAttachment(Collision);
    // Approximate mounting position, editable in child Blueprint viewport.
    DriverSeat->SetRelativeLocation(FVector(-110.f, 0.f, 0.f));

    // Make the large tracked base the blocking/root footprint. Tune in Blueprint.
    Collision->InitBoxExtent(FVector(170.f, 95.f, 55.f));
    TrackedBase->SetRelativeLocation(FVector(0.f, 0.f, -55.f));
    // CargoBin GLB pivot is at its front hinge: (107, 0, 140) above ground.
    BedPivot->SetRelativeLocation(FVector(107.f, 0.f, 85.f));
    GrabTarget->SetRelativeLocation(FVector(-175.f, 0.f, 60.f));
    HandleMesh->SetVisibility(false);
    for (const TObjectPtr<UStaticMeshComponent>& Wheel : Wheels)
        if (Wheel) Wheel->SetVisibility(false);
}

void AElectricCart::BeginPlay()
{
    Super::BeginPlay();
    TrackedMovement->Configure(Collision);
}

bool AElectricCart::StartPush(APawn* Player)
{
    ACharacter* Character = Cast<ACharacter>(Player);
    if (!IsValid(Character) || Driver || !DriverSeat ||
        FVector::DistSquared2D(Character->GetActorLocation(), GetActorLocation()) > FMath::Square(450.f))
        return false;

    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    if (!Movement || !Capsule) return false;

    PreviousMovementMode = Movement->MovementMode;
    PreviousCapsuleCollision = Capsule->GetCollisionEnabled();
    Movement->StopMovementImmediately();
    Movement->DisableMovement();
    Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Character->AttachToComponent(DriverSeat, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    MountedCharacter = Character;
    Driver = Character;  // Existing CorpseHandlingComponent recognizes IsPushing().
    TrackedMovement->ResetMotion();
    SetActorTickEnabled(true);
    return true;
}

bool AElectricCart::FindExitPosition(const ACharacter* Character, FVector& OutLocation) const
{
    if (!Character || !GetWorld() || !Collision) return false;
    const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    if (!Capsule) return false;

    const float Clearance = Collision->GetScaledBoxExtent().Y + Capsule->GetScaledCapsuleRadius() + 45.f;
    const FVector SideOffsets[] = {
        GetActorRightVector() * Clearance,
        -GetActorRightVector() * Clearance,
        -GetActorForwardVector() * (Collision->GetScaledBoxExtent().X + Capsule->GetScaledCapsuleRadius() + 50.f)
    };
    for (const FVector& Offset : SideOffsets)
    {
        const FVector Start = GetActorLocation() + Offset + FVector(0.f, 0.f, 220.f);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(ElectricCartExit), false, Character);
        Params.AddIgnoredActor(this); // Ignore the cart for the ground trace only.
        FHitResult Ground;
        if (!GetWorld()->LineTraceSingleByObjectType(Ground, Start,
            Start - FVector(0.f, 0.f, 650.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
            continue;
        if (Ground.ImpactNormal.Z < 0.65f) continue;

        const FVector Candidate = Ground.ImpactPoint + FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + 3.f);
        FCollisionQueryParams ClearanceParams(SCENE_QUERY_STAT(ElectricCartExitClear), false, Character);
        const bool bBlocked = GetWorld()->OverlapBlockingTestByProfile(
            Candidate, FQuat::Identity, Capsule->GetCollisionProfileName(),
            FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), ClearanceParams);
        if (!bBlocked) { OutLocation = Candidate; return true; }
    }
    return false;
}

void AElectricCart::StopPush()
{
    ACharacter* Character = MountedCharacter.Get();
    if (!Character) Character = Cast<ACharacter>(Driver);
    TrackedMovement->ResetMotion();

    if (IsValid(Character))
    {
        FVector ExitLocation;
        const bool bFoundExit = FindExitPosition(Character, ExitLocation);
        Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        if (bFoundExit)
        {
            Character->SetActorLocation(ExitLocation, false, nullptr, ETeleportType::TeleportPhysics);
        }
        else
        {
            // Emergency fallback for death / teardown. Normal exit should have valid ground.
            Character->SetActorLocation(GetActorLocation() - GetActorForwardVector() * 300.f + FVector(0,0,170.f),
                false, nullptr, ETeleportType::TeleportPhysics);
        }
        Character->GetCapsuleComponent()->SetCollisionEnabled(PreviousCapsuleCollision);
        Character->GetCharacterMovement()->SetMovementMode(
            PreviousMovementMode == MOVE_None ? MOVE_Walking : PreviousMovementMode);
        if (!Character->IsActorBeingDestroyed() && GetWorld() && !GetWorld()->bIsTearingDown)
            if (UPlayerLoadoutComponent* Loadout = Character->FindComponentByClass<UPlayerLoadoutComponent>())
                Loadout->RefreshWeapon();
    }

    MountedCharacter.Reset();
    Driver = nullptr;
    if (Discharge) Discharge->RequestClose();
    SetActorTickEnabled(Discharge && Discharge->IsActive());
}

bool AElectricCart::Dump()
{
    if (!Driver) return false;
    if (!Discharge->IsActive() && FMath::Abs(TrackedMovement->GetSpeed()) > 10.f)
        return false; // Must stop before tipping the loaded bin.
    TrackedMovement->ResetMotion();
    return Super::Dump();
}

bool AElectricCart::SnapToGround()
{
    if (!TrackedMovement) return false;
    TrackedMovement->Configure(Collision);
    return TrackedMovement->SnapToGround();
}

void AElectricCart::AdvanceDriverMovement(float Dt)
{
    TrackedMovement->Advance(Dt, Driver);
}

void AElectricCart::SetDriveInput(float Forward, float Turn)
{
    if (Driver && Discharge && !Discharge->IsActive())
        TrackedMovement->SetInput(Forward, Turn);
    else
        TrackedMovement->StopInput();
}
