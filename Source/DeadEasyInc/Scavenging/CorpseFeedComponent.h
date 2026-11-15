#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CorpseFeedComponent.generated.h"

/** Collects overlapping corpses at a fixed batch interval. No resource policy or physics movement. */
UCLASS(ClassGroup=(Scavenging))
class DEADEASYINC_API UCorpseFeedComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCorpseFeedComponent();
    void Configure(class ACorpseProcessor* InProcessor,class UBoxComponent* InIntake,class USceneComponent* InConsumePoint,class UStaticMeshComponent* InHousing,class UPhysicsHandleComponent* InMotor);
    TArray<class ABorderZombie*> Advance(float Dt,class APawn* Pawn,const class UScavengingSettings* ScavengingSettings);
    void Reset();
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    TWeakObjectPtr<class ACorpseProcessor> Processor;
    UPROPERTY(Transient) TObjectPtr<class UBoxComponent> Intake;
    TArray<TWeakObjectPtr<class ABorderZombie>> ClaimedCorpses;
    float BatchTime=0.f;
};
