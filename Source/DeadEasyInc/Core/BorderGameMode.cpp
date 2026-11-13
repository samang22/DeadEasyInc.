#include "Core/BorderGameMode.h"
#include "Core/DayNight/DayNightDirector.h"
#include "Buildings/Defense/BorderGate.h"
#include "Progression/PlayerLoadoutComponent.h"
#include "Core/Data/BorderRulesSettings.h"
#include "Buildings/Heating/CentralBoiler.h"
#include "Buildings/Building.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Interaction/BuildingRepairComponent.h"
#include "Zombies/BorderZombie.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
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
        TransitionDuration = Settings->TransitionDuration;
        DawnDelay = Settings->DawnDelay;
        RetryDelay = Settings->RetryDelay;
    }
    for (TActorIterator<ADayNightDirector> It(GetWorld()); It; ++It) { Director = *It; break; }
    if (Director) Director->OnTransitionFinished.AddDynamic(this,&ABorderGameMode::FinishNightTransition);
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
    GetWorldTimerManager().ClearTimer(PhaseTimer);
    for (TActorIterator<ABuilding> It(GetWorld()); It; ++It) It->bRepairAllowed = !bIsNight && !bTransitioning && !bGameOver;
    if (!bIsNight && !bTransitioning)
    {
        if (Director) Director->ResetDay();
        for (TActorIterator<ABorderGate> It(GetWorld()); It; ++It)
        {
            It->SetTransitionBarrier(false);
            It->SetOpenFraction(1.f);
        }
    }
    OnPhaseChanged(bIsNight,DayNumber);
    OnPhaseStateChanged.Broadcast(bIsNight,DayNumber);
}

void ABorderGameMode::StartDay()
{
    if (bGameOver || bTransitioning) return;
    if (Dawn.IsEmpty())
        if (APawn* Player = UGameplayStatics::GetPlayerPawn(this,0))
            if (UPlayerLoadoutComponent* Loadout = Player->FindComponentByClass<UPlayerLoadoutComponent>()) Loadout->LoadDawn(DayNumber);
    if (bIsNight) ++DayNumber;
    bIsNight = false;
    bWaveRegistered = false;
    RemainingZombies = 0;
    NightZombies.Reset();
    WaveSources.Reset();
    FinishedSources.Reset();
    BindDefeatConditions();
    ApplyPhase();
    CaptureDawn();
    if (APawn* Player = UGameplayStatics::GetPlayerPawn(this,0))
        if (UPlayerLoadoutComponent* Loadout = Player->FindComponentByClass<UPlayerLoadoutComponent>()) Loadout->SaveDawn(DayNumber);
}

void ABorderGameMode::StartNight()
{
    for (TActorIterator<ABorderGate> It(GetWorld()); It; ++It) { BeginNight(*It); return; }
}

bool ABorderGameMode::CanBeginNight(ABorderGate* Gate) const
{
    return !bGameOver && !bIsNight && !bTransitioning && IsValid(Gate) && Gate->IsPassageClear();
}

bool ABorderGameMode::BeginNight(ABorderGate* Gate)
{
    if (!CanBeginNight(Gate)) return false;
    ActiveGate = Gate;
    bTransitioning = true;
    LockTransitionInput(true);
    Gate->SetTransitionBarrier(true);
    ApplyPhase();
    if (Director) Director->BeginTransition(Gate,TransitionDuration);
    // Missing or interrupted sequences cannot leave the game locked forever.
    GetWorldTimerManager().SetTimer(PhaseTimer,this,&ABorderGameMode::FinishNightTransition,FMath::Max(.1f,TransitionDuration)+.25f,false);
    return true;
}

void ABorderGameMode::LockTransitionInput(bool bLocked)
{
    if (APlayerController* Player = UGameplayStatics::GetPlayerController(this,0))
    {
        if (APawn* Pawn = Player->GetPawn())
        {
            if (UPlayerLoadoutComponent* Loadout = Pawn->FindComponentByClass<UPlayerLoadoutComponent>()) Loadout->StopActions();
            if (UBuildingRepairComponent* Repair = Pawn->FindComponentByClass<UBuildingRepairComponent>()) Repair->StopRepair();
        }
        Player->SetIgnoreMoveInput(bLocked);
        Player->SetIgnoreLookInput(bLocked);
    }
}

