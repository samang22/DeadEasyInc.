#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Progression/Data/LoadoutData.h"
#include "BorderCheckpointComponent.generated.h"

USTRUCT()
struct FBorderCheckpointActor
{
    GENERATED_BODY()
    UPROPERTY() TWeakObjectPtr<AActor> Original;
    UPROPERTY() TSubclassOf<AActor> ActorClass;
    UPROPERTY() FTransform Transform;
    UPROPERTY() TArray<FName> Tags;
    UPROPERTY() TMap<FName, FTransform> PhysicsBodies;
    UPROPERTY() float Health = 0.f;
    UPROPERTY() float MaxHealth = 0.f;
    UPROPERTY() int32 Wood = 0;
    UPROPERTY() bool bCorpse = false;
    UPROPERTY() TObjectPtr<AActor> WallTarget;
    UPROPERTY() bool bHasLoadout = false;
    UPROPERTY() FLoadoutState LoadoutState;
    UPROPERTY() float RestorationProgress = 0.f;
    UPROPERTY() int32 UpgradeLevel=1;
    UPROPERTY() float Fuel=0.f;
    UPROPERTY() int32 TransportSlot=INDEX_NONE;
    UPROPERTY() FName TransportCart;
};

/** Owns the in-memory dawn snapshot and world restoration, without game-phase decisions. */
UCLASS(ClassGroup=(Game))
class DEADEASYINC_API UBorderCheckpointComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UBorderCheckpointComponent();
    void Capture(int32 Day);
    bool HasSnapshot() const { return !Dawn.IsEmpty(); }
    int32 Restore();
private:
    bool IsCheckpointActor(AActor* Actor) const;
    UPROPERTY(Transient) TArray<FBorderCheckpointActor> Dawn;
    FRotator DawnView;
    int32 CheckpointDay=1;
};
