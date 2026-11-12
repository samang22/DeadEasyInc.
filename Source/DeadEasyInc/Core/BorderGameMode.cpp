#include "Core/BorderGameMode.h"
#include "Core/Data/BorderRulesSettings.h"
#include "Buildings/Heating/CentralBoiler.h"
#include "Buildings/Building.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Interaction/BuildingRepairComponent.h"
#include "Zombies/BorderZombie.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/BodyInstance.h"
#include "TimerManager.h"

void ABorderGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (Settings)
    {
        DayDuration = Settings->DayDuration;
        NightDuration = Settings->NightDuration;
        RetryDelay = Settings->RetryDelay;
    }
    // The player pawn is spawned after the GameMode starts play.
    GetWorldTimerManager().SetTimerForNextTick(this, &ABorderGameMode::StartDay);
}

void ABorderGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearAllTimersForObject(this);
    Super::EndPlay(Reason);
}

bool ABorderGameMode::IsCheckpointActor(AActor* Actor) const
{
    return Actor == UGameplayStatics::GetPlayerPawn(this, 0) || Actor->IsA<ABuilding>() || Actor->IsA<ABorderZombie>();
}

void ABorderGameMode::BindDefeatConditions()
{
    AActor* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (*It == Player || It->IsA<ACentralBoiler>())
            if (UCombatHealthComponent* Health = It->FindComponentByClass<UCombatHealthComponent>())
                Health->OnDepleted.AddUniqueDynamic(this, &ABorderGameMode::HandleDefeat);
    }
}

void ABorderGameMode::ApplyPhase()
{
    for (TActorIterator<ABuilding> It(GetWorld()); It; ++It) It->bRepairAllowed = !bIsNight;
    GetWorldTimerManager().SetTimer(PhaseTimer, this,
        bIsNight ? &ABorderGameMode::StartDay : &ABorderGameMode::StartNight,
        FMath::Max(1.f, bIsNight ? NightDuration : DayDuration), false);
    OnPhaseChanged(bIsNight, DayNumber);
}

void ABorderGameMode::StartDay()
{
    if (bGameOver) return;
    if (bIsNight) ++DayNumber;
    bIsNight = false;
    BindDefeatConditions();
    ApplyPhase();
    CaptureDawn();
}

void ABorderGameMode::StartNight()
{
    if (bGameOver || bIsNight) return;
    bIsNight = true;
    ApplyPhase();
}

void ABorderGameMode::CaptureDawn()
{
    Dawn.Reset();
    CheckpointDay = DayNumber;
    if (APlayerController* Player = UGameplayStatics::GetPlayerController(this, 0)) DawnView = Player->GetControlRotation();
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsCheckpointActor(Actor)) continue;
        FBorderCheckpointActor& Record = Dawn.AddDefaulted_GetRef();
        Record.Original = Actor;
        Record.ActorClass = Actor->GetClass();
        Record.Transform = Actor->GetActorTransform();
        Record.Tags = Actor->Tags;
        if (UCombatHealthComponent* Health = Actor->FindComponentByClass<UCombatHealthComponent>())
        {
            Record.Health = Health->Health;
            Record.MaxHealth = Health->MaxHealth;
        }
        if (UBuildingRepairComponent* Repair = Actor->FindComponentByClass<UBuildingRepairComponent>()) Record.Wood = Repair->Wood;
        if (ABorderZombie* Zombie = Cast<ABorderZombie>(Actor))
        {
            Record.Health = Zombie->Health;
            Record.bCorpse = Zombie->bDead;
            Record.WallTarget = Zombie->WallTarget;
            if (Zombie->bDead)
            {
                TArray<FName> Bones;
                Zombie->GetMesh()->GetBoneNames(Bones);
                for (FName Bone : Bones)
                    if (FBodyInstance* Body = Zombie->GetMesh()->GetBodyInstance(Bone))
                        if (Body->IsInstanceSimulatingPhysics()) Record.PhysicsBodies.Add(Bone, Body->GetUnrealWorldTransform());
            }
        }
    }
}

