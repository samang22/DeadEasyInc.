#include "Zombies/BorderZombie.h"
#include "Zombies/Data/ZombieSettings.h"
#include "AIController.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"

#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "PhysicsEngine/BodyInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"


ABorderZombie::ABorderZombie()
{
    PrimaryActorTick.bCanEverTick = true;
    AIControllerClass = AAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    bUseControllerRotationYaw = false;
    GetCapsuleComponent()->InitCapsuleSize(34.f, 90.f);
    GetCharacterMovement()->MaxWalkSpeed = 140.f;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0, 360, 0);
    // Use our bounded corpse push instead of CharacterMovement's generic physics forces.
    GetCharacterMovement()->bEnablePhysicsInteraction = false;
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
    GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));
    GetMesh()->SetCanEverAffectNavigation(false);
}

void ABorderZombie::BeginPlay()
{
    Super::BeginPlay();
    if (Settings)
    {
        Health = FMath::Max(1.f, Settings->MaxHealth);
        GetCharacterMovement()->MaxWalkSpeed = FMath::Max(0.f, Settings->WalkSpeed);
        DetectionRadius = Settings->DetectionRadius;
        AttackReach = Settings->AttackReach;
        AttackDamage = Settings->AttackDamage;
        AttackInterval = Settings->AttackInterval;
        CorpsePushReach = Settings->CorpsePushReach;
        CorpsePushAcceleration = Settings->CorpsePushAcceleration;
        CorpsePushMaxSpeed = Settings->CorpsePushMaxSpeed;
        CorpsePushBone = Settings->CorpsePushBone;
    }
    if (!WallTarget)
    {
        float BestDistance = TNumericLimits<float>::Max();
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            if (It->ActorHasTag(TEXT("BorderWall")))
            {
                const float Distance = FVector::DistSquared(GetActorLocation(), TargetPoint(*It));
                if (Distance < BestDistance) { BestDistance = Distance; WallTarget = *It; }
            }
        }
    }
    PlayAnimation(IdleAnimation, true);
}

FVector ABorderZombie::TargetPoint(AActor* Target) const
{
    FVector Point = Target->GetActorLocation();
    if (UPrimitiveComponent* Shape = Cast<UPrimitiveComponent>(Target->GetRootComponent()))
    {
        FVector Surface;
        if (Shape->GetClosestPointOnCollision(GetActorLocation(), Surface) >= 0.f) Point = Surface;
    }
    return Point;
}

bool ABorderZombie::CanReach(AActor* Target, float Range) const
{
    if (!IsValid(Target) || Target->ActorHasTag(TEXT("Dead"))) return false;
    const FVector Point = TargetPoint(Target);
    if (FVector::Dist(GetActorLocation(), Point) > Range + GetCapsuleComponent()->GetScaledCapsuleRadius()) return false;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieSight), false, this);
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(), Point, ECC_Visibility, Params);
    return !bHit || Hit.GetActor() == Target;
}

void ABorderZombie::PlayAnimation(UAnimSequence* Animation, bool bLoop)
{
    if (Animation && CurrentAnimation != Animation)
    {
        CurrentAnimation = Animation;
        GetMesh()->PlayAnimation(Animation, bLoop);
    }
}

void ABorderZombie::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bDead) return;
    const float Now = GetWorld()->GetTimeSeconds();
    if (bAttackPending)
    {
        if (Now >= AttackTime)
        {
            bAttackPending = false;
            if (CanReach(AttackTarget.Get(), AttackReach))
                UGameplayStatics::ApplyDamage(AttackTarget.Get(), AttackDamage, GetController(), this, nullptr);
        }
        else return;
    }
    AActor* Target = IsValid(WallTarget) && !WallTarget->ActorHasTag(TEXT("Dead")) ? WallTarget.Get() : nullptr;
    APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    if (CanReach(Player, DetectionRadius)) Target = Player;
    if (!Target) { PlayAnimation(IdleAnimation, true); return; }
    if (CanReach(Target, AttackReach))
    {
        GetCharacterMovement()->StopMovementImmediately();
        FVector Facing = TargetPoint(Target) - GetActorLocation();
        Facing.Z = 0;
        if (!Facing.IsNearlyZero()) SetActorRotation(Facing.Rotation());
        if (Now >= NextAttackTime)
        {
            CurrentAnimation = nullptr;
            PlayAnimation(AttackAnimation, false);
            AttackTarget = Target;
            AttackTime = Now + (Settings ? FMath::Max(0.f, Settings->AttackWindup) : 0.4f);
            NextAttackTime = Now + FMath::Max(0.5f, AttackInterval);
            bAttackPending = true;
        }
        return;
    }
    PlayAnimation(WalkAnimation, true);
    FVector Direction = TargetPoint(Target) - GetActorLocation();
    Direction.Z = 0;
    Direction = Direction.GetSafeNormal();
    PushCorpses(Direction, DeltaSeconds);
    AddMovementInput(Direction);
}

