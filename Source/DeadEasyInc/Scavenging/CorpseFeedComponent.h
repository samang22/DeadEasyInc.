#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CorpseFeedComponent.generated.h"

/** Pulls nearby free ragdolls into the intake and collects overlaps at the existing batch interval. */
UCLASS(ClassGroup=(Scavenging))
class DEADEASYINC_API UCorpseFeedComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCorpseFeedComponent();
    void Configure(class ACorpseProcessor* InProcessor,class UBoxComponent* InIntake,class USceneComponent* InConsumePoint,class UStaticMeshComponent* InHousing,class UPhysicsHandleComponent* InMotor);
    TArray<class ABorderZombie*> Advance(float Dt,class APawn* Pawn,const class UScavengingSettings* ScavengingSettings);
    void Reset();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Intake|Suction", meta=(ClampMin="0", Units="cm"))
    float SuctionRadius = 180.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Intake|Suction", meta=(ClampMin="0", Units="cm/s^2"))
    float SuctionAcceleration = 1800.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Intake|Suction", meta=(ClampMin="0", Units="cm/s"))
    float SuctionSpeed = 450.f;
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void AdvanceSuction(float Dt);
    void FindSuctionTargets(const FVector& Target);
    bool CanPull(const ABorderZombie* Corpse) const;

    TWeakObjectPtr<class ACorpseProcessor> Processor;
    UPROPERTY(Transient) TObjectPtr<class UBoxComponent> Intake;
    TArray<TWeakObjectPtr<class ABorderZombie>> ClaimedCorpses;
    TSet<TWeakObjectPtr<ABorderZombie>> SuctionTargets;
    float BatchTime=0.f;
    float TimeUntilSuctionScan = 0.f;
};
