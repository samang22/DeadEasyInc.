#include "Zombies/BorderZombie.h"
#include "Zombies/Components/ZombieCorpseComponent.h"
#include "Zombies/AI/BuildingTargetRegistry.h"
#include "Zombies/AI/ZombieNavigationComponent.h"
#include "Buildings/Building.h"
#include "Core/BorderGameMode.h"
#include "Zombies/Data/ZombieSettings.h"
#include "AIController.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

#include "Engine/World.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"


ABorderZombie::ABorderZombie()
{
    PrimaryActorTick.bCanEverTick = true;
    CorpsePhysics = CreateDefaultSubobject<UZombieCorpseComponent>(TEXT("CorpsePhysics"));
    Navigation = CreateDefaultSubobject<UZombieNavigationComponent>(TEXT("ZombieNavigation"));
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
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel4,ECR_Ignore);
    GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    GetMesh()->SetCollisionResponseToChannel(ECC_GameTraceChannel4,ECR_Block);
    GetMesh()->SetCanEverAffectNavigation(false);
    GetCapsuleComponent()->SetCanEverAffectNavigation(false);
    GetCharacterMovement()->bUseRVOAvoidance = true;
    GetCharacterMovement()->AvoidanceConsiderationRadius = 350.f;
}

void ABorderZombie::ApplyDayReinforcement(float Multiplier)
{
    Multiplier = FMath::Max(1.0f, Multiplier);

    // 체력
    Health *= Multiplier;

    // 공격력
    AttackDamage *= Multiplier;

    // // 탐지 범위
    // DetectionRadius *= Multiplier;

    // // 공격 사거리
    // AttackReach *= Multiplier;

    // // 이동 속도
    // if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    // {
    //     Movement->MaxWalkSpeed *= Multiplier;
    // }
}

void ABorderZombie::BeginPlay()
{
    Super::BeginPlay();
    GetCharacterMovement()->SetAvoidanceEnabled(true);
    // Weapon traces use Visibility. Pawn/CharacterMesh presets may ignore it,
    // including overrides serialized on existing Blueprint instances.
    SetCanBeDamaged(true);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel4,ECR_Ignore);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    GetMesh()->SetCollisionResponseToChannel(ECC_GameTraceChannel4,ECR_Block);
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
    WallTarget = nullptr;
    Navigation->InitializeRoute(*this);
    PlayAnimation(IdleAnimation, true);
    if (bStartAsCorpse && HasAuthority())
        UGameplayStatics::ApplyDamage(this, FMath::Max(1.f, Health), nullptr, nullptr, nullptr);
}

