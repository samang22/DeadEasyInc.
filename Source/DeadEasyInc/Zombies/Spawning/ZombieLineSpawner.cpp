#include "Zombies/Spawning/ZombieLineSpawner.h"
#include "Zombies/Spawning/ZombieSpawnLayoutComponent.h"
#include "Zombies/Data/ZombieWaveSettings.h"
#include "Zombies/BorderZombie.h"
#include "Core/BorderGameMode.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

AZombieLineSpawner::AZombieLineSpawner()
{
    SpawnLayout=CreateDefaultSubobject<UZombieSpawnLayoutComponent>(TEXT("SpawnLayout"));
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    LinePreview = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnLine"));
    LinePreview->SetupAttachment(RootComponent);
    LinePreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LinePreview->SetCanEverAffectNavigation(false);
    LinePreview->SetHiddenInGame(true);
    LinePreview->ShapeColor = FColor::Green;
    DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnDirection"));
    DirectionArrow->SetupAttachment(RootComponent);
    DirectionArrow->ArrowColor = FColor::Green;
    DirectionArrow->ArrowSize = 3.f;
    DirectionArrow->SetHiddenInGame(true);
}
void AZombieLineSpawner::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    LinePreview->SetBoxExtent(FVector(Settings ? Settings->DepthJitter : 80.f,Settings ? Settings->LineLength*.5f : 2000.f,10));
}
void AZombieLineSpawner::BeginPlay()
{
    Super::BeginPlay();
    Mode = GetWorld()->GetAuthGameMode<ABorderGameMode>();
    if (!Mode.IsValid()) return;
    Mode->OnPhaseStateChanged.AddDynamic(this,&AZombieLineSpawner::HandlePhase);
    Mode->OnRunFailed.AddDynamic(this,&AZombieLineSpawner::StopSpawning);
    if (Mode->bIsNight && !Mode->bGameOver && !Mode->bTransitioning) StartWave(Mode->DayNumber);
}
void AZombieLineSpawner::EndPlay(const EEndPlayReason::Type Reason)
{
    StopSpawning();
    if (Mode.IsValid())
    {
        Mode->OnPhaseStateChanged.RemoveAll(this);
        Mode->OnRunFailed.RemoveAll(this);
    }
    Super::EndPlay(Reason);
}
void AZombieLineSpawner::HandlePhase(bool IsNight,int32 Day)
{
    if (IsNight) StartWave(Day);
    else
    {
        StopSpawning();
        StartedDay = INDEX_NONE;
        Alive.Reset();
        AliveFromThisSpawner = 0;
    }
}
void AZombieLineSpawner::StopSpawning()
{
    bRunning = false;
    GetWorldTimerManager().ClearTimer(SpawnTimer);
}
void AZombieLineSpawner::StartWave(int32 Day)
{
    if (!bEnabled || StartedDay==Day || !Mode.IsValid() || Mode->bGameOver || Mode->bTransitioning) return;
    if (!Settings || !ZombieClass || ZombieClass->HasAnyClassFlags(CLASS_Abstract))
    {
        LastSpawnIssue = TEXT("Assign wave settings and a concrete zombie Blueprint.");
        UE_LOG(LogTemp,Error,TEXT("%s: %s"),*GetName(),*LastSpawnIssue);
        return;
    }
    StartedDay = Day;
    SpawnedThisNight = GroupSpawned = 0;
    Alive.Reset(); AliveFromThisSpawner = 0;
    SpawnLayout->Prepare(Settings,Settings->RandomSeed ^ (Day*9781) ^ static_cast<int32>(GetTypeHash(GetFName())));
    Mode->RegisterWaveSource(this);
    bRunning = true;
    Schedule(.05f);
}
void AZombieLineSpawner::Schedule(float Delay)
{
    GetWorldTimerManager().SetTimer(SpawnTimer,this,&AZombieLineSpawner::SpawnStep,FMath::Max(.05f,Delay),false);
}

