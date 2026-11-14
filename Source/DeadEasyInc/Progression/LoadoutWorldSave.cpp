#include "Progression/PlayerLoadoutComponent.h"
#include "Buildings/Building.h"
#include "Buildings/Heating/CentralBoiler.h"
#include "Zombies/BorderZombie.h"
#include "Scavenging/CorpseHandlingComponent.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Scavenging/Handcart.h"

void UPlayerLoadoutComponent::CaptureWorldSave(ULoadoutSave* Save) const
{
    Save->bHasWorld=true;
    for (TActorIterator<AActor> It(GetWorld());It;++It)
    {
        auto* Building=Cast<ABuilding>(*It);auto* Zombie=Cast<ABorderZombie>(*It);
        auto* Cart=Cast<AHandcart>(*It);
        if ((!Building && !Zombie && !Cart) || It->IsActorBeingDestroyed()) continue;
        auto& Record=Save->World.AddDefaulted_GetRef();Record.ActorName=It->GetFName();Record.ActorClass=It->GetClass();Record.Transform=It->GetActorTransform();Record.bHandcart=Cart!=nullptr;
        if (Building)
        {
            Record.Level=Building->UpgradeLevel;Record.Restoration=Building->RestorationProgress;
            Record.Health=Building->CombatHealth->Health;Record.MaxHealth=Building->CombatHealth->MaxHealth;
            if (auto* Boiler=Cast<ACentralBoiler>(Building)) Record.Fuel=Boiler->Fuel;
        }
        if (Zombie)
        {
            Record.bZombie=true;Record.bCorpse=Zombie->bDead;Record.Health=Zombie->Health;Record.TransportSlot=Zombie->TransportSlot;Record.TransportCart=Zombie->Carrier.IsValid() ? Zombie->Carrier->GetFName() : NAME_None;
            if (Zombie->bDead)
            {
                TArray<FName> Bones;Zombie->GetMesh()->GetBoneNames(Bones);
                for (FName Bone : Bones) if (auto* Body=Zombie->GetMesh()->GetBodyInstance(Bone))
                    if (Body->IsInstanceSimulatingPhysics()) Record.Bodies.Add(Bone,Body->GetUnrealWorldTransform());
            }
        }
    }
}
void UPlayerLoadoutComponent::RestoreWorldSave(const ULoadoutSave* Save)
{
    if (!Save->bHasWorld) return; // Version-1 progression-only saves remain compatible.
    if (auto* Work=GetOwner()->FindComponentByClass<UCorpseHandlingComponent>()) { Work->Release();Work->ReleaseCargo(); }
    TArray<ABorderZombie*> Zombies;for (TActorIterator<ABorderZombie> It(GetWorld());It;++It) Zombies.Add(*It);
    for (auto* Zombie : Zombies) Zombie->Destroy();
    // Restore cart positions before restoring the corpses that belong to them.
    for (const auto& Record : Save->World) if (Record.bHandcart)
    {
        AHandcart* Cart=nullptr;
        for (TActorIterator<AHandcart> It(GetWorld());It;++It) if (It->GetFName()==Record.ActorName) { Cart=*It;break; }
        if (!Cart)
        {
            UClass* Class=Record.ActorClass.LoadSynchronous();if (!Class || !Class->IsChildOf(AHandcart::StaticClass())) continue;
            FActorSpawnParameters Params;Params.Name=Record.ActorName;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Cart=GetWorld()->SpawnActor<AHandcart>(Class,Record.Transform,Params);
        }
        if (Cart) Cart->SetActorTransform(Record.Transform,false,nullptr,ETeleportType::TeleportPhysics);
    }
    for (const auto& Record : Save->World)
    {
        if (Record.bHandcart) continue;
        if (!Record.bZombie)
        {
            for (TActorIterator<ABuilding> It(GetWorld());It;++It) if (It->GetFName()==Record.ActorName)
            {
                It->RestoreUpgrade(Record.Level);It->RestorationProgress=Record.Restoration;
                It->CombatHealth->MaxHealth=FMath::Max(1.f,Record.MaxHealth);It->CombatHealth->Health=FMath::Clamp(Record.Health,0.f,It->CombatHealth->MaxHealth);
                It->CombatHealth->OnHealthChanged.Broadcast(It->CombatHealth->Health,It->CombatHealth->MaxHealth);
                if (auto* Boiler=Cast<ACentralBoiler>(*It)) { Boiler->Fuel=FMath::Clamp(Record.Fuel,0.f,Boiler->Capacity());Boiler->bDestroyed=Record.Health<=0; }
                break;
            }
            continue;
        }
        UClass* Class=Record.ActorClass.LoadSynchronous();if (!Class || !Class->IsChildOf(ABorderZombie::StaticClass())) continue;
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Zombie=GetWorld()->SpawnActor<ABorderZombie>(Class,Record.Transform,Params);if (!Zombie) continue;
        Zombie->Health=Record.bCorpse ? 1.f : Record.Health;
        if (Record.bCorpse)
        {
            UGameplayStatics::ApplyDamage(Zombie,1.f,nullptr,nullptr,nullptr);
            for (const auto& Pair : Record.Bodies) if (auto* Body=Zombie->GetMesh()->GetBodyInstance(Pair.Key)) Body->SetBodyTransform(Pair.Value,ETeleportType::TeleportPhysics);
            Zombie->GetMesh()->SetAllPhysicsLinearVelocity(FVector::ZeroVector);Zombie->GetMesh()->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

        }
    }
    for (TActorIterator<AHandcart> It(GetWorld());It;++It) It->RefreshCargo();
}
