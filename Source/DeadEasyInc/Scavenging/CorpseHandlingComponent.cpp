#include "Scavenging/CorpseHandlingComponent.h"
#include "Scavenging/CorpseProcessor.h"
#include "Scavenging/WorkEquipment.h"
#include "Progression/PlayerLoadoutComponent.h"
#include "Buildings/Building.h"
#include "Core/BorderGameMode.h"
#include "Zombies/BorderZombie.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Scavenging/Handcart.h"
#include "Scavenging/ElectricCart.h" // Legacy GetMountedVehicle API only.
#include "Scavenging/UsableEquipment.h"
#include "Engine/OverlapResult.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Interaction/GateSwitch.h"

UCorpseHandlingComponent::UCorpseHandlingComponent() { PrimaryComponentTick.bCanEverTick=true; }
UPlayerLoadoutComponent* UCorpseHandlingComponent::Loadout() const { return GetOwner()->FindComponentByClass<UPlayerLoadoutComponent>(); }
void UCorpseHandlingComponent::BeginPlay()
{
    Super::BeginPlay();
    Handle=NewObject<UPhysicsHandleComponent>(GetOwner());Handle->RegisterComponent();
    Handle->InterpolationSpeed=12.f;Handle->LinearStiffness=1800.f;Handle->LinearDamping=350.f;
    if (auto* Character=Cast<ACharacter>(GetOwner())) NormalSpeed=Character->GetCharacterMovement()->MaxWalkSpeed;
    if (Settings && Settings->EquipmentClass)
    {
        FActorSpawnParameters Params;Params.Owner=GetOwner();Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Equipment=GetWorld()->SpawnActor<AWorkEquipment>(Settings->EquipmentClass,GetOwner()->GetActorTransform(),Params);
        if (Equipment)
        {
            Equipment->AttachToActor(GetOwner(),FAttachmentTransformRules::SnapToTargetNotIncludingScale);
            if (auto* Camera=GetOwner()->FindComponentByClass<UCameraComponent>())
            {
                Equipment->Fork->AttachToComponent(Camera,FAttachmentTransformRules::KeepRelativeTransform);
                Equipment->Hammer->AttachToComponent(Camera,FAttachmentTransformRules::KeepRelativeTransform);
            }
        }
    }
    RefreshVisuals();
}
void UCorpseHandlingComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    Release();EndEquipmentUse(true);if (Equipment) Equipment->Destroy();if (Handle) Handle->DestroyComponent();Super::EndPlay(Reason);
}
bool UCorpseHandlingComponent::CanWork() const
{
    const auto* Mode=GetWorld()->GetAuthGameMode<ABorderGameMode>();
    return Loadout() && !Loadout()->bMenuOpen && !GetOwner()->ActorHasTag(TEXT("Dead")) &&
        (!Mode || (!Mode->bIsNight && !Mode->bGameOver && !Mode->bTransitioning));
}
int32 UCorpseHandlingComponent::Capacity() const
{
    return Settings ? Settings->CartCapacity : 12;
}
void UCorpseHandlingComponent::SetWorkActive(bool Active)
{
    if (!EndEquipmentUse())
    { if (Loadout()) Loadout()->Notify(TEXT("하차할 공간이 없습니다."),true);return; }
    Release();bActive=Active;RefreshVisuals();
}
void UCorpseHandlingComponent::Select() { SetWorkActive(true); }
void UCorpseHandlingComponent::Cycle(float Direction)
{
    TArray<EWorkTool> Choices={EWorkTool::Fork,EWorkTool::Repair};

    const int32 Index=Choices.IndexOfByKey(Tool);
    Release();Tool=Choices[(FMath::Max(0,Index)+(Direction>0 ? 1 : Choices.Num()-1))%Choices.Num()];SetWorkActive(true);
}
AActor* UCorpseHandlingComponent::Focus(FHitResult& Hit) const
{
    const auto* Pawn=Cast<APawn>(GetOwner());const auto* Player=Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!Player) return nullptr;
    FVector Eye;FRotator View;Player->GetPlayerViewPoint(Eye,View);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CorpseUse),false,GetOwner());
    if (Equipment) Params.AddIgnoredActor(Equipment);
    if (Held) Params.AddIgnoredActor(Held);
    const float Reach=Settings ? Settings->Reach : 350.f;
    if (!GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+View.Vector()*Reach,ECC_Visibility,Params))
        GetWorld()->SweepSingleByChannel(Hit,Eye,Eye+View.Vector()*Reach,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(28.f),Params);
    return Hit.GetActor();
}
void UCorpseHandlingComponent::Primary()
{
    if (auto* Use=GetUsedEquipment())
    { if (CanWork()) Use->ExecuteSecondaryAction();return; }
    if (!bActive || !CanWork() || Tool==EWorkTool::Repair || GetWorld()->GetTimeSeconds()<NextUseTime) return;
    NextUseTime=GetWorld()->GetTimeSeconds()+.2f;
    if (Held) { Release();return; }
    FHitResult Hit;auto* Corpse=Cast<ABorderZombie>(Focus(Hit));
    if (!Corpse || !Corpse->bDead || Corpse->IsActorBeingDestroyed() || Corpse->ProcessingOwner.IsValid() || Corpse->IntakeOwner.IsValid()) return;
    Corpse->HandlingOwner=this;
    if (Corpse->Carrier.IsValid()) Corpse->Carrier->DetachCorpse(Corpse);
    Held=Corpse;
    FName Bone=Settings ? Settings->GrabBone : FName(TEXT("pelvis"));
    if (Corpse->GetMesh()->GetBoneIndex(Bone)==INDEX_NONE) Bone=Corpse->CorpsePushBone;
    if (!Corpse->GetMesh()->GetBodyInstance(Bone)) Bone=Hit.BoneName;
    Corpse->GetMesh()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
    Handle->GrabComponentAtLocation(Corpse->GetMesh(),Bone,Corpse->GetMesh()->GetSocketLocation(Bone));
    if (!Handle->GetGrabbedComponent()) Release();
}
void UCorpseHandlingComponent::Release()
{
    if (Handle) Handle->ReleaseComponent();

    if (IsValid(Held) && Held->HandlingOwner.Get()==this) Held->HandlingOwner.Reset();
    Held=nullptr;
}
AHandcart* UCorpseHandlingComponent::EnsureHandcart()
{
    if (IsValid(Handcart)) return Handcart;
    for (TActorIterator<AHandcart> It(GetWorld());It;++It) if (!It->IsActorBeingDestroyed()) { Handcart=*It;return Handcart; }
    if (!Settings || !Settings->HandcartClass || !Loadout() || !Loadout()->State.OwnedTools.Contains(TEXT("Handcart"))) return nullptr;
    FActorSpawnParameters Params;Params.Name=TEXT("StarterHandcart");Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    const FVector Position=GetOwner()->GetActorLocation()+GetOwner()->GetActorRightVector()*250.f+GetOwner()->GetActorForwardVector()*100.f;
    Handcart=GetWorld()->SpawnActor<AHandcart>(Settings->HandcartClass,Position,GetOwner()->GetActorRotation(),Params);
    if (Handcart) Handcart->SnapToGround();return Handcart;
}
IUsableEquipment* UCorpseHandlingComponent::GetUsedEquipment() const
{
    AActor* Actor=CurrentEquipment.Get();
    if (!IsValid(Actor) || Actor->IsActorBeingDestroyed()) return nullptr;
    auto* Use=Cast<IUsableEquipment>(Actor);
    return Use && Use->IsUsedBy(Cast<APawn>(GetOwner())) ? Use : nullptr;
}
bool UCorpseHandlingComponent::IsPushing() const { return GetUsedEquipment()!=nullptr; }
AElectricCart* UCorpseHandlingComponent::GetMountedVehicle() const
{
    return IsPushing() ? Cast<AElectricCart>(CurrentEquipment.Get()) : nullptr;
}
bool UCorpseHandlingComponent::EndEquipmentUse(bool bForce)
{
    if (auto* Use=GetUsedEquipment())
        if (!Use->EndUse(Cast<APawn>(GetOwner()),bForce)) return false;
    CurrentEquipment.Reset();
    if (!GetOwner()->IsActorBeingDestroyed() && GetWorld() && !GetWorld()->bIsTearingDown)
        if (Loadout()) Loadout()->RefreshWeapon();
    return true;
}
void UCorpseHandlingComponent::StopPushing() { EndEquipmentUse(true); }
void UCorpseHandlingComponent::Store()
{
    if (!CanWork() || !Loadout()) return;
    if (IsPushing())
    {
        if (!GetUsedEquipment()->ExecutePrimaryAction()) Loadout()->Notify(TEXT("장비를 멈춘 뒤 다시 시도해 주세요."),true);
        return;
    }
    Loadout()->Notify(TEXT("시체를 들어 카트 위에 좌클릭으로 놓아주세요. 적재는 자동 감지됩니다."));
}
int32 UCorpseHandlingComponent::CargoCount() const { return IsValid(Handcart) ? Handcart->CargoCount() : 0; }
bool UCorpseHandlingComponent::AdoptCorpse(ABorderZombie* Corpse,int32 Slot)
{
    AHandcart* Cart=EnsureHandcart();return Cart && Cart->LoadCorpse(Corpse,Slot);
}
void UCorpseHandlingComponent::DetachCorpse(ABorderZombie* Corpse)
{
    if (!Corpse) return;if (Held==Corpse) Release();
    if (Corpse->Carrier.IsValid()) Corpse->Carrier->DetachCorpse(Corpse);
}
void UCorpseHandlingComponent::ReleaseCargo()
{
    for (TActorIterator<AHandcart> It(GetWorld());It;++It) It->ReleaseCargo();
}
void UCorpseHandlingComponent::RefreshVisuals()
{
    if (Equipment) Equipment->Refresh(Tool,bActive && !IsPushing(),false);
    EnsureHandcart();
}
bool UCorpseHandlingComponent::Interact()
{
    if (IsPushing())
    {
        if (!EndEquipmentUse()) { if (Loadout()) Loadout()->Notify(TEXT("하차할 공간이 없습니다."),true); }
        else RefreshVisuals();
        return true;
    }
    if (!CanWork()) return false;
    if (AActor* Target=GetUsableEquipment())
    {
        if (Held) { Loadout()->Notify(TEXT("시체를 먼저 내려놓아 주세요."),true);return true; }
        auto* Use=Cast<IUsableEquipment>(Target);
        if (!Use || !Use->BeginUse(Cast<APawn>(GetOwner())))
        { Loadout()->Notify(TEXT("장비 접근 위치에서 다시 시도해 주세요."),true);return true; }
        CurrentEquipment=Target;
        if (auto* Cart=Cast<AHandcart>(Target)) Handcart=Cart; // Cargo API compatibility, not use state.
        bActive=false;RefreshVisuals();Loadout()->RefreshWeapon();return true;
    }
    FHitResult Hit;
    if (!Cast<ACorpseProcessor>(Focus(Hit))) return false;
    Loadout()->Notify(TEXT("시체를 집어 투입구에 넣거나, 수레를 잡고 앞에서 우클릭해 비우세요."));return true;
}
AHandcart* UCorpseHandlingComponent::GetUsableHandcart() const
{
    return Cast<AHandcart>(GetUsableEquipment());
}
AActor* UCorpseHandlingComponent::GetUsableEquipment() const
{
    if (!CanWork() || IsPushing()) return nullptr;
    auto* Pawn=Cast<APawn>(GetOwner());
    const auto* Player=Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!Player) return nullptr;
    FHitResult FocusHit;AActor* Focused=Focus(FocusHit);
    FVector Eye;FRotator View;Player->GetPlayerViewPoint(Eye,View);
    const float Reach=Settings ? Settings->CartInteractionReach : 350.f;
    auto Eligible=[&](AActor* Actor)
    {
        auto* Use=Cast<IUsableEquipment>(Actor);
        return IsValid(Actor) && !Actor->IsActorBeingDestroyed() && Use && Use->CanInteract(Pawn) &&
            Use->GetInteractionArea() && FVector::DistSquared(Eye,Use->GetInteractionArea()->GetComponentLocation())<=FMath::Square(Reach);
    };
    // A root collision hit belongs to the same equipment; do not require hitting its small area exactly.
    if (Eligible(Focused)) return Focused;
    if (Focused && (Focused->IsA<ABuilding>() || Focused->IsA<AGateSwitch>())) return nullptr;
    TArray<FOverlapResult> Nearby;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(EquipmentInteraction),false,Pawn);
    if (Equipment) Params.AddIgnoredActor(Equipment);
    if (Held) Params.AddIgnoredActor(Held);
    GetWorld()->OverlapMultiByObjectType(Nearby,Eye,FQuat::Identity,
        FCollisionObjectQueryParams::AllObjects,FCollisionShape::MakeSphere(Reach),Params);
    const FVector Aim=View.Vector().GetSafeNormal2D();
    AActor* Best=nullptr;float BestDistance=Reach;
    for (const FOverlapResult& Overlap : Nearby)
    {
        AActor* Actor=Overlap.GetActor();
        if (!Eligible(Actor)) continue;
        auto* Use=Cast<IUsableEquipment>(Actor);
        const FVector Grab=Use->GetInteractionArea()->GetComponentLocation();
        const float Distance=FVector::Dist(Eye,Grab);
        if (Distance>BestDistance || FVector::DotProduct((Grab-Eye).GetSafeNormal2D(),Aim)<.6f) continue;
        FHitResult Block;
        const bool bHit=GetWorld()->LineTraceSingleByChannel(Block,Eye,Grab,ECC_Visibility,Params);
        const auto* Body=Cast<ABorderZombie>(Block.GetActor());
        if (bHit && Block.GetActor()!=Actor && !(Body && Body->Carrier.Get()==Actor)) continue;
        Best=Actor;BestDistance=Distance;
    }
    return Best;
}
void UCorpseHandlingComponent::RecoverLegacyCargo()
{
    if (!Loadout() || !Settings || !Settings->CorpseClass) return;
    for (auto* Legacy : {&Loadout()->State.Backpack,&Loadout()->State.Cart})
    {
        for (int32 Index=Legacy->Num()-1;Index>=0;--Index)
        {
            FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            const FVector Position=GetOwner()->GetActorLocation()+GetOwner()->GetActorForwardVector()*220.f+FVector(0,Index*65.f,20.f);
            auto* Corpse=GetWorld()->SpawnActor<ABorderZombie>(Settings->CorpseClass,Position,GetOwner()->GetActorRotation(),Params);
            if (!Corpse) continue;
            Corpse->Health=1.f;UGameplayStatics::ApplyDamage(Corpse,1.f,nullptr,nullptr,nullptr);
            const auto& Old=(*Legacy)[Index];
            for (int32 Bone=0;Bone<FMath::Min(Old.Bones.Num(),Old.Pose.Num());++Bone)
                if (auto* Body=Corpse->GetMesh()->GetBodyInstance(Old.Bones[Bone]))
                {
                    FTransform Pose=Old.Pose[Bone];Pose.SetLocation(Position+GetOwner()->GetActorRotation().RotateVector(Pose.GetLocation()));
                    Pose.SetRotation(GetOwner()->GetActorQuat()*Pose.GetRotation());Body->SetBodyTransform(Pose,ETeleportType::TeleportPhysics);
                }
            Legacy->RemoveAt(Index);
        }
    }
}
FString UCorpseHandlingComponent::Hint() const
{
    if (!Loadout()) return FString();
    if (const auto* Use=GetUsedEquipment())
    {
        const FString EquipmentHint=Use->GetUseHint();
        if (!EquipmentHint.IsEmpty()) return EquipmentHint;
        return Use->IsDirectlyDriven() ? TEXT("F: 하차 | WASD: 장비 조종 | 우클릭: 배출") :
            FString::Printf(TEXT("적재 시체 %d구 | F: 손수레 놓기 | 이동: 수레 밀기 | 우클릭: 끝판 개폐·비우기"),CargoCount());
    }
    if (bActive)
        return FString::Printf(TEXT("%s | 적재 시체 %d구 | 좌클릭: 집기/놓기 | F: 수레 잡기/놓기"),Tool==EWorkTool::Repair ? TEXT("Hammer: hold E to repair") : TEXT("Corpse fork"),CargoCount());
    return FString::Printf(TEXT("적재 시체 %d구 | 3 Work tools | F grab/release cart | U building upgrade"),CargoCount());
}
void UCorpseHandlingComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick)
{
    Super::TickComponent(Dt,Type,Tick);
    if (!CurrentEquipment.IsExplicitlyNull() && !GetUsedEquipment())
    {
        CurrentEquipment.Reset();RefreshVisuals();if (Loadout()) Loadout()->RefreshWeapon();
    }
    if (!CanWork()) { if (Held) Release();if (IsPushing()) StopPushing();return; }
    if (Held && (!IsValid(Held) || Held->IsActorBeingDestroyed())) { Release(); return; }
    if (Held && Handle)
    {
        auto* Player=Cast<APlayerController>(Cast<APawn>(GetOwner())->GetController());
        if (!Player) { Release();return; }
        FVector Eye;FRotator View;Player->GetPlayerViewPoint(Eye,View);
        const FVector Target=Eye+View.Vector()*(Settings ? Settings->HoldDistance : 180.f)-FVector(0,0,35);
        const FName ConfiguredBone=Settings ? Settings->GrabBone : FName(TEXT("pelvis"));
        const FName Bone=Held->GetMesh()->GetBoneIndex(ConfiguredBone)!=INDEX_NONE ? ConfiguredBone : Held->CorpsePushBone;
        if (FVector::DistSquared(Held->GetMesh()->GetSocketLocation(Bone),Target)>FMath::Square(650.f)) Release();
        else Handle->SetTargetLocation(Target);
    }
}
