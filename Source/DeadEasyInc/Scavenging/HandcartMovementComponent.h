#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HandcartMovementComponent.generated.h"

/** Owns follow motion, driver speed limiting, ground placement and wheel travel. */
UCLASS(ClassGroup=(Scavenging))
class DEADEASYINC_API UHandcartMovementComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHandcartMovementComponent();
    void Configure(class UScavengingSettings* InSettings,class UBoxComponent* InCollision,class USceneComponent* InWheelAxle);
    void BeginPush(class APawn* Player);
    void EndPush(class APawn* Player);
    void ResetMotion();
    bool SnapToGround(class APawn* Player);
    void Advance(float Dt,class APawn* Player);
    void UpdateWheelRoll();
private:
    UPROPERTY(Transient) TObjectPtr<class UScavengingSettings> Settings;
    UPROPERTY(Transient) TObjectPtr<class UBoxComponent> Collision;
    UPROPERTY(Transient) TObjectPtr<class USceneComponent> WheelAxle;
    float NormalSpeed=500.f;
    FVector LastWheelLocation=FVector::ZeroVector;
    FVector FollowVelocity=FVector::ZeroVector;
    float FollowYawRate=0.f;
};
