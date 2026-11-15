#include "Buildings/BuildingDamageDecalComponent.h"
#include "Components/DecalComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"

UBuildingDamageDecalComponent::UBuildingDamageDecalComponent()
{
    PrimaryComponentTick.bCanEverTick=false;
}

void UBuildingDamageDecalComponent::AddHit(const FHitResult& Hit)
{
    if (!DecalMaterial || GetNetMode()==NM_DedicatedServer || !Hit.bBlockingHit) return;
    auto* Mesh=Cast<UMeshComponent>(Hit.GetComponent());
    if (!IsValid(Mesh) || Mesh->GetOwner()!=GetOwner() || !Mesh->bReceivesDecals ||
        Hit.ImpactPoint.ContainsNaN() || Hit.ImpactNormal.ContainsNaN() || Hit.ImpactNormal.IsNearlyZero()) return;
    Decals.RemoveAll([](const auto& Decal){return !IsValid(Decal.Get());});
    const float DistanceSquared=FMath::Square(FMath::Max(1.f,MinimumDistance));
    for (const auto& Decal : Decals)
        if (FVector::DistSquared(Decal->GetComponentLocation(),Hit.ImpactPoint)<DistanceSquared) return;

    auto* Decal=NewObject<UDecalComponent>(GetOwner());
    if (!Decal) return;
    while (Decals.Num()>=5)
    {
        Decals[0]->DestroyComponent();
        Decals.RemoveAt(0);
    }
    Decal->PrimaryComponentTick.bCanEverTick=false;
    Decal->SetupAttachment(Mesh,Hit.BoneName);
    // Keep projection size bounded even on scaled building actors.
    Decal->SetAbsolute(false,false,true);
    Decal->DecalSize=FVector(FMath::Clamp(DecalSize.X,1.f,20.f),
        FMath::Clamp(DecalSize.Y,1.f,150.f),FMath::Clamp(DecalSize.Z,1.f,150.f));
    Decal->SetDecalMaterial(DecalMaterial);
    Decal->bDestroyOwnerAfterFade=false;
    Decal->RegisterComponent();
    const FVector Normal=Hit.ImpactNormal.GetSafeNormal();
    Decal->SetWorldLocationAndRotation(Hit.ImpactPoint+Normal*.5f,Normal.Rotation());
    Decals.Add(Decal);
}

void UBuildingDamageDecalComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    for (const auto& Decal : Decals) if (IsValid(Decal)) Decal->DestroyComponent();
    Decals.Reset();
    Super::EndPlay(Reason);
}
