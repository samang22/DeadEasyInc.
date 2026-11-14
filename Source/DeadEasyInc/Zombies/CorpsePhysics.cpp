#include "Zombies/BorderZombie.h"
#include "Zombies/Data/ZombieSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Core/BorderGameMode.h"
#include "Scavenging/Handcart.h"

void ABorderZombie::ApplyCorpseCollision()
{
    auto* CorpseMesh=GetMesh();
    CorpseMesh->SetCollisionObjectType(ECC_GameTraceChannel3);
    CorpseMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CorpseMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    CorpseMesh->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
    CorpseMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel3,ECR_Block);
    CorpseMesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    CorpseMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel4,ECR_Block);
    CorpseMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel5,ECR_Block);
    CorpseMesh->SetGenerateOverlapEvents(true);
    TArray<FName> Bones;CorpseMesh->GetBoneNames(Bones);
    const TArray<FName> DefaultBones={TEXT("pelvis"),TEXT("spine_01"),TEXT("spine_02"),TEXT("spine_03"),TEXT("spine_04"),TEXT("spine_05")};
    const auto& Solid=Settings ? Settings->SolidCorpseBones : DefaultBones;
    for (FName Bone : Bones) if (auto* Body=CorpseMesh->GetBodyInstance(Bone))
        Body->SetCollisionEnabled(Solid.Contains(Bone) ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void ABorderZombie::TickCorpseInfluences(float Dt)
{
    const auto* Mode=GetWorld()->GetAuthGameMode<ABorderGameMode>();
    if ((Mode && (Mode->bGameOver || Mode->bTransitioning)) || HandlingOwner.IsValid() || ProcessingOwner.IsValid()) return;
    const float Now=GetWorld()->GetTimeSeconds();if (Now<NextCorpseContactTime) return;
    NextCorpseContactTime=Now+.08f;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);Objects.AddObjectTypesToQuery(ECC_PhysicsBody);Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CorpseOneWayContact),false,this);
    TArray<FOverlapResult> Hits;
    const FVector Center=GetMesh()->GetSocketLocation(CorpsePushBone);
    GetWorld()->OverlapMultiByObjectType(Hits,Center,FQuat::Identity,Objects,FCollisionShape::MakeSphere(85.f),Params);
    FVector Force=FVector::ZeroVector;TSet<AActor*> Visited;
    auto* Body=GetMesh()->GetBodyInstance(CorpsePushBone);if (!Body) return;
    for (const auto& Hit : Hits)
    {
        AActor* Actor=Hit.GetActor();auto* Component=Hit.GetComponent();
        if (!Actor || !Component || Actor==GetOwner() || Actor==Carrier.Get() || Visited.Contains(Actor)) continue;
        // Living zombies already apply a bounded one-way force from their movement code.
        if (Actor->IsA<ABorderZombie>()) continue;
        Visited.Add(Actor);
        FVector Velocity=Component->IsSimulatingPhysics() ? Component->GetPhysicsLinearVelocity() : Actor->GetVelocity();Velocity.Z=0.f;
        if (Velocity.SizeSquared()<100.f) continue;
        FVector Surface;if (Component->GetClosestPointOnCollision(Center,Surface)<0.f || FVector::DistSquared(Surface,Center)>FMath::Square(85.f)) continue;
        FHitResult Obstacle;
        if (GetWorld()->LineTraceSingleByChannel(Obstacle,Surface,Center,ECC_Visibility,Params) && Obstacle.GetActor()!=this && Obstacle.GetActor()!=Actor) continue;
        const FVector Direction=Velocity.GetSafeNormal();
        if (FVector::DotProduct(Center-Surface,Direction)<-10.f) continue;
        const float Speed=FVector::DotProduct(GetMesh()->GetPhysicsLinearVelocity(CorpsePushBone),Direction);
        Force+=Direction*Body->GetBodyMass()*FMath::Min(CorpsePushAcceleration,FMath::Max(0.f,CorpsePushMaxSpeed-Speed)/.08f);
    }
    Force=Force.GetClampedToMaxSize(Body->GetBodyMass()*CorpsePushAcceleration);
    // Impulse only affects the corpse; the mover receives no reaction impulse.
    if (!Force.IsNearlyZero()) GetMesh()->AddImpulse(Force*.08f,CorpsePushBone);
}
