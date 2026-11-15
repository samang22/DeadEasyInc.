#include "Core/BorderGameMode.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Core/DayNight/DayNightDirector.h"
#include "Buildings/Defense/BorderGate.h"
#include "Progression/PlayerLoadoutComponent.h"
#include "Core/Data/BorderRulesSettings.h"
#include "Buildings/Heating/CentralBoiler.h"
#include "Buildings/Building.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Interaction/BuildingRepairComponent.h"
#include "Zombies/BorderZombie.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Scavenging/CorpseHandlingComponent.h"

ABorderGameMode::ABorderGameMode()
{
    Checkpoint=CreateDefaultSubobject<UBorderCheckpointComponent>(TEXT("Checkpoint"));
}

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
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        // Disk restoration belongs only to initial startup, never a normal dawn transition.
        if (APawn* Player = UGameplayStatics::GetPlayerPawn(this,0))
            if (UPlayerLoadoutComponent* Loadout = Player->FindComponentByClass<UPlayerLoadoutComponent>()) Loadout->LoadDawn(DayNumber);
        StartDay();
    }));
}

void ABorderGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearAllTimersForObject(this);
    Super::EndPlay(Reason);
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
#if !UE_BUILD_SHIPPING
    TArray<TWeakObjectPtr<ABorderZombie>> Corpses;
    for (TActorIterator<ABorderZombie> It(GetWorld()); It; ++It)
        if (It->bDead) Corpses.Add(*It);
    auto TraceCorpses = [Corpses](const TCHAR* Stage)
    {
        int32 Present=0, Hidden=0, Simulating=0;
        for (const auto& Entry : Corpses)
            if (const auto* Corpse=Entry.Get(); IsValid(Corpse) && !Corpse->IsActorBeingDestroyed())
            {
                ++Present;
                const auto* Mesh=Corpse->GetMesh();
                Hidden += Corpse->IsHidden() || !Mesh->IsVisible() || Mesh->bHiddenInGame;
                Simulating += Mesh->IsSimulatingPhysics();
            }
        UE_LOG(LogTemp, Warning, TEXT("DawnCorpse [%s] tracked=%d present=%d hidden=%d simulating=%d"),
            Stage, Corpses.Num(), Present, Hidden, Simulating);
        if (!Corpses.IsEmpty())
            if (const auto* Corpse=Corpses[0].Get(); IsValid(Corpse))
            {
                const auto* Mesh=Corpse->GetMesh();
                const auto* Body=Mesh->GetBodyInstance(Corpse->CorpsePushBone);
                UE_LOG(LogTemp, Warning, TEXT("DawnCorpse [%s] sample=%s root=%s body=%s bounds=%s"),
                    Stage, *Corpse->GetName(), *Corpse->GetActorLocation().ToString(),
                    *(Body ? Body->GetUnrealWorldTransform().GetLocation() : Mesh->GetComponentLocation()).ToString(),
                    *Mesh->Bounds.Origin.ToString());
            }
    };
    TraceCorpses(TEXT("before"));
#endif
    if (bIsNight) ++DayNumber;
    // Keep all unprocessed ragdolls across dawn, including any with an external cleanup timer.
    for (TActorIterator<ABorderZombie> It(GetWorld()); It; ++It)
        if (It->bDead) It->SetLifeSpan(0.f);
    bIsNight = false;
    bWaveRegistered = false;
    RemainingZombies = 0;
    NightZombies.Reset();
    WaveSources.Reset();
    FinishedSources.Reset();
    BindDefeatConditions();
    ApplyPhase();
#if !UE_BUILD_SHIPPING
    TraceCorpses(TEXT("after phase"));
#endif
    Checkpoint->Capture(DayNumber);
#if !UE_BUILD_SHIPPING
    TraceCorpses(TEXT("after capture"));
#endif
    if (APawn* Player = UGameplayStatics::GetPlayerPawn(this,0))
        if (UPlayerLoadoutComponent* Loadout = Player->FindComponentByClass<UPlayerLoadoutComponent>()) Loadout->SaveDawn(DayNumber);
#if !UE_BUILD_SHIPPING
    TraceCorpses(TEXT("after save"));
    FTimerHandle ProbeTimer;
    GetWorldTimerManager().SetTimer(ProbeTimer, FTimerDelegate::CreateWeakLambda(this, [TraceCorpses]()
    {
        TraceCorpses(TEXT("one second after dawn"));
    }), 1.f, false);
#endif
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
    if (!bGameOver || !Checkpoint->HasSnapshot()) return;
    GetWorldTimerManager().ClearTimer(RetryTimer);
    DayNumber = Checkpoint->Restore();
    bIsNight = false;
    bGameOver = false;
    bTransitioning = false;
    NightZombies.Reset(); WaveSources.Reset(); FinishedSources.Reset();
    RemainingZombies = 0;
    bWaveRegistered = false;
    BindDefeatConditions();
    ApplyPhase();
}
