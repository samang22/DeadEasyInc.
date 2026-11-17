#include "Scavenging/TractorIntakeComponent.h"
#include "Scavenging/K17CollectorTractor.h"
#include "Zombies/BorderZombie.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

UTractorIntakeComponent::UTractorIntakeComponent()
{
    SetAutoActivate(false);
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.bStartWithTickEnabled=false;
    PrimaryComponentTick.TickGroup=TG_PrePhysics;
}
void UTractorIntakeComponent::Configure(UBoxComponent* Area,USceneComponent* Target,USceneComponent* Spawn,UBoxComponent* CargoArea,USceneComponent* Outlet,float Interval)
{
    Tractor=Cast<AK17CollectorTractor>(GetOwner());
    IntakeArea=Area;IntakeTarget=Target;CageSpawnPoint=Spawn;CageCargoArea=CargoArea;DischargePoint=Outlet;
    ScanInterval=FMath::Max(.05f,Interval);
    if (Area)
    {
        Area->SetBoxExtent(FVector(FMath::Max(1.f,IntakeDistance)*.5f,FMath::Max(1.f,IntakeWidth)*.5f,FMath::Max(1.f,IntakeHeight)*.5f));
        Area->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Area->SetCollisionObjectType(ECC_GameTraceChannel5);
        Area->SetCollisionResponseToAllChannels(ECR_Ignore);
        Area->SetCollisionResponseToChannel(ECC_GameTraceChannel3,ECR_Overlap);
        Area->SetGenerateOverlapEvents(false); // Explicit local queries do not need overlap event bookkeeping.
    }
    AddTickPrerequisiteActor(GetOwner());
    UpdateActivity();
}
int32 UTractorIntakeComponent::GetCargoCount() const
{
    int32 Count=0;for (const FStored& State : Stored) if (State.Corpse.IsValid()) ++Count;return Count;
}
FName UTractorIntakeComponent::ResolvePullBone(const ABorderZombie* Corpse) const
{
    return Corpse->GetMesh()->GetBoneIndex(PullBone)!=INDEX_NONE ? PullBone : Corpse->CorpsePushBone;
}
FVector UTractorIntakeComponent::CorpsePosition(ABorderZombie* Corpse) const
{
    const FName Bone=ResolvePullBone(Corpse);
    if (const auto* Body=Corpse->GetMesh()->GetBodyInstance(Bone))
        return Body->GetUnrealWorldTransform().GetLocation();
    return Corpse->GetMesh()->GetSocketLocation(Bone);
}
bool UTractorIntakeComponent::CanClaim(ABorderZombie* Corpse) const
{
    return IsValid(Corpse) && Corpse->bDead && !Corpse->IsActorBeingDestroyed() &&
        !Corpse->Carrier.IsValid() && !Corpse->HandlingOwner.IsValid() && !Corpse->ProcessingOwner.IsValid() &&
        !Corpse->IntakeOwner.IsValid() && Corpse->GetMesh()->IsSimulatingPhysics(ResolvePullBone(Corpse));
}
void UTractorIntakeComponent::SetIntakeEnabled(bool bEnabled)
{
    bEnabled=bEnabled && !bDischarging && Tractor.IsValid() && Tractor->IsMounted();
    if (bIntakeEnabled==bEnabled && (bEnabled || Pulling.IsEmpty())) return;
    bIntakeEnabled=bEnabled;
    if (!bEnabled)
    {
        for (const FPulling& State : Pulling) ReleasePull(State.Corpse.Get(),false);
        Pulling.Reset();RetryAfter.Reset();
    }
    UpdateActivity();
    if (bEnabled) Scan();
}
void UTractorIntakeComponent::OnUseEnded()
{
    SetIntakeEnabled(false);
    bDischarging=false;bReleasedBatch=false;DischargeTime=0.f;
    if (Tractor.IsValid()) Tractor->SetFrontFlapOpen(false);
    UpdateActivity();
}
void UTractorIntakeComponent::UpdateActivity()
{
    if (!GetWorld()) return;
    const bool bScan=!Stored.IsEmpty() || (bIntakeEnabled && !bDischarging && GetCargoCount()+Pulling.Num()<FMath::Max(1,MaxCargo));
    if (bScan && !GetWorld()->GetTimerManager().IsTimerActive(ScanTimer))
        GetWorld()->GetTimerManager().SetTimer(ScanTimer,this,&UTractorIntakeComponent::Scan,ScanInterval,true);
    else if (!bScan) GetWorld()->GetTimerManager().ClearTimer(ScanTimer);
    SetComponentTickEnabled(!Pulling.IsEmpty() || bDischarging);
}
void UTractorIntakeComponent::Scan()
{
    RefreshStored();
    if (!bIntakeEnabled || bDischarging || !IntakeArea.IsValid() || !IntakeTarget.IsValid() || !Tractor.IsValid())
    { DrawDebug();UpdateActivity();return; }
    const float Now=GetWorld()->GetTimeSeconds();
    for (auto It=RetryAfter.CreateIterator();It;++It) if (!It.Key().IsValid() || It.Value()<=Now) It.RemoveCurrent();
    TArray<AActor*> Candidates;FindCandidates(Candidates);
    LastCandidateCount=Candidates.Num();LastObstructedCount=0;
    const FVector Target=IntakeTarget->GetComponentLocation();
    for (AActor* Candidate : Candidates)
    {
        if (!bIntakeEnabled || bDischarging || GetCargoCount()+Pulling.Num()>=FMath::Max(1,MaxCargo)) break;
        auto* Corpse=Cast<ABorderZombie>(Candidate);
        if (!CanClaim(Corpse) || RetryAfter.Contains(Corpse)) continue;
        const FVector Position=CorpsePosition(Corpse);
        if (FVector::DistSquared(Position,Target)>FMath::Square(FMath::Max(1.f,IntakeDistance)) ||
            FVector::DotProduct(Position-Target,GetOwner()->GetActorForwardVector()) < -CompletionDistance) continue;
        // Do not pull through buildings or terrain; corpses use a different object channel.
        FHitResult Obstacle;
        FCollisionQueryParams Sight(SCENE_QUERY_STAT(TractorIntakeSight),false,GetOwner());Sight.AddIgnoredActor(Corpse);
        if (GetWorld()->LineTraceSingleByObjectType(Obstacle,Position,Target,FCollisionObjectQueryParams(ECC_WorldStatic),Sight))
        {
            ++LastObstructedCount;
            if (bDebugDraw) DrawDebugLine(GetWorld(),Position,Obstacle.ImpactPoint,FColor::Red,false,ScanInterval+.03f);
            continue;
        }
        Corpse->GetMesh()->WakeAllRigidBodies();
        Corpse->IntakeOwner=this;
        Corpse->OnDestroyed.AddUniqueDynamic(this,&UTractorIntakeComponent::HandleCorpseDestroyed);
        FPulling& State=Pulling.AddDefaulted_GetRef();State.Corpse=Corpse;State.BestDistance=FVector::Dist(Position,Target);
        Tractor->OnIntakeCandidate(Corpse); // Preserve the existing BP notification after reserving ownership.
    }
    DrawDebug();UpdateActivity();
}
void UTractorIntakeComponent::FindCandidates(TArray<AActor*>& OutActors) const
{
    OutActors.Reset();if (!IntakeArea.IsValid() || !GetWorld()) return;
    TArray<FOverlapResult> Hits;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(TractorIntake),false,GetOwner());
    GetWorld()->OverlapMultiByObjectType(Hits,IntakeArea->GetComponentLocation(),IntakeArea->GetComponentQuat(),
        FCollisionObjectQueryParams(ECC_GameTraceChannel3),FCollisionShape::MakeBox(IntakeArea->GetScaledBoxExtent()),Params);
    for (const FOverlapResult& Hit : Hits)
        if (auto* Corpse=Cast<ABorderZombie>(Hit.GetActor());IsValid(Corpse) && Corpse->bDead) OutActors.AddUnique(Corpse);
}
void UTractorIntakeComponent::HandleCorpseDestroyed(AActor* Actor)
{
    Pulling.RemoveAll([Actor](const FPulling& State){ return !State.Corpse.IsValid() || State.Corpse.Get()==Actor; });
    Stored.RemoveAll([Actor](const FStored& State){ return !State.Corpse.IsValid() || State.Corpse.Get()==Actor; });
    RetryAfter.Remove(Cast<ABorderZombie>(Actor));UpdateActivity();
}
void UTractorIntakeComponent::ReleasePull(ABorderZombie* Corpse,bool bRetryDelay)
{
    if (!IsValid(Corpse)) return;
    Corpse->OnDestroyed.RemoveDynamic(this,&UTractorIntakeComponent::HandleCorpseDestroyed);
    if (Corpse->IntakeOwner.Get()==this) Corpse->IntakeOwner.Reset();
    if (bRetryDelay && GetWorld()) RetryAfter.Add(Corpse,GetWorld()->GetTimeSeconds()+2.f);
}
void UTractorIntakeComponent::TeleportCorpse(ABorderZombie* Corpse,const FVector& Position)
{
    auto* Mesh=Corpse->GetMesh();
    const FVector Offset=Position-CorpsePosition(Corpse);
    FTransform MeshWorld=Mesh->GetComponentTransform();MeshWorld.AddToTranslation(Offset);
    // Capture the current physical pose, not the pose from when this corpse was collected.
    TMap<FName,FTransform> Poses;
    TArray<FName> Bones;Mesh->GetBoneNames(Bones);
    for (FName Bone : Bones) if (auto* Body=Mesh->GetBodyInstance(Bone))
    {
        FTransform Pose=Body->GetUnrealWorldTransform();Pose.AddToTranslation(Offset);Poses.Add(Bone,Pose);
    }
    Corpse->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    Corpse->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
    Mesh->SetWorldTransform(MeshWorld,false,nullptr,ETeleportType::TeleportPhysics);
    if (!Mesh->IsSimulatingPhysics(ResolvePullBone(Corpse)))
    { Mesh->SetSimulatePhysics(true);Corpse->ApplyCorpseCollision(); }
    for (const auto& Pair : Poses) if (auto* Body=Mesh->GetBodyInstance(Pair.Key))
        Body->SetBodyTransform(Pair.Value,ETeleportType::TeleportPhysics);
    Mesh->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
    Mesh->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    Corpse->SetActorHiddenInGame(false);Mesh->SetVisibility(true);
    Mesh->WakeAllRigidBodies();
}
bool UTractorIntakeComponent::StoreCorpse(ABorderZombie* Corpse)
{
    if (!IsValid(Corpse) || Corpse->IntakeOwner.Get()!=this || !CageSpawnPoint.IsValid() || !CageCargoArea.IsValid() ||
        GetCargoCount()>=FMath::Max(1,MaxCargo) || GetWorld()->GetTimeSeconds()<NextCargoSpawnTime) return false;
    const FVector Position=CageSpawnPoint->GetComponentLocation();
    const FVector Local=CageCargoArea->GetComponentTransform().InverseTransformPosition(Position);
    const FVector Extent=CageCargoArea->GetUnscaledBoxExtent();
    // A misplaced spawn point must be fixed in BP, not silently auto-positioned by code.
    if (FMath::Abs(Local.X)>Extent.X || FMath::Abs(Local.Y)>Extent.Y || FMath::Abs(Local.Z)>Extent.Z) return false;
    FStored State;State.Corpse=Corpse;
    TeleportCorpse(Corpse,Position);
    if (bUseCargoCCD)
    {
        TArray<FName> Bones;Corpse->GetMesh()->GetBoneNames(Bones);
        for (FName Bone : Bones) if (auto* Body=Corpse->GetMesh()->GetBodyInstance(Bone))
            if (Body->IsInstanceSimulatingPhysics() && Body->GetCollisionEnabled()!=ECollisionEnabled::NoCollision && !Body->bUseCCD)
            { State.EnabledCCDBones.Add(Bone);Body->SetUseCCD(true); }
    }
    Corpse->TransportSlot=INDEX_NONE; // Real physics cargo has no fixed slot or attachment.
    Stored.Add(MoveTemp(State));
    NextCargoSpawnTime=GetWorld()->GetTimeSeconds()+FMath::Max(.05f,CargoSpawnInterval);
    return true;
}
void UTractorIntakeComponent::ReleaseStoredOwnership(FStored& State)
{
    ABorderZombie* Corpse=State.Corpse.Get();if (!IsValid(Corpse)) return;
    // Restore only CCD flags changed by this component; leave the pose and velocity alone.
    for (FName Bone : State.EnabledCCDBones) if (auto* Body=Corpse->GetMesh()->GetBodyInstance(Bone)) Body->SetUseCCD(false);
    Corpse->OnDestroyed.RemoveDynamic(this,&UTractorIntakeComponent::HandleCorpseDestroyed);
    if (Corpse->IntakeOwner.Get()==this) Corpse->IntakeOwner.Reset();
    Corpse->TransportSlot=INDEX_NONE;
}
void UTractorIntakeComponent::RefreshStored()
{
    for (int32 Index=Stored.Num()-1;Index>=0;--Index)
    {
        FStored& State=Stored[Index];ABorderZombie* Corpse=State.Corpse.Get();
        bool bInside=false;
        if (IsValid(Corpse) && !Corpse->IsActorBeingDestroyed() && Corpse->IntakeOwner.Get()==this && CageCargoArea.IsValid())
        {
            const FVector Local=CageCargoArea->GetComponentTransform().InverseTransformPosition(CorpsePosition(Corpse));
            const FVector Extent=CageCargoArea->GetUnscaledBoxExtent()+FVector(FMath::Max(0.f,CargoExitMargin));
            bInside=FMath::Abs(Local.X)<=Extent.X && FMath::Abs(Local.Y)<=Extent.Y && FMath::Abs(Local.Z)<=Extent.Z;
        }
        if (!bInside)
        {
            ReleaseStoredOwnership(State);
            if (IsValid(Corpse)) RetryAfter.Add(Corpse,GetWorld()->GetTimeSeconds()+2.f);
            Stored.RemoveAtSwap(Index);
        }
    }
}
void UTractorIntakeComponent::RestoreCorpse(FStored& State,const FVector& Position,bool bImpulse)
{
    ABorderZombie* Corpse=State.Corpse.Get();if (!IsValid(Corpse) || Corpse->IsActorBeingDestroyed()) return;
    TeleportCorpse(Corpse,Position);
    ReleaseStoredOwnership(State);
    if (bImpulse) Corpse->GetMesh()->AddImpulseToAllBodiesBelow(
        GetOwner()->GetActorForwardVector()*DischargeImpulse+FVector(0.f,0.f,DischargeUpImpulse),NAME_None,true,true);
}
bool UTractorIntakeComponent::BeginDischarge()
{
    RefreshStored();
    if (bDischarging || GetCargoCount()==0 || !Tractor.IsValid()) return false;
    SetIntakeEnabled(false);
    bDischarging=true;bReleasedBatch=false;DischargeTime=0.f;
    Tractor->SetFrontFlapOpen(true);UpdateActivity();return true;
}
void UTractorIntakeComponent::ReleaseBatch()
{
    if (!DischargePoint.IsValid()) return;
    const FVector Forward=GetOwner()->GetActorForwardVector();const FVector Right=GetOwner()->GetActorRightVector();
    const float Spacing=FMath::Max(80.f,DischargeSpacing);
    TArray<FStored> Remaining;
    int32 Index=0;
    for (FStored& State : Stored)
    {
        if (!State.Corpse.IsValid()) continue;
        const FVector Base=DischargePoint->GetComponentLocation()+Forward*((Index/3)*Spacing)+Right*((Index%3-1)*Spacing);
        bool bClear=false;FVector Position=Base;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(TractorDischarge),false,GetOwner());Params.AddIgnoredActor(State.Corpse.Get());
        for (int32 Attempt=0;Attempt<4;++Attempt)
        {
            Position=Base+Forward*(Attempt*Spacing)+FVector(0.f,0.f,Attempt*25.f);
            if (!GetWorld()->OverlapBlockingTestByChannel(Position,FQuat::Identity,ECC_GameTraceChannel3,
                FCollisionShape::MakeSphere(38.f),Params,FCollisionResponseParams(State.Corpse->GetMesh()->GetCollisionResponseToChannels()))) { bClear=true;break; }
        }
        if (bClear) RestoreCorpse(State,Position,true);
        else Remaining.Add(MoveTemp(State)); // Keep cargo if every front discharge point is obstructed.
        ++Index;
    }
    Stored=MoveTemp(Remaining);
}
void UTractorIntakeComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick)
{
    Super::TickComponent(Dt,Type,Tick);
    if (bDischarging)
    {
        if (!Tractor.IsValid()) { OnUseEnded();return; }
        DischargeTime+=Dt;
        if (!bReleasedBatch)
        {
            const bool bOpen=FMath::Abs(FMath::FindDeltaAngleDegrees(Tractor->FrontFlapPivot->GetRelativeRotation().Pitch,Tractor->FlapOpenPitch))<2.f;
            if (bOpen) { ReleaseBatch();bReleasedBatch=true;DischargeTime=0.f; }
            else if (DischargeTime>5.f) { OnUseEnded();return; }
        }
        else if (DischargeTime>=.4f)
        {
            Tractor->SetFrontFlapOpen(false);bDischarging=false;UpdateActivity();
        }
        return;
    }
    if (!bIntakeEnabled || !IntakeTarget.IsValid()) { SetIntakeEnabled(false);return; }
    const FVector Target=IntakeTarget->GetComponentLocation();
    for (int32 Index=Pulling.Num()-1;Index>=0;--Index)
    {
        FPulling& State=Pulling[Index];ABorderZombie* Corpse=State.Corpse.Get();
        if (!IsValid(Corpse) || Corpse->IsActorBeingDestroyed() || Corpse->IntakeOwner.Get()!=this ||
            Corpse->ProcessingOwner.IsValid() || Corpse->HandlingOwner.IsValid() || Corpse->Carrier.IsValid())
        { ReleasePull(Corpse,false);Pulling.RemoveAtSwap(Index);continue; }
        const FName Bone=ResolvePullBone(Corpse);
        auto* Mesh=Corpse->GetMesh();auto* Body=Mesh->GetBodyInstance(Bone);
        if (!Body || !Body->IsInstanceSimulatingPhysics()) { ReleasePull(Corpse,true);Pulling.RemoveAtSwap(Index);continue; }
        const FVector Delta=Target-CorpsePosition(Corpse);const float Distance=Delta.Size();
        State.Age+=Dt;
        if (Distance<=FMath::Max(1.f,CompletionDistance) && State.Age>=State.NextStoreAttempt)
        {
            State.NextStoreAttempt=State.Age+.1f;
            if (StoreCorpse(Corpse)) { Pulling.RemoveAtSwap(Index);continue; }
        }
        if (Distance<State.BestDistance-5.f) { State.BestDistance=Distance;State.StalledTime=0.f; }
        else State.StalledTime+=Dt;
        if (State.Age>FMath::Max(.5f,MaxPullSeconds) || State.StalledTime>2.f || Distance>IntakeDistance*1.5f)
        { ReleasePull(Corpse,true);Pulling.RemoveAtSwap(Index);continue; }
        const float Acceleration=FMath::Max(1.f,IntakeAcceleration);const float MaxSpeed=FMath::Max(1.f,MaxIntakeSpeed);
        const FVector Velocity=Mesh->GetPhysicsLinearVelocity(Bone);
        const float Speed=FMath::Min(MaxSpeed,FMath::Sqrt(2.f*Acceleration*FMath::Max(0.f,Distance-CompletionDistance*.5f)));
        FVector ForceAcceleration=((Delta.GetSafeNormal()*Speed-Velocity)/.2f).GetClampedToMaxSize(Acceleration);
        if (Delta.Z>20.f) ForceAcceleration.Z-=GetWorld()->GetGravityZ();
        ForceAcceleration.Z=FMath::Clamp(ForceAcceleration.Z,-Acceleration,Acceleration);
        // The entire ragdoll must overcome ground friction, not just the pelvis body's mass.
        Mesh->AddForceToAllBodiesBelow(ForceAcceleration,NAME_None,true,true);
        if (Velocity.SizeSquared()>FMath::Square(MaxSpeed)) Mesh->SetPhysicsLinearVelocity(Velocity.GetClampedToMaxSize(MaxSpeed),false,Bone);
    }
    UpdateActivity();
}
void UTractorIntakeComponent::DrawDebug() const
{
    if (!bDebugDraw || !IntakeArea.IsValid() || !IntakeTarget.IsValid()) return;
    DrawDebugBox(GetWorld(),IntakeArea->GetComponentLocation(),IntakeArea->GetScaledBoxExtent(),IntakeArea->GetComponentQuat(),FColor::Cyan,false,ScanInterval+.03f);
    DrawDebugSphere(GetWorld(),IntakeTarget->GetComponentLocation(),CompletionDistance,12,FColor::Yellow,false,ScanInterval+.03f);
    if (CageCargoArea.IsValid()) DrawDebugBox(GetWorld(),CageCargoArea->GetComponentLocation(),CageCargoArea->GetScaledBoxExtent(),CageCargoArea->GetComponentQuat(),FColor::Green,false,ScanInterval+.03f);
    if (CageSpawnPoint.IsValid()) DrawDebugSphere(GetWorld(),CageSpawnPoint->GetComponentLocation(),CargoSpawnClearance,12,FColor::Magenta,false,ScanInterval+.03f);
    DrawDebugString(GetWorld(),IntakeTarget->GetComponentLocation()+FVector(0,0,100),FString::Printf(TEXT("Candidates %d | Pulling %d | Blocked %d | Cargo %d/%d"),LastCandidateCount,Pulling.Num(),LastObstructedCount,GetCargoCount(),MaxCargo),nullptr,FColor::White,ScanInterval+.03f);
}
void UTractorIntakeComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    OnUseEnded();
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ScanTimer);
    // Cage colliders disappear with their owner; leave the real ragdolls free at their current positions.
    for (FStored& State : Stored) ReleaseStoredOwnership(State);
    Stored.Reset();RetryAfter.Reset();Super::EndPlay(Reason);
}