void ABorderZombie::PushCorpses(const FVector& Direction, float DeltaSeconds)
{
    if (Direction.IsNearlyZero() || DeltaSeconds <= SMALL_NUMBER) return;
    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float Reach = FMath::Max(0.f, CorpsePushReach);
    TArray<FOverlapResult> Overlaps;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieCorpsePush), false, this);
    GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation() + Direction * Reach,
        FQuat::Identity, Objects, FCollisionShape::MakeCapsule(Radius + Reach,
        FMath::Max(Radius + Reach, GetCapsuleComponent()->GetScaledCapsuleHalfHeight())), Params);

    TSet<ABorderZombie*> Visited;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        ABorderZombie* Corpse = Cast<ABorderZombie>(Overlap.GetActor());
        if (!Corpse || !Corpse->bDead || Visited.Contains(Corpse)) continue;
        Visited.Add(Corpse);
        USkeletalMeshComponent* CorpseMesh = Corpse->GetMesh();
        FVector Surface;
        if (CorpseMesh->GetClosestPointOnCollision(GetActorLocation(), Surface) < 0.f) continue;
        const FVector Offset = Surface - GetActorLocation();
        if (Offset.Size2D() > Radius + Reach || FVector::DotProduct(Offset, Direction) < -Radius) continue;

        // Do not reach through a wall or another living character to push a body.
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(), Surface, ECC_Visibility, Params)
            && Hit.GetActor() != Corpse) continue;

        FBodyInstance* Body = CorpseMesh->GetBodyInstance(Corpse->CorpsePushBone);
        if (!Body || !Body->IsInstanceSimulatingPhysics()) continue;
        const float Mass = Body->GetBodyMass();
        const float Speed = FVector::DotProduct(CorpseMesh->GetPhysicsLinearVelocity(Corpse->CorpsePushBone), Direction);
        const float Acceleration = FMath::Min(FMath::Max(0.f, CorpsePushAcceleration),
            FMath::Max(0.f, CorpsePushMaxSpeed - Speed) / DeltaSeconds);
        if (Corpse->LastPushFrame != GFrameCounter)
        {
            Corpse->LastPushFrame = GFrameCounter;
            Corpse->PushForceThisFrame = 0.f;
        }
        // A crowd must not multiply the force without bound on a single corpse.
        const float Budget = Mass * FMath::Max(0.f, Corpse->CorpsePushAcceleration);
        const float Force = FMath::Min(Mass * Acceleration, FMath::Max(0.f, Budget - Corpse->PushForceThisFrame));
        if (Force <= 0.f) continue;
        Corpse->PushForceThisFrame += Force;
        CorpseMesh->WakeAllRigidBodies();
        CorpseMesh->AddForce(Direction * Force, Corpse->CorpsePushBone);
    }
}

float ABorderZombie::TakeDamage(float Amount, const FDamageEvent& Event, AController* InstigatorController, AActor* Causer)
{
    if (bDead || Amount <= 0) return 0.f;
    const float Applied = FMath::Min(Health, Amount);
    Health = FMath::Max(0.f, Health - Amount);
    if (Health <= 0)
    {
        bDead = true;
        Tags.AddUnique(TEXT("Dead"));
        GetCharacterMovement()->DisableMovement();
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        GetCapsuleComponent()->SetCanEverAffectNavigation(false);
        GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
        GetMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        GetMesh()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
        GetMesh()->CanCharacterStepUpOn = ECB_No;
        GetMesh()->SetSimulatePhysics(true);
        if (Controller) Controller->UnPossess();
        SetActorTickEnabled(false);
    }
    return Applied;
}
