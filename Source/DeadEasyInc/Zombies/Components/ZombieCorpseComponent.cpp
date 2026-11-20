#include "Zombies/Components/ZombieCorpseComponent.h"
#include "Zombies/BorderZombie.h"
#include "Zombies/Data/ZombieSettings.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Core/BorderGameMode.h"
#include "Scavenging/Handcart.h"

UZombieCorpseComponent::UZombieCorpseComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UZombieCorpseComponent::CapturePose(USkeletalMeshComponent& Mesh, TMap<FName, FTransform>& OutBodies) const
{
    TArray<FName> Bones;
    Mesh.GetBoneNames(Bones);
    for (const FName Bone : Bones)
        if (const FBodyInstance* Body = Mesh.GetBodyInstance(Bone); Body && Body->IsInstanceSimulatingPhysics())
            OutBodies.Add(Bone, Body->GetUnrealWorldTransform());
}

void UZombieCorpseComponent::RestorePose(USkeletalMeshComponent& Mesh, const TMap<FName, FTransform>& Bodies) const
{
    for (const auto& Pair : Bodies)
        if (FBodyInstance* Body = Mesh.GetBodyInstance(Pair.Key))
            Body->SetBodyTransform(Pair.Value, ETeleportType::TeleportPhysics);
    Mesh.SetAllPhysicsLinearVelocity(FVector::ZeroVector);
    Mesh.SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
}

void UZombieCorpseComponent::ActivateRagdoll(USkeletalMeshComponent& Mesh, const UZombieSettings* Settings)
{
    Mesh.SetCollisionProfileName(TEXT("Ragdoll"));
    Mesh.SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    Mesh.SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    Mesh.SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
    Mesh.CanCharacterStepUpOn = ECB_No;
    Mesh.SetSimulatePhysics(true);
    ApplyCollision(Mesh, Settings);
}

void UZombieCorpseComponent::ApplyCollision(USkeletalMeshComponent& Mesh, const UZombieSettings* Settings)
{
    Mesh.SetCollisionObjectType(ECC_GameTraceChannel3);
    Mesh.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Mesh.SetCollisionResponseToAllChannels(ECR_Ignore);
    Mesh.SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
    Mesh.SetCollisionResponseToChannel(ECC_GameTraceChannel3,ECR_Block);
    Mesh.SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    Mesh.SetCollisionResponseToChannel(ECC_GameTraceChannel4,ECR_Block);
    Mesh.SetCollisionResponseToChannel(ECC_GameTraceChannel5,ECR_Block);
    Mesh.SetGenerateOverlapEvents(true);
    TArray<FName> Bones;Mesh.GetBoneNames(Bones);
    const TArray<FName> DefaultBones={TEXT("pelvis"),TEXT("spine_01"),TEXT("spine_02"),TEXT("spine_03"),TEXT("spine_04"),TEXT("spine_05")};
    const auto& Solid=Settings ? Settings->SolidCorpseBones : DefaultBones;
    for (FName Bone : Bones) if (auto* Body=Mesh.GetBodyInstance(Bone))
        Body->SetCollisionEnabled(Solid.Contains(Bone) ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void UZombieCorpseComponent::ApplyContactInfluences(const ABorderZombie& Corpse)
{
    const auto* Mode=Corpse.GetWorld()->GetAuthGameMode<ABorderGameMode>();
    if ((Mode && (Mode->bGameOver || Mode->bTransitioning)) || Corpse.HandlingOwner.IsValid() || Corpse.ProcessingOwner.IsValid() || Corpse.IntakeOwner.IsValid()) return;
    const float Now=Corpse.GetWorld()->GetTimeSeconds();if (Now<NextCorpseContactTime) return;
    NextCorpseContactTime=Now+.08f;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);Objects.AddObjectTypesToQuery(ECC_PhysicsBody);Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CorpseOneWayContact),false,&Corpse);
    TArray<FOverlapResult> Hits;
    auto* Mesh = Corpse.GetMesh();
    const FName PushBone = Corpse.CorpsePushBone;
    const FVector Center=Mesh->GetSocketLocation(PushBone);
    Corpse.GetWorld()->OverlapMultiByObjectType(Hits,Center,FQuat::Identity,Objects,FCollisionShape::MakeSphere(85.f),Params);
    FVector Force=FVector::ZeroVector;TSet<AActor*> Visited;
    auto* Body=Mesh->GetBodyInstance(PushBone);if (!Body) return;
    for (const auto& Hit : Hits)
    {
        AActor* Actor=Hit.GetActor();auto* Component=Hit.GetComponent();
        if (!Actor || !Component || Actor==Corpse.GetOwner() || Actor==Corpse.Carrier.Get() || Visited.Contains(Actor)) continue;
        // Living zombies already apply a bounded one-way force from their movement code.
        if (Actor->IsA<ABorderZombie>()) continue;
        Visited.Add(Actor);
        FVector Velocity=Component->IsSimulatingPhysics() ? Component->GetPhysicsLinearVelocity() : Actor->GetVelocity();Velocity.Z=0.f;
        if (Velocity.SizeSquared()<100.f) continue;
        FVector Surface;if (Component->GetClosestPointOnCollision(Center,Surface)<0.f || FVector::DistSquared(Surface,Center)>FMath::Square(85.f)) continue;
        FHitResult Obstacle;
        if (Corpse.GetWorld()->LineTraceSingleByChannel(Obstacle,Surface,Center,ECC_Visibility,Params) && Obstacle.GetActor()!=&Corpse && Obstacle.GetActor()!=Actor) continue;
        const FVector Direction=Velocity.GetSafeNormal();
        if (FVector::DotProduct(Center-Surface,Direction)<-10.f) continue;
        const float Speed=FVector::DotProduct(Mesh->GetPhysicsLinearVelocity(PushBone),Direction);
        Force+=Direction*Body->GetBodyMass()*FMath::Min(Corpse.CorpsePushAcceleration,FMath::Max(0.f,Corpse.CorpsePushMaxSpeed-Speed)/.08f);
    }
    Force=Force.GetClampedToMaxSize(Body->GetBodyMass()*Corpse.CorpsePushAcceleration);
    // Impulse only affects the corpse; the mover receives no reaction impulse.
    if (!Force.IsNearlyZero()) Mesh->AddImpulse(Force*.08f,PushBone);
}

