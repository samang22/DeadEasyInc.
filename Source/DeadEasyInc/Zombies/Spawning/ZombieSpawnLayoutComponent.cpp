#include "Zombies/Spawning/ZombieSpawnLayoutComponent.h"
#include "Zombies/Data/ZombieWaveSettings.h"
#include "Zombies/BorderZombie.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

UZombieSpawnLayoutComponent::UZombieSpawnLayoutComponent() { PrimaryComponentTick.bCanEverTick=false; }
void UZombieSpawnLayoutComponent::Prepare(const UZombieWaveSettings* Settings,int32 Seed)
{
    Random.Initialize(Seed);
    const int32 SlotCount=FMath::Clamp(FMath::FloorToInt(Settings->LineLength/FMath::Max(70.f,Settings->SlotSpacing))+1,2,1024);
    Slots.Reset();
    for (int32 Index=0;Index<SlotCount;++Index) Slots.Add(Index);
    ShuffleSlots();
}
void UZombieSpawnLayoutComponent::ShuffleSlots()
{
    for (int32 Index=Slots.Num()-1;Index>0;--Index) Slots.Swap(Index,Random.RandRange(0,Index));
    Cursor = 0;
}
bool UZombieSpawnLayoutComponent::FindSpawnPoint(const FTransform& Transform,const UZombieWaveSettings* Settings,TSubclassOf<ABorderZombie> ZombieClass,FVector& Position)
{
    if (Cursor>=Slots.Num()) ShuffleSlots();
    const float Width=FMath::Max(100.f,Settings->LineLength);
    const float T=static_cast<float>(Slots[Cursor++])/FMath::Max(1,Slots.Num()-1);
    const float Y=FMath::Clamp(FMath::Lerp(-Width*.5f,Width*.5f,T)+Random.FRandRange(-Settings->LateralJitter,Settings->LateralJitter),-Width*.5f,Width*.5f);
    FVector Point=Transform.TransformPosition(FVector(Random.FRandRange(-Settings->DepthJitter,Settings->DepthJitter),Y,0));
    FHitResult Ground;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieSpawn),false,GetOwner());
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
