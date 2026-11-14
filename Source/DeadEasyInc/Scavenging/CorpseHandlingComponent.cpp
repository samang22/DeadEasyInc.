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
            if (auto* Camera=GetOwner()->FindComponentByClass<UCameraComponent>()) Equipment->Fork->AttachToComponent(Camera,FAttachmentTransformRules::KeepRelativeTransform);
        }
    }
    RefreshVisuals();
}
void UCorpseHandlingComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    Release();if (IsPushing()) Handcart->StopPush();if (Equipment) Equipment->Destroy();if (Handle) Handle->DestroyComponent();Super::EndPlay(Reason);
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
    StopPushing();
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
    if (IsPushing() || !bActive || !CanWork() || Tool==EWorkTool::Repair || GetWorld()->GetTimeSeconds()<NextUseTime) return;
    NextUseTime=GetWorld()->GetTimeSeconds()+.2f;
    if (Held) { Release();return; }
    FHitResult Hit;auto* Corpse=Cast<ABorderZombie>(Focus(Hit));
    if (!Corpse || !Corpse->bDead || Corpse->IsActorBeingDestroyed() || Corpse->ProcessingOwner.IsValid()) return;
    Corpse->HandlingOwner=this;
    if (Corpse->Carrier.IsValid()) Corpse->Carrier->DetachCorpse(Corpse);
    Held=Corpse;
    FName Bone=Settings ? Settings->GrabBone : FName(TEXT("pelvis"));
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
bool UCorpseHandlingComponent::IsPushing() const { return IsValid(Handcart) && Handcart->Driver==GetOwner(); }
void UCorpseHandlingComponent::StopPushing()
{
    if (IsPushing()) Handcart->StopPush();
}
void UCorpseHandlingComponent::Store()
{
    if (!CanWork() || !Loadout()) return;
    if (IsPushing())
    {
        if (!Handcart->Dump()) Loadout()->Notify(TEXT("분류기 투입구 앞에 수레를 가까이 대고 우클릭하세요."),true);
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
    if (!CanWork()) return false;
    if (IsPushing()) { StopPushing();RefreshVisuals();return true; }
    FHitResult Hit;AActor* Target=Focus(Hit);
    if (auto* Cart=GetUsableHandcart())
    {
        if (Held) { Loadout()->Notify(TEXT("시체를 먼저 내려놓아 주세요."),true);return true; }
        Handcart=Cart;
        if (!Cart->StartPush(Cast<APawn>(GetOwner()))) { Loadout()->Notify(TEXT("수레 가까이에서 손수레 쪽을 바라보고 잡아주세요."),true);return true; }
        bActive=false;RefreshVisuals();Loadout()->RefreshWeapon();return true;
    }
    if (!Cast<ACorpseProcessor>(Target)) return false;
    Loadout()->Notify(TEXT("시체를 집어 투입구에 넣거나, 수레를 잡고 앞에서 우클릭해 비우세요."));return true;
}
AHandcart* UCorpseHandlingComponent::GetUsableHandcart() const
{
    if (!CanWork() || IsPushing()) return nullptr;
    const auto* Pawn=Cast<APawn>(GetOwner());
    const auto* Player=Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!Player) return nullptr;
    FHitResult FocusHit;AActor* Focused=Focus(FocusHit);
    if (auto* Direct=Cast<AHandcart>(Focused))
        if (!Direct->Driver && FVector::DistSquared2D(Direct->GetActorLocation(),Pawn->GetActorLocation())<=FMath::Square(450.f)) return Direct;
    // Explicitly aiming at a station or switch keeps that object's interaction.
    if (Focused && (Focused->IsA<ABuilding>() || Focused->IsA<AGateSwitch>())) return nullptr;
    FVector Eye;FRotator View;Player->GetPlayerViewPoint(Eye,View);
    const FVector Aim=View.Vector().GetSafeNormal2D();
    const float Reach=Settings ? Settings->CartInteractionReach : 350.f;
    AHandcart* Best=nullptr;float BestDistance=Reach;
    for (TActorIterator<AHandcart> It(GetWorld());It;++It)
    {
        if (It->Driver || It->IsActorBeingDestroyed() || FVector::DistSquared2D(It->GetActorLocation(),Pawn->GetActorLocation())>FMath::Square(450.f)) continue;
        const FVector Grab=It->GrabTarget->GetComponentLocation();
        const float Distance=FVector::Dist2D(Eye,Grab);
        if (Distance>BestDistance || FVector::DotProduct((It->GetActorLocation()-Eye).GetSafeNormal2D(),Aim)<.6f) continue;
        FHitResult Block;FCollisionQueryParams Params(SCENE_QUERY_STAT(CartInteraction),false,Pawn);
        if (Equipment) Params.AddIgnoredActor(Equipment);if (Held) Params.AddIgnoredActor(Held);
        const bool Hit=GetWorld()->LineTraceSingleByChannel(Block,Eye,Grab,ECC_Visibility,Params);
        const auto* Body=Cast<ABorderZombie>(Block.GetActor());
        if (Hit && Block.GetActor()!=*It && !(Body && Body->Carrier.Get()==*It)) continue;
        Best=*It;BestDistance=Distance;
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
    if (IsPushing()) return FString::Printf(TEXT("적재 시체 %d구 | F: 손수레 놓기 | 이동: 수레 밀기 | 우클릭: 끝판 개폐·비우기"),CargoCount());
    if (bActive)
        return FString::Printf(TEXT("%s | 적재 시체 %d구 | 좌클릭: 집기/놓기 | F: 수레 잡기/놓기"),Tool==EWorkTool::Repair ? TEXT("Repair tool: hold E") : TEXT("Corpse fork"),CargoCount());
    return FString::Printf(TEXT("적재 시체 %d구 | 3 Work tools | F grab/release cart | U building upgrade"),CargoCount());
}
void UCorpseHandlingComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick)
{
    Super::TickComponent(Dt,Type,Tick);
    if (!CanWork()) { if (Held) Release();StopPushing();return; }
    if (Held && (!IsValid(Held) || Held->IsActorBeingDestroyed())) { Release(); return; }
    if (Held && Handle)
    {
        auto* Player=Cast<APlayerController>(Cast<APawn>(GetOwner())->GetController());
        if (!Player) { Release();return; }
        FVector Eye;FRotator View;Player->GetPlayerViewPoint(Eye,View);
        const FVector Target=Eye+View.Vector()*(Settings ? Settings->HoldDistance : 180.f)-FVector(0,0,35);
        if (FVector::DistSquared(Held->GetMesh()->GetSocketLocation(Settings ? Settings->GrabBone : FName(TEXT("pelvis"))),Target)>FMath::Square(650.f)) Release();
        else Handle->SetTargetLocation(Target);
    }
}