void AZombieLineSpawner::SpawnStep()
{
    if (!bRunning || !Mode.IsValid() || !Mode->bIsNight ||
        Mode->bGameOver || Mode->bTransitioning || !bEnabled)
    {
        StopSpawning();
        return;
    }

    // 일차별 강화 배율 (1일차 1.0, 2일차 1.1, 3일차 1.2 ...)
    const float ReinforcementMultiplier =
        1.0f +
        FMath::Max(0, StartedDay - 1) *
        FMath::Max(0.0f, Settings->ReinforcementCoefficient);

    // 일차별 총 스폰 수
    const int32 ReinforcedTotalZombies = FMath::Max(
        0,
        FMath::RoundToInt(
            Settings->TotalZombies * ReinforcementMultiplier
        )
    );

    // 일차별 동시 생존 제한
    const int32 ReinforcedMaxAlive = FMath::Max(
        1,
        FMath::RoundToInt(
            Settings->MaxAlive * ReinforcementMultiplier
        )
    );

    // 목표 수량에 도달하면 생성 종료
    if (SpawnedThisNight >= ReinforcedTotalZombies)
    {
        StopSpawning();
        Mode->ReportSpawningFinished(this);
        LastSpawnIssue.Reset();
        return;
    }

    // 동시 생존 제한 확인
    if (Mode->RemainingZombies >= ReinforcedMaxAlive)
    {
        LastSpawnIssue = TEXT("Alive limit reached; waiting.");
        Schedule(Settings->RetryInterval);
        return;
    }

    ABorderZombie* Zombie = nullptr;

    for (int32 Attempt = 0;
         Attempt < FMath::Clamp(Settings->AttemptsPerStep, 1, 32);
         ++Attempt)
    {
        FVector Position;

        if (!SpawnLayout->FindSpawnPoint(
                GetActorTransform(),
                Settings,
                ZombieClass,
                Position))
        {
            continue;
        }

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;

        Zombie = GetWorld()->SpawnActor<ABorderZombie>(
            ZombieClass,
            Position,
            GetActorRotation(),
            Params
        );

        if (Zombie)
        {
            // 개별 좀비 스탯 강화
            Zombie->ApplyDayReinforcement(ReinforcementMultiplier);
            break;
        }
    }

    if (!Zombie)
    {
        LastSpawnIssue = TEXT("No clear ground slot; retrying.");
        Schedule(Settings->RetryInterval);
        return;
    }

    // 생성 수량 갱신
    ++SpawnedThisNight;
    ++GroupSpawned;

    // 생존 좀비 관리
    Alive.Add(Zombie);
    AliveFromThisSpawner = Alive.Num();

    Mode->RegisterNightZombie(Zombie);

    Zombie->OnDied.AddUniqueDynamic(
        this,
        &AZombieLineSpawner::UpdateAliveCount
    );

    Zombie->OnDestroyed.AddUniqueDynamic(
        this,
        &AZombieLineSpawner::HandleZombieDestroyed
    );

    LastSpawnIssue.Reset();

    // 그룹 생성 완료 여부
    const bool bGroupDone =
        GroupSpawned >= FMath::Max(1, Settings->GroupSize);

    if (bGroupDone)
    {
        GroupSpawned = 0;
    }

    // 다음 좀비 생성 예약
    Schedule(
        SpawnedThisNight >= ReinforcedTotalZombies
            ? .05f
            : bGroupDone
                ? Settings->GroupInterval
                : Settings->SpawnInterval
    );
}

void AZombieLineSpawner::UpdateAliveCount()
{
    for (auto It=Alive.CreateIterator();It;++It)
        if (!It->IsValid() || It->Get()->IsActorBeingDestroyed() || It->Get()->bDead) It.RemoveCurrent();
    AliveFromThisSpawner=Alive.Num();
}
void AZombieLineSpawner::HandleZombieDestroyed(AActor* Actor) { UpdateAliveCount(); }
