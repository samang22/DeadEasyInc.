#include "Scavenging/EquipmentMountComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

UEquipmentMountComponent::UEquipmentMountComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UEquipmentMountComponent::Mount(ACharacter* Character, USceneComponent* Position)
{
    if (!IsValid(Character) || Driver.IsValid() || !Position ||
        Character->GetAttachParentActor() || Character->ActorHasTag(TEXT("Dead"))) return false;
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    if (!Movement || !Capsule) return false;
    EntryOffsetLocal = GetOwner()->GetActorTransform().InverseTransformPosition(Character->GetActorLocation());
    EntryOffsetLocal.Z = 0.f;
    PreviousMode = Movement->MovementMode;
    PreviousCustomMode = Movement->CustomMovementMode;
    PreviousCollision = Capsule->GetCollisionEnabled();
    Movement->StopMovementImmediately();
    Movement->DisableMovement();
    Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (!Character->AttachToComponent(Position, FAttachmentTransformRules::SnapToTargetNotIncludingScale))
    {
        Capsule->SetCollisionEnabled(PreviousCollision);
        Movement->SetMovementMode(PreviousMode, PreviousCustomMode);
        return false;
    }
    Driver = Character;
    return true;
}

bool UEquipmentMountComponent::FindExit(FVector& OutLocation) const
{
    ACharacter* Character = Driver.Get();
    if (!Character || !GetWorld()) return false;
    const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    const AActor* Vehicle = GetOwner();
    const auto* Root = Cast<UPrimitiveComponent>(Vehicle->GetRootComponent());
    const FVector Extent = Root ? Root->Bounds.BoxExtent : FVector(200.f, 100.f, 60.f);
    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const float Clearance = FMath::Max(Extent.X, Extent.Y) + Radius + 45.f;
    const FVector Side = Vehicle->GetActorRightVector() * (EntryOffsetLocal.Y >= 0.f ? Clearance : -Clearance);
    const FVector Offsets[] = {
        Vehicle->GetActorTransform().TransformVector(EntryOffsetLocal),
        Side, -Side, -Vehicle->GetActorForwardVector() * Clearance,
        Vehicle->GetActorForwardVector() * Clearance
    };
    FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(EquipmentExitGround), false, Vehicle);
    GroundParams.AddIgnoredActor(Character);
    FCollisionQueryParams ClearParams(SCENE_QUERY_STAT(EquipmentExitClear), false, Character);
    // Vehicle must participate in clearance: a clear ground ray alone is insufficient.
    for (const FVector& Offset : Offsets)
    {
        const FVector Start = Vehicle->GetActorLocation() + Offset + FVector(0.f, 0.f, 300.f);
        FHitResult Ground;
        if (!GetWorld()->LineTraceSingleByObjectType(Ground, Start, Start-FVector(0.f,0.f,950.f),
            FCollisionObjectQueryParams(ECC_WorldStatic), GroundParams) || Ground.ImpactNormal.Z < .65f) continue;
        const FVector Candidate = Ground.ImpactPoint + FVector(0.f, 0.f, HalfHeight + 3.f);
        if (!GetWorld()->OverlapBlockingTestByProfile(Candidate, FQuat::Identity, Capsule->GetCollisionProfileName(),
            FCollisionShape::MakeCapsule(Radius, HalfHeight), ClearParams))
        { OutLocation = Candidate; return true; }
    }
    return false;
}

bool UEquipmentMountComponent::Dismount(bool bForce)
{
    ACharacter* Character = Driver.Get();
    if (!IsValid(Character)) { Driver.Reset(); return true; }
    FVector Exit;
    const bool bFound = FindExit(Exit);
    if (!bFound && !bForce) return false;
    Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    if (bFound) Character->SetActorLocation(Exit, false, nullptr, ETeleportType::TeleportPhysics);
    // During teardown preserve the current location if every exit is obstructed.
    Character->GetCapsuleComponent()->SetCollisionEnabled(PreviousCollision);
    Character->GetCharacterMovement()->StopMovementImmediately();
    Character->GetCharacterMovement()->SetMovementMode(PreviousMode, PreviousCustomMode);
    Driver.Reset();
    return true;
}

void UEquipmentMountComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    Dismount(true);
    Super::EndPlay(Reason);
}
