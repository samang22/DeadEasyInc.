#include "Core/BorderCheckpointComponent.h"
#include "Buildings/Building.h"
#include "Buildings/Heating/CentralBoiler.h"
#include "Progression/PlayerLoadoutComponent.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Interaction/BuildingRepairComponent.h"
#include "Zombies/BorderZombie.h"
#include "Scavenging/CorpseHandlingComponent.h"
#include "Scavenging/Handcart.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"

UBorderCheckpointComponent::UBorderCheckpointComponent() { PrimaryComponentTick.bCanEverTick=false; }
bool UBorderCheckpointComponent::IsCheckpointActor(AActor* Actor) const
{
    return Actor == UGameplayStatics::GetPlayerPawn(this, 0) || Actor->IsA<ABuilding>() || Actor->IsA<ABorderZombie>() || Actor->IsA<AHandcart>();
}

void UBorderCheckpointComponent::Capture(int32 Day)
{
    if (auto* Pawn=UGameplayStatics::GetPlayerPawn(this,0))
        if (auto* Work=Pawn->FindComponentByClass<UCorpseHandlingComponent>()) Work->Release();
    Dawn.Reset();
    CheckpointDay = Day;
    if (APlayerController* Player = UGameplayStatics::GetPlayerController(this, 0)) DawnView = Player->GetControlRotation();
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsCheckpointActor(Actor) || Actor->IsActorBeingDestroyed()) continue;
        FBorderCheckpointActor& Record = Dawn.AddDefaulted_GetRef();
        Record.Original = Actor;
        Record.ActorClass = Actor->GetClass();
        Record.Transform = Actor->GetActorTransform();
        Record.Tags = Actor->Tags;
        if (ABuilding* Building = Cast<ABuilding>(Actor)) { Record.RestorationProgress = Building->RestorationProgress;Record.UpgradeLevel=Building->UpgradeLevel; }
        if (auto* Boiler=Cast<ACentralBoiler>(Actor)) Record.Fuel=Boiler->Fuel;
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
            Record.WallTarget = Zombie->WallTarget;Record.TransportSlot=Zombie->TransportSlot;Record.TransportCart=Zombie->Carrier.IsValid() ? Zombie->Carrier->GetFName() : NAME_None;
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

int32 UBorderCheckpointComponent::Restore()
{
    if (Dawn.IsEmpty()) return CheckpointDay;
    if (auto* Pawn=UGameplayStatics::GetPlayerPawn(this,0))
        if (auto* Work=Pawn->FindComponentByClass<UCorpseHandlingComponent>()) { Work->Release();Work->ReleaseCargo(); }
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
        if (ABuilding* Building = Cast<ABuilding>(Actor)) { Building->RestorationProgress = Record.RestorationProgress;Building->RestoreUpgrade(Record.UpgradeLevel); }
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
        if (ACentralBoiler* Boiler = Cast<ACentralBoiler>(Actor)) { Boiler->bDestroyed = Record.Health <= 0.f;Boiler->Fuel=Record.Fuel; }
        if (ABorderZombie* Zombie = Cast<ABorderZombie>(Actor))
        {
            Zombie->WallTarget = Record.WallTarget;Zombie->TransportSlot=Record.TransportSlot;
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
    for (TActorIterator<AHandcart> It(GetWorld());It;++It) It->RefreshCargo();
    if (APlayerController* Player = UGameplayStatics::GetPlayerController(this, 0))
    {
        Player->ResetIgnoreMoveInput();
        Player->ResetIgnoreLookInput();
        Player->SetControlRotation(DawnView);
    }
    return CheckpointDay;
}