float UZombieCorpseComponent::ConsumePushForce(float RequestedForce, float Budget)
{
    if (LastPushFrame != GFrameCounter)
    {
        LastPushFrame = GFrameCounter;
        PushForceThisFrame = 0.f;
    }
    // A crowd must not multiply the force without bound on a single corpse.
    const float Force = FMath::Min(RequestedForce, FMath::Max(0.f, Budget - PushForceThisFrame));
    if (Force > 0.f) PushForceThisFrame += Force;
    return Force;
}

void UZombieCorpseComponent::PushNearbyCorpses(const ABorderZombie& Mover, const FVector& Direction, float DeltaSeconds)
{
    if (Direction.IsNearlyZero() || DeltaSeconds <= SMALL_NUMBER) return;
    const float Radius = Mover.GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float Reach = FMath::Max(0.f, Mover.CorpsePushReach);
    TArray<FOverlapResult> Overlaps;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
    Objects.AddObjectTypesToQuery(ECC_GameTraceChannel3);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieCorpsePush), false, &Mover);
    Mover.GetWorld()->OverlapMultiByObjectType(Overlaps, Mover.GetActorLocation() + Direction * Reach,
        FQuat::Identity, Objects, FCollisionShape::MakeCapsule(Radius + Reach,
        FMath::Max(Radius + Reach, Mover.GetCapsuleComponent()->GetScaledCapsuleHalfHeight())), Params);

    TSet<ABorderZombie*> Visited;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        ABorderZombie* Corpse = Cast<ABorderZombie>(Overlap.GetActor());
        if (!Corpse || !Corpse->bDead || Visited.Contains(Corpse)) continue;
        Visited.Add(Corpse);
        USkeletalMeshComponent* CorpseMesh = Corpse->GetMesh();
        FVector Surface;
        if (CorpseMesh->GetClosestPointOnCollision(Mover.GetActorLocation(), Surface) < 0.f) continue;
        const FVector Offset = Surface - Mover.GetActorLocation();
        if (Offset.Size2D() > Radius + Reach || FVector::DotProduct(Offset, Direction) < -Radius) continue;

        // Do not reach through a wall or another living character to push a body.
        FHitResult Hit;
        if (Mover.GetWorld()->LineTraceSingleByChannel(Hit, Mover.GetActorLocation(), Surface, ECC_Visibility, Params)
            && Hit.GetActor() != Corpse) continue;

        FBodyInstance* Body = CorpseMesh->GetBodyInstance(Corpse->CorpsePushBone);
        if (!Body || !Body->IsInstanceSimulatingPhysics()) continue;
        const float Mass = Body->GetBodyMass();
        const float Speed = FVector::DotProduct(CorpseMesh->GetPhysicsLinearVelocity(Corpse->CorpsePushBone), Direction);
        const float Acceleration = FMath::Min(FMath::Max(0.f, Mover.CorpsePushAcceleration),
            FMath::Max(0.f, Mover.CorpsePushMaxSpeed - Speed) / DeltaSeconds);
        const float Budget = Mass * FMath::Max(0.f, Corpse->CorpsePushAcceleration);
        const float Force = Corpse->GetCorpsePhysics()->ConsumePushForce(Mass * Acceleration, Budget);
        if (Force <= 0.f) continue;
        CorpseMesh->WakeAllRigidBodies();
        CorpseMesh->AddForce(Direction * Force, Corpse->CorpsePushBone);
    }
}