void ABorderZombie::EndPlay(const EEndPlayReason::Type Reason)
{
    UWorld* World = GetWorld();
    if (bDead && (Reason == EEndPlayReason::Destroyed || Reason == EEndPlayReason::RemovedFromWorld))
    {
        const auto* Mode = World ? World->GetAuthGameMode<ABorderGameMode>() : nullptr;
        UE_LOG(LogTemp, Warning, TEXT("Corpse removed: %s Reason=%d Day=%d Night=%d Position=%s"),
            *GetName(), static_cast<int32>(Reason), Mode ? Mode->DayNumber : 0, Mode && Mode->bIsNight, *GetActorLocation().ToString());
    }
    if (UBuildingTargetRegistry* Registry = World ? World->GetSubsystem<UBuildingTargetRegistry>() : nullptr)
        Registry->Release(this);
    Super::EndPlay(Reason);
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
    if (bDead) { TickCorpseInfluences(DeltaSeconds);return; }
    if (const ABorderGameMode* Mode = GetWorld()->GetAuthGameMode<ABorderGameMode>())
        if (!Mode->bIsNight || Mode->bGameOver || Mode->bTransitioning)
        {
            GetCharacterMovement()->StopMovementImmediately();
            PlayAnimation(IdleAnimation,true);
            return;
        }
    const float Now = GetWorld()->GetTimeSeconds();
    Navigation->UpdateTarget(*this, Now, CurrentObjective, CurrentTarget);
    if (bAttackPending && AttackTarget.Get()!=CurrentTarget) bAttackPending=false;
    if (bAttackPending)
    {
        if (Now >= AttackTime)
        {
            bAttackPending = false;
            if (UZombieNavigationComponent::CanReach(*this, AttackTarget.Get(), AttackReach))
            {
                AActor* Victim=AttackTarget.Get();
                FHitResult Hit;
                bool bHasSurfaceHit=false;
                const FVector Start=GetActorLocation();
                const FVector Point=UZombieNavigationComponent::GetTargetPoint(*this, *Victim);
                const FVector Direction=(Point-Start).GetSafeNormal();
                if (auto* Building=Cast<ABuilding>(Victim))
                {
                    FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieBuildingImpact),false,this);
                    // Component geometry traces may report touching hits rather than blocking hits.
                    bHasSurfaceHit=GetWorld()->LineTraceSingleByChannel(Hit,Start,Point+Direction*10.f,ECC_Visibility,Params) && Hit.GetActor()==Building;
                }
                if (bHasSurfaceHit)
                    UGameplayStatics::ApplyPointDamage(Victim,AttackDamage,Direction,Hit,GetController(),this,nullptr);
                else
                    UGameplayStatics::ApplyDamage(Victim,AttackDamage,GetController(),this,nullptr);
            }
        }
        else return;
    }
    AActor* Target = CurrentTarget;
    if (!Target) { PlayAnimation(IdleAnimation, true); return; }
    if (UZombieNavigationComponent::CanReach(*this, Target, AttackReach))
    {
        GetCharacterMovement()->StopMovementImmediately();
        FVector Facing = UZombieNavigationComponent::GetTargetPoint(*this, *Target) - GetActorLocation();
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
    FVector Direction;
    if (Navigation->TryGetMoveDirection(*this, Direction))
    {
        CorpsePhysics->PushNearbyCorpses(*this, Direction, DeltaSeconds);
        AddMovementInput(Direction);
    }
    else
    {
        GetCharacterMovement()->StopMovementImmediately();
    }
}

void ABorderZombie::ApplyCorpseCollision()
{
    CorpsePhysics->ApplyCollision(*GetMesh(), Settings);
}

void ABorderZombie::TickCorpseInfluences(float DeltaSeconds)
{
    CorpsePhysics->ApplyContactInfluences(*this);
}

bool ABorderZombie::IsHeadshot(const FHitResult& Hit) const
{
    if (bDead || Hit.GetComponent()!=GetMesh() || Hit.BoneName.IsNone()) return false;
    FName Bone=Hit.BoneName;
    while (!Bone.IsNone())
    {
        if (Settings ? Settings->HeadshotBones.Contains(Bone) : Bone==TEXT("head")) return true;
        const FName Parent=GetMesh()->GetParentBone(Bone);
        if (Parent==Bone) break;
        Bone=Parent;
    }
    return false;
}

TArray<FName> ABorderZombie::GetHeadshotPhysicsBodies() const
{
    TArray<FName> Result;
    const UPhysicsAsset* Physics=GetMesh()->GetPhysicsAsset();
    if (!Physics) return Result;
    const TArray<FName> Bones=Settings ? Settings->HeadshotBones : TArray<FName>{TEXT("head")};
    for (FName Bone : Bones) if (Physics->FindBodyIndex(Bone)!=INDEX_NONE) Result.Add(Bone);
    return Result;
}

float ABorderZombie::TakeDamage(float Amount, const FDamageEvent& Event, AController* InstigatorController, AActor* Causer)
{
    if (bDead || Amount <= 0) return 0.f;
    const float Applied = FMath::Min(Health, Amount);
    Health = FMath::Max(0.f, Health - Amount);
    if (Health <= 0)
    {
        bDead = true;
        // Corpses persist for daytime cleanup; only processing or checkpoint restore removes them.
        SetLifeSpan(0.f);
        Tags.AddUnique(TEXT("Dead"));
        GetCharacterMovement()->SetAvoidanceEnabled(false);
        UWorld* World = GetWorld();
        if (UBuildingTargetRegistry* Registry = World ? World->GetSubsystem<UBuildingTargetRegistry>() : nullptr)
            Registry->Release(this);
        GetCharacterMovement()->DisableMovement();
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        GetCapsuleComponent()->SetCanEverAffectNavigation(false);
        CorpsePhysics->ActivateRagdoll(*GetMesh(), Settings);
        if (Controller) Controller->UnPossess();
        SetActorTickEnabled(true);
        OnDied.Broadcast();
    }
    return Applied;
}