void ABorderGameMode::HandleDefeat()
{
    if (bGameOver) return;
    bGameOver = true;
    GetWorldTimerManager().ClearTimer(PhaseTimer);
    if (APlayerController* Player = UGameplayStatics::GetPlayerController(this, 0))
    {
        Player->SetIgnoreMoveInput(true);
        Player->SetIgnoreLookInput(true);
        if (APawn* Pawn = Player->GetPawn())
            if (UBuildingRepairComponent* Repair = Pawn->FindComponentByClass<UBuildingRepairComponent>()) Repair->StopRepair();
    }
    for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
    {
        It->GetCharacterMovement()->StopMovementImmediately();
        if (It->IsA<ABorderZombie>()) It->SetActorTickEnabled(false);
    }
    OnGameOver();
    GetWorldTimerManager().SetTimer(RetryTimer, this, &ABorderGameMode::RetryFromDawn, FMath::Max(0.1f, RetryDelay), false);
}

void ABorderGameMode::RetryFromDawn()
{
    if (!bGameOver || Dawn.IsEmpty()) return;
    GetWorldTimerManager().ClearTimer(RetryTimer);
    // All zombies are recreated so death, attack wind-up and physics state are reset.
    TArray<AActor*> ToRemove;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (!IsCheckpointActor(*It)) continue;
        const bool bInDawn = Dawn.ContainsByPredicate([&](const FBorderCheckpointActor& R) { return R.Original.Get() == *It; });
        if (It->IsA<ABorderZombie>() || !bInDawn) ToRemove.Add(*It);
    }
    for (AActor* Actor : ToRemove) Actor->Destroy();
    for (FBorderCheckpointActor& Record : Dawn)
    {
        AActor* Actor = Record.Original.Get();
        if (!IsValid(Actor))
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Actor = GetWorld()->SpawnActor<AActor>(Record.ActorClass, Record.Transform, Params);
            Record.Original = Actor;
        }
        if (!Actor) continue;
        Actor->SetActorTransform(Record.Transform, false, nullptr, ETeleportType::TeleportPhysics);
        Actor->Tags = Record.Tags;
        if (UCombatHealthComponent* Health = Actor->FindComponentByClass<UCombatHealthComponent>())
        {
            Health->MaxHealth = Record.MaxHealth;
            Health->Health = Record.Health;
            Health->OnHealthChanged.Broadcast(Health->Health, Health->MaxHealth);
        }
        if (UBuildingRepairComponent* Repair = Actor->FindComponentByClass<UBuildingRepairComponent>())
        {
            Repair->StopRepair();
            Repair->Wood = Record.Wood;
        }
        if (ACentralBoiler* Boiler = Cast<ACentralBoiler>(Actor)) Boiler->bDestroyed = Record.Health <= 0.f;
        if (ABorderZombie* Zombie = Cast<ABorderZombie>(Actor))
        {
            Zombie->WallTarget = Record.WallTarget;
            Zombie->Health = Record.bCorpse ? 1.f : Record.Health;
            if (Record.bCorpse)
            {
                UGameplayStatics::ApplyDamage(Zombie, 1.f, nullptr, nullptr, nullptr);
                for (const auto& Pair : Record.PhysicsBodies)
                    if (FBodyInstance* Body = Zombie->GetMesh()->GetBodyInstance(Pair.Key))
                        Body->SetBodyTransform(Pair.Value, ETeleportType::TeleportPhysics);
                Zombie->GetMesh()->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
                Zombie->GetMesh()->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
            }
        }
        else if (ACharacter* Character = Cast<ACharacter>(Actor))
        {
            Character->GetCharacterMovement()->StopMovementImmediately();
            Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        }
    }
    if (APlayerController* Player = UGameplayStatics::GetPlayerController(this, 0))
    {
        Player->ResetIgnoreMoveInput();
        Player->ResetIgnoreLookInput();
        Player->SetControlRotation(DawnView);
    }
    DayNumber = CheckpointDay;
    bIsNight = false;
    bGameOver = false;
    BindDefeatConditions();
    ApplyPhase();
}
