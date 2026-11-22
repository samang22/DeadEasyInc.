#include "Scavenging/HandcartMovementComponent.h"
#include "Scavenging/ScavengingData.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

UHandcartMovementComponent::UHandcartMovementComponent() { PrimaryComponentTick.bCanEverTick=false; }
void UHandcartMovementComponent::Configure(UScavengingSettings* InSettings, UBoxComponent* InCollision,
                                           USceneComponent* InWheelAxle, USceneComponent* InGroundContact)
{
    if (!Collision) LastWheelLocation=GetOwner()->GetActorLocation();
    Settings=InSettings;Collision=InCollision;WheelAxle=InWheelAxle;
    GroundContact = InGroundContact;
}
void UHandcartMovementComponent::BeginPush(APawn* Player)
{
    SnapToGround(Player, true);
    LastWheelLocation=GetOwner()->GetActorLocation();
    if (Collision) Collision->IgnoreActorWhenMoving(Player, true);
    if (auto* Character=Cast<ACharacter>(Player))
    { NormalSpeed=Character->GetCharacterMovement()->MaxWalkSpeed;Character->GetCharacterMovement()->MaxWalkSpeed=FMath::Min(NormalSpeed,Settings ? Settings->CartSpeed : 350.f); }
}
void UHandcartMovementComponent::EndPush(APawn* Player)
{
    if (auto* Character=Cast<ACharacter>(Player)) Character->GetCharacterMovement()->MaxWalkSpeed=NormalSpeed;
    if (Collision && Player) Collision->IgnoreActorWhenMoving(Player, false);
}
bool UHandcartMovementComponent::HasGroundClearance(const UBoxComponent* Box, const USceneComponent* Contact)
{
    constexpr float MinimumClearanceCm = 1.f;
    return IsValid(Box) && IsValid(Contact) &&
        Box->GetComponentLocation().Z - Contact->GetComponentLocation().Z >= Box->Bounds.BoxExtent.Z + MinimumClearanceCm - KINDA_SMALL_NUMBER;
}

FVector UHandcartMovementComponent::GetGroundContactOffset(const FQuat& Rotation) const
{
    const AActor* Cart = GetOwner();
    const FVector Offset = GroundContact->GetComponentLocation() - Cart->GetActorLocation();
    return Rotation.RotateVector(Cart->GetActorQuat().UnrotateVector(Offset));
}

bool UHandcartMovementComponent::SnapToGround(APawn* Player, bool bOnlyIfBelowGround)
{
    AActor* Cart=GetOwner();
    if (!HasGroundClearance(Collision, GroundContact)) return false;
    FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(CartGround),false,Cart);
    if (Player) Params.AddIgnoredActor(Player);
    const FVector Start=Cart->GetActorLocation();
    const FVector Contact = GroundContact->GetComponentLocation();
    if (!GetWorld()->LineTraceSingleByObjectType(Hit,Contact+FVector(0,0,200),Contact-FVector(0,0,800),FCollisionObjectQueryParams(ECC_WorldStatic),Params)) return false;
    if (Hit.ImpactNormal.Z<.65f) return false;
    const FVector Extent = Collision->GetScaledBoxExtent();
    const double GroundedZ = Start.Z + Hit.ImpactPoint.Z - Contact.Z;
    if (bOnlyIfBelowGround && Start.Z >= GroundedZ - KINDA_SMALL_NUMBER) return true;

    FVector Position = Start;
    Position.Z = GroundedZ;
    if (GetWorld()->OverlapBlockingTestByChannel(Position, Cart->GetActorQuat(),
        Collision->GetCollisionObjectType(), FCollisionShape::MakeBox(Extent), Params,
        FCollisionResponseParams(Collision->GetCollisionResponseToChannels())))
        return false;

    // A sweep would start inside the ground when recovering a low placed cart.
    Cart->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
    LastWheelLocation = Position;
    return true;
}
void UHandcartMovementComponent::Advance(APawn* Player)
{
    if (!IsValid(Player) || !HasGroundClearance(Collision, GroundContact)) return;
    AActor* Cart = GetOwner();
    const FRotator Rotation(0.f, Player->GetViewRotation().Yaw, 0.f);
    const float PushDistance = Settings ? Settings->CartPushDistance : 200.f;
    FVector Desired = Player->GetActorLocation() + Rotation.Vector() * PushDistance;
    Desired.Z = Cart->GetActorLocation().Z;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(CartMove), false, Cart);
    Params.AddIgnoredActor(Player);
    const FVector ContactOffset = GetGroundContactOffset(Rotation.Quaternion());
    const FVector Contact = Desired + ContactOffset;
    FHitResult Ground;
    if (!GetWorld()->LineTraceSingleByObjectType(Ground, Contact + FVector(0, 0, 100), Contact - FVector(0, 0, 250),
        FCollisionObjectQueryParams(ECC_WorldStatic), Params) || Ground.ImpactNormal.Z <= .65f)
        return;

    Desired.Z = Ground.ImpactPoint.Z - ContactOffset.Z;
    // Test the final orientation as the movement sweep does not sweep rotation itself.
    if (GetWorld()->OverlapBlockingTestByChannel(Desired, Rotation.Quaternion(), Collision->GetCollisionObjectType(),
        FCollisionShape::MakeBox(Collision->GetScaledBoxExtent()), Params,
        FCollisionResponseParams(Collision->GetCollisionResponseToChannels())))
        return;

    Cart->SetActorLocationAndRotation(Desired, Rotation, true, nullptr, ETeleportType::None);
}

void UHandcartMovementComponent::UpdateWheelRoll()
{
    AActor* Cart=GetOwner();
    const FVector Delta=Cart->GetActorLocation()-LastWheelLocation;LastWheelLocation=Cart->GetActorLocation();
    if (Delta.SizeSquared()>FMath::Square(200.f)) return; // Checkpoint teleports are not wheel travel.
    const float Distance=FVector::DotProduct(Delta,Cart->GetActorForwardVector());
    const float Radius=FMath::Max(1.f,(Settings ? Settings->CartWheelRadius : 51.f)*FMath::Abs(Cart->GetActorScale3D().Z));
    WheelAxle->AddLocalRotation(FQuat(FVector::RightVector,Distance/Radius));
}
