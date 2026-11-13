#include "Zombies/Spawning/ZombieLineSpawner.h"
#include "Zombies/Data/ZombieWaveSettings.h"
#include "Zombies/BorderZombie.h"
#include "Core/BorderGameMode.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

AZombieLineSpawner::AZombieLineSpawner()
{
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
    SpawnedThisNight = GroupSpawned = Cursor = 0;
    Alive.Reset(); AliveFromThisSpawner = 0;
    Random.Initialize(Settings->RandomSeed ^ (Day*9781) ^ static_cast<int32>(GetTypeHash(GetFName())));
    const int32 SlotCount = FMath::Clamp(FMath::FloorToInt(Settings->LineLength/FMath::Max(70.f,Settings->SlotSpacing))+1,2,1024);
    Slots.Reset();
    for (int32 Index=0;Index<SlotCount;++Index) Slots.Add(Index);
    ShuffleSlots();
    Mode->RegisterWaveSource(this);
    bRunning = true;
    Schedule(.05f);
}
void AZombieLineSpawner::ShuffleSlots()
{
    for (int32 Index=Slots.Num()-1;Index>0;--Index) Slots.Swap(Index,Random.RandRange(0,Index));
    Cursor = 0;
}
void AZombieLineSpawner::Schedule(float Delay)
{
    GetWorldTimerManager().SetTimer(SpawnTimer,this,&AZombieLineSpawner::SpawnStep,FMath::Max(.05f,Delay),false);
}
bool AZombieLineSpawner::FindSpawnPoint(FVector& Position)
{
    if (Cursor>=Slots.Num()) ShuffleSlots();
    const float Width=FMath::Max(100.f,Settings->LineLength);
    const float T=static_cast<float>(Slots[Cursor++])/FMath::Max(1,Slots.Num()-1);
    const float Y=FMath::Clamp(FMath::Lerp(-Width*.5f,Width*.5f,T)+Random.FRandRange(-Settings->LateralJitter,Settings->LateralJitter),-Width*.5f,Width*.5f);
    FVector Point=GetActorTransform().TransformPosition(FVector(Random.FRandRange(-Settings->DepthJitter,Settings->DepthJitter),Y,0));
    FHitResult Ground;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieSpawn),false,this);
    FCollisionObjectQueryParams GroundObjects;
    GroundObjects.AddObjectTypesToQuery(ECC_WorldStatic);
    if (!GetWorld()->LineTraceSingleByObjectType(Ground,Point+FVector(0,0,Settings->GroundSearchUp),
        Point-FVector(0,0,Settings->GroundSearchDown),GroundObjects,Params) || Ground.ImpactNormal.Z<Settings->MinGroundNormalZ) return false;
    const ABorderZombie* Default=ZombieClass->GetDefaultObject<ABorderZombie>();
    const float Radius=Default->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float Height=Default->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    Position=Ground.ImpactPoint+FVector(0,0,Height+5.f);
    FCollisionObjectQueryParams Obstacles;
    Obstacles.AddObjectTypesToQuery(ECC_Pawn);
    Obstacles.AddObjectTypesToQuery(ECC_PhysicsBody);
    Obstacles.AddObjectTypesToQuery(ECC_WorldStatic);
    Obstacles.AddObjectTypesToQuery(ECC_WorldDynamic);
    return !GetWorld()->OverlapAnyTestByObjectType(Position,FQuat::Identity,Obstacles,FCollisionShape::MakeCapsule(Radius,Height),Params);
}
void AZombieLineSpawner::SpawnStep()
{
    if (!bRunning || !Mode.IsValid() || !Mode->bIsNight || Mode->bGameOver || Mode->bTransitioning || !bEnabled)
    {
        StopSpawning(); return;
    }
    if (SpawnedThisNight>=FMath::Max(0,Settings->TotalZombies))
    {
        StopSpawning();
        Mode->ReportSpawningFinished(this);
        LastSpawnIssue.Reset();
        return;
    }
    if (Mode->RemainingZombies>=FMath::Max(1,Settings->MaxAlive))
    {
        LastSpawnIssue=TEXT("Alive limit reached; waiting.");
        Schedule(Settings->RetryInterval); return;
    }
    ABorderZombie* Zombie=nullptr;
    for (int32 Attempt=0;Attempt<FMath::Clamp(Settings->AttemptsPerStep,1,32);++Attempt)
    {
        FVector Position;
        if (!FindSpawnPoint(Position)) continue;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
        Zombie=GetWorld()->SpawnActor<ABorderZombie>(ZombieClass,Position,GetActorRotation(),Params);
        if (Zombie) break;
    }
    if (!Zombie)
    {
        LastSpawnIssue=TEXT("No clear ground slot; retrying.");
        Schedule(Settings->RetryInterval); return;
    }
    ++SpawnedThisNight; ++GroupSpawned;
    Alive.Add(Zombie);
    AliveFromThisSpawner=Alive.Num();
    Mode->RegisterNightZombie(Zombie);
    Zombie->OnDied.AddUniqueDynamic(this,&AZombieLineSpawner::UpdateAliveCount);
    Zombie->OnDestroyed.AddUniqueDynamic(this,&AZombieLineSpawner::HandleZombieDestroyed);
    LastSpawnIssue.Reset();
    const bool bGroupDone=GroupSpawned>=FMath::Max(1,Settings->GroupSize);
    if (bGroupDone) GroupSpawned=0;
    Schedule(SpawnedThisNight>=Settings->TotalZombies ? .05f : bGroupDone ? Settings->GroupInterval : Settings->SpawnInterval);
}
void AZombieLineSpawner::UpdateAliveCount()
{
    for (auto It=Alive.CreateIterator();It;++It)
        if (!It->IsValid() || It->Get()->IsActorBeingDestroyed() || It->Get()->bDead) It.RemoveCurrent();
    AliveFromThisSpawner=Alive.Num();
}
void AZombieLineSpawner::HandleZombieDestroyed(AActor* Actor) { UpdateAliveCount(); }
