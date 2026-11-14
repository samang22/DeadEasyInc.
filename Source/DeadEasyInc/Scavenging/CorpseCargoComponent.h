#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CorpseCargoComponent.generated.h"

class ABorderZombie;
class AHandcart;
class UScavengingSettings;
class UBoxComponent;
class UPrimitiveComponent;
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCargoDetached,ABorderZombie*);

// Observes freely simulated bodies. Never grabs, moves or constrains them.
UCLASS(ClassGroup=(Scavenging),meta=(BlueprintSpawnableComponent))
class DEADEASYINC_API UCorpseCargoComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCorpseCargoComponent();
    FOnCargoDetached OnAttached;
    FOnCargoDetached OnDetached;
    void Configure(UScavengingSettings* InSettings,UBoxComponent* InVolume);
    void RefreshTracking();
    TArray<ABorderZombie*> GetCorpses() const;
    // Compatibility entry: observes the actual position; does not load into a slot.
    bool LoadCorpse(ABorderZombie* Corpse,int32 RequestedSlot,int32& AssignedSlot);
    void DetachCorpse(ABorderZombie* Corpse);
    void ReleaseCargo();
    int32 CargoCount() const;
    ABorderZombie* FirstCorpse() const;
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<UScavengingSettings> Settings;
    TWeakObjectPtr<UBoxComponent> Volume;
    TWeakObjectPtr<AHandcart> Cart;
    TSet<TWeakObjectPtr<ABorderZombie>> Candidates;
    TSet<TWeakObjectPtr<ABorderZombie>> Tracked;
    FTimerHandle MembershipTimer;
    bool IsInside(ABorderZombie* Corpse,float Margin) const;
    bool TryTrack(ABorderZombie* Corpse);
    void UpdateMembership();
    void UpdateTimer();
    UFUNCTION() void OnEnter(UPrimitiveComponent* Overlapped,AActor* Other,UPrimitiveComponent* OtherComponent,int32 BodyIndex,bool FromSweep,const FHitResult& Hit);
    UFUNCTION() void OnLeave(UPrimitiveComponent* Overlapped,AActor* Other,UPrimitiveComponent* OtherComponent,int32 BodyIndex);
};
