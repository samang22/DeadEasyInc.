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
    void Configure(class UScavengingSettings* InSettings, class UBoxComponent* InCollision,
                   class USceneComponent* InWheelAxle, class USceneComponent* InGroundContact);
    static bool HasGroundClearance(const UBoxComponent* Box, const USceneComponent* Contact);
    bool BeginPush(class APawn* Player);
    void EndPush(class APawn* Player);
    bool SnapToGround(class APawn* Player, bool bOnlyIfBelowGround = false);
    void Advance(class APawn* Player);
    void UpdateWheelRoll();
private:
    FVector GetGroundContactOffset(const FQuat& Rotation) const;
    float GetPushDistance() const;
    UPROPERTY(Transient) TObjectPtr<class UScavengingSettings> Settings;
    UPROPERTY(Transient) TObjectPtr<class UBoxComponent> Collision;
    UPROPERTY(Transient) TObjectPtr<class USceneComponent> WheelAxle;
    UPROPERTY(Transient) TObjectPtr<USceneComponent> GroundContact;
    float NormalSpeed=500.f;
    FVector LastWheelLocation=FVector::ZeroVector;
    FVector LastDriverLocation=FVector::ZeroVector;
    float LastDriverYaw=0.f;
};
