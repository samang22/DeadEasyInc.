#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TrackedCartMovementComponent.generated.h"

class UBoxComponent;

/** Kinematic tracked drive. Does not Tick independently; ElectricCart drives Advance(). */
UCLASS(ClassGroup=(Scavenging))
class DEADEASYINC_API UTrackedCartMovementComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UTrackedCartMovementComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tracked Drive", meta=(ClampMin="0"))
    float MaxForwardSpeed = 450.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tracked Drive", meta=(ClampMin="0"))
    float MaxReverseSpeed = 220.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tracked Drive", meta=(ClampMin="1"))
    float Acceleration = 350.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tracked Drive", meta=(ClampMin="1"))
    float BrakeDeceleration = 750.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tracked Drive", meta=(ClampMin="1"))
    float TurnSpeedDegrees = 75.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tracked Drive", meta=(ClampMin="0"))
    float PivotTurnSpeedDegrees = 45.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tracked Drive", meta=(ClampMin="1"))
    float TurnAcceleration = 180.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tracked Drive", meta=(ClampMin="0.01", ToolTip="Min allowed ground normal Z."))
    float MinGroundNormalZ = 0.65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tracked Drive", meta=(ClampMin="0"))
    float GroundClearance = 30.f;

    void Configure(UBoxComponent* InRootCollision);
    void SetInput(float Forward, float Turn);
    void StopInput();
    void ResetMotion();
    void Advance(float DeltaSeconds, const AActor* Driver);
    bool SnapToGround();
    float GetSpeed() const { return CurrentSpeed; }

private:
    UPROPERTY(Transient) TObjectPtr<UBoxComponent> RootCollision;
    float ThrottleInput = 0.f;
    float TurnInput = 0.f;
    float CurrentSpeed = 0.f;
    float CurrentYawRate = 0.f;
};
