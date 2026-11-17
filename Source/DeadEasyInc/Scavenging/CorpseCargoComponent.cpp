#include "Scavenging/CorpseCargoComponent.h"
#include "Scavenging/ScavengingData.h"
#include "Scavenging/Handcart.h"
#include "Zombies/BorderZombie.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "PhysicsEngine/BodyInstance.h"

UCorpseCargoComponent::UCorpseCargoComponent() { PrimaryComponentTick.bCanEverTick=false; }
void UCorpseCargoComponent::Configure(UScavengingSettings* InSettings,UBoxComponent* InVolume)
{
    if (Volume.IsValid())
    {
        Volume->OnComponentBeginOverlap.RemoveDynamic(this,&UCorpseCargoComponent::OnEnter);
        Volume->OnComponentEndOverlap.RemoveDynamic(this,&UCorpseCargoComponent::OnLeave);
    }
    Settings=InSettings;Volume=InVolume;Cart=Cast<AHandcart>(GetOwner());
    if (Volume.IsValid())
    {
        Volume->OnComponentBeginOverlap.AddUniqueDynamic(this,&UCorpseCargoComponent::OnEnter);
        Volume->OnComponentEndOverlap.AddUniqueDynamic(this,&UCorpseCargoComponent::OnLeave);
    }
    RefreshTracking();
}
bool UCorpseCargoComponent::IsInside(ABorderZombie* Corpse,float Margin) const
{
    if (!Volume.IsValid() || !IsValid(Corpse) || !Corpse->bDead || Corpse->ProcessingOwner.IsValid() || Corpse->HandlingOwner.IsValid() || Corpse->IntakeOwner.IsValid()) return false;
    const FName ConfiguredBone=Settings ? Settings->GrabBone : FName(TEXT("pelvis"));
    const FName Bone=Corpse->GetMesh()->GetBoneIndex(ConfiguredBone)!=INDEX_NONE ? ConfiguredBone : Corpse->CorpsePushBone;
    const FBodyInstance* Body=Corpse->GetMesh()->GetBodyInstance(Bone);
    const FVector Center=Body ? Body->GetUnrealWorldTransform().GetLocation() : Corpse->GetMesh()->GetSocketLocation(Bone);
    const FVector P=Volume->GetComponentTransform().InverseTransformPosition(Center);
    const FVector E=Volume->GetUnscaledBoxExtent()+FVector(Margin);
    return FMath::Abs(P.X)<=E.X && FMath::Abs(P.Y)<=E.Y && FMath::Abs(P.Z)<=E.Z;
}
bool UCorpseCargoComponent::TryTrack(ABorderZombie* Corpse)
{
    if (!IsInside(Corpse,0.f) || (Corpse->Carrier.IsValid() && Corpse->Carrier.Get()!=Cart.Get())) return false;
    if (!Tracked.Contains(Corpse)) { Tracked.Add(Corpse);OnAttached.Broadcast(Corpse); }
    return true;
}
void UCorpseCargoComponent::OnEnter(UPrimitiveComponent* Overlapped,AActor* Other,UPrimitiveComponent* OtherComponent,int32 BodyIndex,bool FromSweep,const FHitResult& Hit)
{
    if (auto* Corpse=Cast<ABorderZombie>(Other)) { Candidates.Add(Corpse);TryTrack(Corpse);UpdateTimer(); }
}
void UCorpseCargoComponent::OnLeave(UPrimitiveComponent* Overlapped,AActor* Other,UPrimitiveComponent* OtherComponent,int32 BodyIndex)
{
    if (auto* Corpse=Cast<ABorderZombie>(Other)) { if (!Volume.IsValid() || !Volume->IsOverlappingActor(Corpse)) Candidates.Remove(Corpse);UpdateMembership(); }
}
void UCorpseCargoComponent::UpdateTimer()
{
    if (Candidates.IsEmpty() && Tracked.IsEmpty()) GetWorld()->GetTimerManager().ClearTimer(MembershipTimer);
    else if (!GetWorld()->GetTimerManager().IsTimerActive(MembershipTimer))
        GetWorld()->GetTimerManager().SetTimer(MembershipTimer,this,&UCorpseCargoComponent::UpdateMembership,Settings ? FMath::Max(.05f,Settings->CargoScanInterval) : .2f,true);
}
void UCorpseCargoComponent::UpdateMembership()
{
    const float Margin=Settings ? FMath::Max(0.f,Settings->CargoExitMargin) : 20.f;
    for (auto It=Tracked.CreateIterator();It;++It)
    {
        ABorderZombie* Corpse=It->Get();
        if (!IsInside(Corpse,Margin)) { It.RemoveCurrent();if (Corpse) OnDetached.Broadcast(Corpse); }
    }
    for (auto It=Candidates.CreateIterator();It;++It)
    {
        if (!It->IsValid()) It.RemoveCurrent();
        else TryTrack(It->Get());
    }
    UpdateTimer();
}
void UCorpseCargoComponent::RefreshTracking()
{
    if (!Volume.IsValid()) return;
    Volume->UpdateOverlaps();
    TArray<AActor*> Nearby;Volume->GetOverlappingActors(Nearby,ABorderZombie::StaticClass());
    Candidates.Reset();for (AActor* Actor : Nearby) Candidates.Add(Cast<ABorderZombie>(Actor));
    UpdateMembership();
}
bool UCorpseCargoComponent::LoadCorpse(ABorderZombie* Corpse,int32 RequestedSlot,int32& AssignedSlot)
{
    AssignedSlot=INDEX_NONE;
    if (!IsInside(Corpse,0.f)) return false;
    Candidates.Add(Corpse);const bool Result=TryTrack(Corpse);UpdateTimer();return Result;
}
void UCorpseCargoComponent::DetachCorpse(ABorderZombie* Corpse)
{
    if (Tracked.Remove(Corpse)>0 && Corpse) OnDetached.Broadcast(Corpse);
    UpdateTimer();
}
void UCorpseCargoComponent::ReleaseCargo()
{
    const auto Previous=Tracked;Tracked.Reset();Candidates.Reset();
    for (const auto& Weak : Previous) if (Weak.IsValid()) OnDetached.Broadcast(Weak.Get());
    GetWorld()->GetTimerManager().ClearTimer(MembershipTimer);
}
int32 UCorpseCargoComponent::CargoCount() const { int32 Count=0;for (const auto& Weak : Tracked) if (Weak.IsValid()) ++Count;return Count; }
TArray<ABorderZombie*> UCorpseCargoComponent::GetCorpses() const { TArray<ABorderZombie*> Result;for (const auto& Weak : Tracked) if (Weak.IsValid()) Result.Add(Weak.Get());return Result; }
ABorderZombie* UCorpseCargoComponent::FirstCorpse() const { for (const auto& Weak : Tracked) if (Weak.IsValid()) return Weak.Get();return nullptr; }
void UCorpseCargoComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Volume.IsValid())
    {
        Volume->OnComponentBeginOverlap.RemoveDynamic(this,&UCorpseCargoComponent::OnEnter);
        Volume->OnComponentEndOverlap.RemoveDynamic(this,&UCorpseCargoComponent::OnLeave);
    }
    ReleaseCargo();OnAttached.Clear();OnDetached.Clear();Super::EndPlay(Reason);
}
