#include "Scavenging/ElectricCart.h"
#include "Scavenging/EquipmentMountComponent.h"
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
    MountState = CreateDefaultSubobject<UEquipmentMountComponent>(TEXT("MountState"));
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
    if (!CanInteract(Player) || !MountState->Mount(Cast<ACharacter>(Player), DriverSeat)) return false;
    Driver=Player;
    TrackedMovement->ResetMotion();
    SetActorTickEnabled(true);
    return true;
}

void AElectricCart::StopPush()
{
    EndUse(Driver, true); // Existing automatic death/phase/teardown callers require cleanup.
}

bool AElectricCart::EndUse(APawn* User, bool bForce)
{
    if (Driver && Driver!=User) return false;
    APawn* Previous=Driver;
    if (!MountState->Dismount(bForce)) return false;
    TrackedMovement->ResetMotion();
    Driver=nullptr;
    if (Discharge) Discharge->RequestClose();
    SetActorTickEnabled(Discharge && Discharge->IsActive());
    if (IsValid(Previous) && !Previous->IsActorBeingDestroyed() && GetWorld() && !GetWorld()->bIsTearingDown)
        if (auto* Loadout=Previous->FindComponentByClass<UPlayerLoadoutComponent>()) Loadout->RefreshWeapon();
    return true;
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
