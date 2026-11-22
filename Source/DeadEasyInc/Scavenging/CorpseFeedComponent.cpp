#include "Scavenging/CorpseFeedComponent.h"
#include "Scavenging/CorpseProcessor.h"
#include "Scavenging/CorpseHandlingComponent.h"
#include "Scavenging/Handcart.h"
#include "Scavenging/ScavengingData.h"
#include "Zombies/BorderZombie.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
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
    SuctionTargets.Reset();
    TimeUntilSuctionScan = 0.f;
}

bool UCorpseFeedComponent::CanPull(const ABorderZombie* Corpse) const
{
    return IsValid(Corpse) && Corpse->bDead && !Corpse->IsActorBeingDestroyed() &&
        !Corpse->HandlingOwner.IsValid() && !Corpse->Carrier.IsValid() &&
        !Corpse->IntakeOwner.IsValid() && !Corpse->ProcessingOwner.IsValid() &&
        Corpse->GetMesh()->IsSimulatingPhysics(Corpse->CorpsePushBone);
}

void UCorpseFeedComponent::FindSuctionTargets(const FVector& Target)
{
    SuctionTargets.Reset();
    TArray<FOverlapResult> Hits;
    const FCollisionQueryParams Params(SCENE_QUERY_STAT(SorterSuctionTargets), false, Processor.Get());
    GetWorld()->OverlapMultiByObjectType(Hits, Target, FQuat::Identity,
        FCollisionObjectQueryParams(ECC_GameTraceChannel3), FCollisionShape::MakeSphere(SuctionRadius), Params);
    for (const FOverlapResult& Hit : Hits)
        if (ABorderZombie* Corpse = Cast<ABorderZombie>(Hit.GetActor()); CanPull(Corpse))
            SuctionTargets.Add(Corpse);
}

void UCorpseFeedComponent::AdvanceSuction(float Dt)
{
    if (SuctionRadius <= 0.f || SuctionAcceleration <= 0.f || SuctionSpeed <= 0.f) return;
    const FVector Target = Intake->GetComponentLocation();
    TimeUntilSuctionScan -= Dt;
    if (TimeUntilSuctionScan <= 0.f)
    {
        FindSuctionTargets(Target);
        TimeUntilSuctionScan = .15f;
    }

    const FCollisionQueryParams Params(SCENE_QUERY_STAT(SorterSuctionObstruction), false, Processor.Get());
    const FCollisionObjectQueryParams Obstacles(ECC_WorldStatic);
    for (auto It = SuctionTargets.CreateIterator(); It; ++It)
    {
        const ABorderZombie* Corpse = It->Get();
        if (!CanPull(Corpse)) { It.RemoveCurrent(); continue; }

        USkeletalMeshComponent* Mesh = Corpse->GetMesh();
        const FName Bone = Corpse->CorpsePushBone;
        const FVector Position = Mesh->GetSocketLocation(Bone);
        const FVector Delta = Target - Position;
        if (Delta.SizeSquared() > FMath::Square(SuctionRadius)) continue;
        if (GetWorld()->LineTraceTestByObjectType(Position, Target, Obstacles, Params)) continue;

        const FVector DesiredVelocity = (Delta / .22f).GetClampedToMaxSize(SuctionSpeed);
        const FVector Velocity = Mesh->GetPhysicsLinearVelocity(Bone);
        FVector Acceleration = ((DesiredVelocity - Velocity) / .15f).GetClampedToMaxSize(SuctionAcceleration);
        Acceleration.Z -= GetWorld()->GetGravityZ();
        if (!Mesh->RigidBodyIsAwake(Bone)) Mesh->WakeAllRigidBodies();
        Mesh->AddForceToAllBodiesBelow(Acceleration, NAME_None, true, true);
    }
}

TArray<ABorderZombie*> UCorpseFeedComponent::Advance(float Dt,APawn* Pawn,const UScavengingSettings* ScavengingSettings)
{
    TArray<ABorderZombie*> Result;
    if (!Intake || !Processor.IsValid() || !ScavengingSettings) return Result;
    AdvanceSuction(FMath::Max(0.f, Dt));
    BatchTime+=FMath::Max(0.f,Dt);
    if (BatchTime<FMath::Max(.1f,ScavengingSettings->ProcessingSeconds)) return Result;
    BatchTime=0.f;
    TArray<AActor*> Candidates;Intake->GetOverlappingActors(Candidates,ABorderZombie::StaticClass());
    auto* Work=Pawn ? Pawn->FindComponentByClass<UCorpseHandlingComponent>() : nullptr;
    for (AActor* Candidate : Candidates)
    {
        auto* Corpse=Cast<ABorderZombie>(Candidate);
        if (!IsValid(Corpse) || !Corpse->bDead || Corpse->IsActorBeingDestroyed() || Corpse->ProcessingOwner.IsValid() || Corpse->IntakeOwner.IsValid()) continue;
        Corpse->ProcessingOwner=Processor.Get();ClaimedCorpses.Add(Corpse);
        if (Corpse->Carrier.IsValid()) Corpse->Carrier->DetachCorpse(Corpse);
        if (Work) Work->DetachCorpse(Corpse);
        Result.Add(Corpse);
    }
    return Result;
}
