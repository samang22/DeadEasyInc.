#include "Scavenging/K17MovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UK17MovementComponent::UK17MovementComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UK17MovementComponent::SetInput(float Forward, float Right)
{
    ForwardInput = FMath::Clamp(Forward, -1.f, 1.f);
    RightInput = FMath::Clamp(Right, -1.f, 1.f);
    LastInputTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void UK17MovementComponent::ClearInput()
{
    ForwardInput = 0.f;
    RightInput = 0.f;
}

void UK17MovementComponent::ResetMotion()
{
    ClearInput();
    CurrentSpeed = 0.f;
}

bool UK17MovementComponent::TraceGroundAt(const FVector& Location, const AActor* Driver, FHitResult& OutHit) const
{
    UWorld* World = GetWorld();
    if (!World) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(K17Ground), false, GetOwner());
    if (Driver) Query.AddIgnoredActor(Driver);
    return World->LineTraceSingleByObjectType(
        OutHit, Location + FVector(0.f, 0.f, 300.f), Location - FVector(0.f, 0.f, 500.f),
        FCollisionObjectQueryParams(ECC_WorldStatic), Query);
}

bool UK17MovementComponent::SnapToGround(const UBoxComponent& Collision, const AActor* Driver,
                                        float GroundClearance, float MinGroundNormalZ)
{
    AActor* Vehicle = GetOwner();
    if (!Vehicle) return false;
    FVector Location = Vehicle->GetActorLocation();
    FHitResult Ground;
    if (!TraceGroundAt(Location, Driver, Ground) || Ground.ImpactNormal.Z < MinGroundNormalZ)
        return false;
    Location.Z = Ground.ImpactPoint.Z + Collision.GetScaledBoxExtent().Z + GroundClearance;
    Vehicle->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
    return true;
}

void UK17MovementComponent::Advance(float DeltaTime, const UBoxComponent& Collision, const AActor* Driver,
                                   const FK17DriveTuning& Tuning, TArrayView<USceneComponent* const> Wheels)
{
    AActor* Vehicle = GetOwner();
    UWorld* World = GetWorld();
    if (!Vehicle || !Driver || !World) return;

    // Neutralize input when Enhanced Input stops calling the action.
    if (World->GetTimeSeconds() - LastInputTime > 0.15f)
        ClearInput();

    const float TargetSpeed = ForwardInput >= 0.f
        ? ForwardInput * Tuning.MaxForwardSpeed : ForwardInput * Tuning.MaxReverseSpeed;
    const float SpeedRate = FMath::IsNearlyZero(ForwardInput) || FMath::Abs(TargetSpeed) < FMath::Abs(CurrentSpeed)
        ? Tuning.Deceleration : Tuning.Acceleration;
    CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, TargetSpeed, DeltaTime, SpeedRate);
    if (FMath::IsNearlyZero(CurrentSpeed, 0.01f) && FMath::IsNearlyZero(RightInput, 0.01f)) return;

    const FVector OldLocation = Vehicle->GetActorLocation();
    const float OldYaw = Vehicle->GetActorRotation().Yaw;
    const float TargetYaw = OldYaw + RightInput * Tuning.TurnSpeedDegrees * DeltaTime;
    const FRotator NewRotation(0.f, TargetYaw, 0.f);
    const FVector Forward = NewRotation.Vector();
    FVector Destination = OldLocation + Forward * CurrentSpeed * DeltaTime;

    // Sample the center, front and rear so the root box clears mild slopes.
    FHitResult Center, Front, Rear;
    const bool bCenter = TraceGroundAt(Destination, Driver, Center);
    const bool bFront = TraceGroundAt(Destination + Forward * 150.f, Driver, Front);
    const bool bRear = TraceGroundAt(Destination - Forward * 150.f, Driver, Rear);
    if (!bCenter || Center.ImpactNormal.Z < Tuning.MinGroundNormalZ)
    {
        CurrentSpeed = 0.f;
        return;
    }
    float HighestGroundZ = Center.ImpactPoint.Z;
    if (bFront && Front.ImpactNormal.Z >= Tuning.MinGroundNormalZ)
        HighestGroundZ = FMath::Max(HighestGroundZ, Front.ImpactPoint.Z);
    if (bRear && Rear.ImpactNormal.Z >= Tuning.MinGroundNormalZ)
        HighestGroundZ = FMath::Max(HighestGroundZ, Rear.ImpactPoint.Z);
    Destination.Z = HighestGroundZ + Collision.GetScaledBoxExtent().Z + Tuning.GroundClearance;

    FHitResult Hit;
    Vehicle->SetActorLocationAndRotation(Destination, NewRotation, true, &Hit, ETeleportType::None);
    if (Hit.bBlockingHit) CurrentSpeed = 0.f;

    AnimateWheels(OldLocation, OldYaw, Tuning.WheelRadiusCm, Wheels);
}

void UK17MovementComponent::AnimateWheels(const FVector& OldLocation, float OldYaw, float WheelRadiusCm,
                                         TArrayView<USceneComponent* const> Wheels) const
{
    const AActor* Vehicle = GetOwner();
    const float ForwardDist = FVector::DotProduct(Vehicle->GetActorLocation() - OldLocation, Vehicle->GetActorForwardVector());
    const float YawRadians = FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(OldYaw, Vehicle->GetActorRotation().Yaw));
    const float HalfTrackCm = 94.f;
    const float LeftDegrees = FMath::RadiansToDegrees((ForwardDist - YawRadians * HalfTrackCm) / WheelRadiusCm);
    const float RightDegrees = FMath::RadiansToDegrees((ForwardDist + YawRadians * HalfTrackCm) / WheelRadiusCm);
    // Wheels are supplied left pair first and then right pair.
    for (int32 Index = 0; Index < Wheels.Num(); ++Index)
        if (USceneComponent* Wheel = Wheels[Index])
            Wheel->AddLocalRotation(FRotator(Index < 2 ? -LeftDegrees : -RightDegrees, 0.f, 0.f));
}
