#include "Scavenging/CorpseFeedComponent.h"
#include "Scavenging/CorpseProcessor.h"
#include "Scavenging/CorpseHandlingComponent.h"
#include "Scavenging/Handcart.h"
#include "Scavenging/ScavengingData.h"
#include "Zombies/BorderZombie.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"

UCorpseFeedComponent::UCorpseFeedComponent() { PrimaryComponentTick.bCanEverTick=false; }
void UCorpseFeedComponent::Configure(ACorpseProcessor* InProcessor,UBoxComponent* InIntake,USceneComponent*,UStaticMeshComponent*,UPhysicsHandleComponent*)
{
    // The old scene components remain on the Actor for Blueprint compatibility.
    Processor=InProcessor;Intake=InIntake;
}
void UCorpseFeedComponent::EndPlay(const EEndPlayReason::Type Reason) { Reset();Super::EndPlay(Reason); }
void UCorpseFeedComponent::Reset()
{
    for (const auto& Weak : ClaimedCorpses)
        if (auto* Corpse=Weak.Get(); Corpse && Corpse->ProcessingOwner.Get()==Processor.Get()) Corpse->ProcessingOwner.Reset();
    ClaimedCorpses.Reset();BatchTime=0.f;
}
TArray<ABorderZombie*> UCorpseFeedComponent::Advance(float Dt,APawn* Pawn,const UScavengingSettings* ScavengingSettings)
{
    TArray<ABorderZombie*> Result;
    if (!Intake || !Processor.IsValid() || !ScavengingSettings) return Result;
    BatchTime+=FMath::Max(0.f,Dt);
    if (BatchTime<FMath::Max(.1f,ScavengingSettings->ProcessingSeconds)) return Result;
    BatchTime=0.f;
    TArray<AActor*> Candidates;Intake->GetOverlappingActors(Candidates,ABorderZombie::StaticClass());
    auto* Work=Pawn ? Pawn->FindComponentByClass<UCorpseHandlingComponent>() : nullptr;
    for (AActor* Candidate : Candidates)
    {
        auto* Corpse=Cast<ABorderZombie>(Candidate);
        if (!IsValid(Corpse) || !Corpse->bDead || Corpse->IsActorBeingDestroyed() || Corpse->ProcessingOwner.IsValid()) continue;
        Corpse->ProcessingOwner=Processor.Get();ClaimedCorpses.Add(Corpse);
        if (Corpse->Carrier.IsValid()) Corpse->Carrier->DetachCorpse(Corpse);
        if (Work) Work->DetachCorpse(Corpse);
        Result.Add(Corpse);
    }
    return Result;
}
