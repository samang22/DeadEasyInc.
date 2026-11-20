#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/ArrayView.h"
#include "K17MovementComponent.generated.h"

class UBoxComponent;
class USceneComponent;

struct FK17DriveTuning
{
    float MaxForwardSpeed;
    float MaxReverseSpeed;
    float Acceleration;
    float Deceleration;
    float TurnSpeedDegrees;
    float WheelRadiusCm;
    float GroundClearance;
    float MinGroundNormalZ;
};

/** Kinematic drive and wheel roll advanced by the owner's Tick before its camera update. */
UCLASS(ClassGroup=(Scavenging))
class DEADEASYINC_API UK17MovementComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UK17MovementComponent();

    void SetInput(float Forward, float Right);
    void ClearInput();
    void ResetMotion();
    float GetSpeed() const { return CurrentSpeed; }

    bool SnapToGround(const UBoxComponent& Collision, const AActor* Driver,
                      float GroundClearance, float MinGroundNormalZ);
    // Wheel order is front-left, rear-left, front-right, rear-right.
    void Advance(float DeltaTime, const UBoxComponent& Collision, const AActor* Driver,
                 const FK17DriveTuning& Tuning, TArrayView<USceneComponent* const> Wheels);

private:
    bool TraceGroundAt(const FVector& Location, const AActor* Driver, FHitResult& OutHit) const;
    void AnimateWheels(const FVector& OldLocation, float OldYaw, float WheelRadiusCm,
                       TArrayView<USceneComponent* const> Wheels) const;

    float ForwardInput = 0.f;
    float RightInput = 0.f;
    float CurrentSpeed = 0.f;
    float LastInputTime = -1000.f;
};