void ABorderGameMode::FinishNightTransition()
{
    if (!bTransitioning || bGameOver) return;
    GetWorldTimerManager().ClearTimer(PhaseTimer);
    if (Director) Director->SetNight();
    if (ActiveGate.IsValid())
    {
        ActiveGate->SetOpenFraction(0.f);
        ActiveGate->SetTransitionBarrier(false);
    }
    bTransitioning = false;
    bIsNight = true;
    bWaveRegistered = false;
    NightZombies.Reset(); WaveSources.Reset(); FinishedSources.Reset();
    LockTransitionInput(false);
    for (TActorIterator<ABorderZombie> It(GetWorld()); It; ++It)
        if (!It->bDead) RegisterNightZombie(*It);
    // Placed enemies form a finite encounter until a proper spawner registers.
    if (!NightZombies.IsEmpty()) bWaveRegistered = true;
    ApplyPhase();
    CheckNightCompletion();
}

void ABorderGameMode::RegisterWaveSource(AActor* Source)
{
    if (!bIsNight || bGameOver || !IsValid(Source)) return;
    bWaveRegistered = true;
    WaveSources.Add(Source);
    GetWorldTimerManager().ClearTimer(PhaseTimer);
}

void ABorderGameMode::ReportSpawningFinished(AActor* Source)
{
    if (WaveSources.Contains(Source)) FinishedSources.Add(Source);
    CheckNightCompletion();
}

void ABorderGameMode::RegisterNightZombie(ABorderZombie* Zombie)
{
    if (!bIsNight || bGameOver || !IsValid(Zombie) || Zombie->bDead) return;
    NightZombies.Add(Zombie);
    Zombie->OnDied.AddUniqueDynamic(this,&ABorderGameMode::CheckNightCompletion);
    Zombie->OnDestroyed.AddUniqueDynamic(this,&ABorderGameMode::OnNightZombieDestroyed);
    RemainingZombies = NightZombies.Num();
    GetWorldTimerManager().ClearTimer(PhaseTimer);
}

void ABorderGameMode::OnNightZombieDestroyed(AActor* Actor) { CheckNightCompletion(); }
void ABorderGameMode::CheckNightCompletion()
{
    if (!bIsNight || bGameOver || bTransitioning) return;
    for (auto It=NightZombies.CreateIterator(); It; ++It)
        if (!It->IsValid() || It->Get()->IsActorBeingDestroyed() || It->Get()->bDead) It.RemoveCurrent();
    RemainingZombies = NightZombies.Num();
    if (bWaveRegistered && WaveSources.Num()==FinishedSources.Num() && RemainingZombies==0)
    {
        if (!GetWorldTimerManager().IsTimerActive(PhaseTimer))
            GetWorldTimerManager().SetTimer(PhaseTimer,this,&ABorderGameMode::StartDay,FMath::Max(.1f,DawnDelay),false);
    }
    else GetWorldTimerManager().ClearTimer(PhaseTimer);
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
        if (ABuilding* Building = Cast<ABuilding>(Actor)) Record.RestorationProgress = Building->RestorationProgress;
        if (UPlayerLoadoutComponent* Loadout = Actor->FindComponentByClass<UPlayerLoadoutComponent>())
        {
            Record.bHasLoadout = true;
            Record.LoadoutState = Loadout->State;
        }
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
    OnRunFailed.Broadcast();
    bTransitioning = false;
    if (Director) Director->ResetDay();
    GetWorldTimerManager().ClearTimer(PhaseTimer);
    if (APlayerController* Player = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (APawn* Pawn = Player->GetPawn())
            if (UPlayerLoadoutComponent* Loadout = Pawn->FindComponentByClass<UPlayerLoadoutComponent>()) Loadout->StopActions();
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
        // Static buildings never move during play; restoring their health must not
        // request a transform update (or change mobility and dirty navigation).
        if (USceneComponent* Root = Actor->GetRootComponent())
            if (Root->Mobility == EComponentMobility::Movable && !Actor->GetActorTransform().Equals(Record.Transform))
                Actor->SetActorTransform(Record.Transform, false, nullptr, ETeleportType::TeleportPhysics);
        Actor->Tags = Record.Tags;
        if (ABuilding* Building = Cast<ABuilding>(Actor)) Building->RestorationProgress = Record.RestorationProgress;
        if (Record.bHasLoadout)
            if (UPlayerLoadoutComponent* Loadout = Actor->FindComponentByClass<UPlayerLoadoutComponent>()) Loadout->RestoreState(Record.LoadoutState);
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
    bTransitioning = false;
    NightZombies.Reset(); WaveSources.Reset(); FinishedSources.Reset();
    RemainingZombies = 0;
    bWaveRegistered = false;
    BindDefeatConditions();
    ApplyPhase();
}
