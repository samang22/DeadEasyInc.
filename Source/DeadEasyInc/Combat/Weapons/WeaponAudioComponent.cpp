#include "Combat/Weapons/WeaponAudioComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Progression/Data/LoadoutData.h"
#include "TimerManager.h"

UWeaponAudioComponent::UWeaponAudioComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UWeaponAudioComponent::Configure(UWeaponDefinition* Definition)
{
    WeaponDefinition = Definition;
}

float UWeaponAudioComponent::AudioPitch() const
{
    if (!WeaponDefinition) return 1.f;
    const auto& Audio=WeaponDefinition->Audio;
    return FMath::Clamp(Audio.Pitch+FMath::FRandRange(-Audio.PitchVariation,Audio.PitchVariation),.5f,2.f);
}
void UWeaponAudioComponent::PlayWeaponSound(USoundBase* Sound)
{
    if (!Sound) return;
    const auto* Pawn=Cast<APawn>(GetOwner()->GetOwner());
    const float Volume=WeaponDefinition ? WeaponDefinition->Audio.Volume : 1.f;
    auto* Concurrency=WeaponDefinition ? WeaponDefinition->Audio.Concurrency.Get() : nullptr;
    if (Pawn && Pawn->IsLocallyControlled())
        UGameplayStatics::PlaySound2D(this,Sound,Volume,AudioPitch(),0.f,Concurrency,GetOwner()->GetOwner(),false);
    else
        UGameplayStatics::PlaySoundAtLocation(this,Sound,GetOwner()->GetActorLocation(),FRotator::ZeroRotator,Volume,AudioPitch(),0.f,
            WeaponDefinition ? WeaponDefinition->Audio.ImpactAttenuation.Get() : nullptr,Concurrency,GetOwner()->GetOwner());
}
void UWeaponAudioComponent::PlayImpact(const FHitResult& Hit, bool bOrganic)
{
    if (!WeaponDefinition) return;
    const auto& Audio=WeaponDefinition->Audio;
    USoundBase* Sound=Audio.StructureImpactSound;
    if (bOrganic) Sound=Audio.OrganicImpactSound;
    else
    {
        const auto* Component=Hit.GetComponent();
        const AActor* Actor=Hit.GetActor();
        auto IsMetalName=[](const FString& Name)
        { return Name.Contains(TEXT("Metal")) || Name.Contains(TEXT("Steel")) || Name.Contains(TEXT("Iron")); };
        bool bMetal=(Actor && Actor->ActorHasTag(TEXT("Metal"))) || (Component && Component->ComponentHasTag(TEXT("Metal"))) ||
            (Hit.PhysMaterial.IsValid() && IsMetalName(Hit.PhysMaterial->GetName()));
        if (!bMetal && Component)
            for (int32 Index=0;Index<Component->GetNumMaterials();++Index)
                if (const auto* Material=Component->GetMaterial(Index);Material && IsMetalName(Material->GetName())) { bMetal=true;break; }
        if (bMetal && Audio.MetalImpactSound) Sound=Audio.MetalImpactSound;
    }
    if (Sound) UGameplayStatics::PlaySoundAtLocation(this,Sound,Hit.ImpactPoint,FRotator::ZeroRotator,Audio.Volume,
        AudioPitch(),0.f,Audio.ImpactAttenuation,Audio.Concurrency,GetOwner()->GetOwner());
}
void UWeaponAudioComponent::PlayEquip() { if (WeaponDefinition) PlayWeaponSound(WeaponDefinition->Audio.EquipSound); }
void UWeaponAudioComponent::PlayUnequip() { if (WeaponDefinition) PlayWeaponSound(WeaponDefinition->Audio.UnequipSound); }
void UWeaponAudioComponent::PlayDryFire() { if (WeaponDefinition) PlayWeaponSound(WeaponDefinition->Audio.DryFireSound); }
void UWeaponAudioComponent::CancelReloadAudio()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ReloadInsertTimer);
        GetWorld()->GetTimerManager().ClearTimer(ReloadEndTimer);
    }
    for (UAudioComponent* Voice : ReloadVoices) if (IsValid(Voice)) Voice->Stop();
    ReloadVoices.Reset();
}
void UWeaponAudioComponent::PlayReloadStage(USoundBase* Sound)
{
    if (!Sound || !WeaponDefinition) return;
    ReloadVoices.RemoveAll([](const auto& Voice){ return !IsValid(Voice) || !Voice->IsPlaying(); });
    UAudioComponent* Voice=nullptr;
    const auto* Pawn=Cast<APawn>(GetOwner()->GetOwner());
    const auto& Audio=WeaponDefinition->Audio;
    if (Pawn && Pawn->IsLocallyControlled())
        Voice=UGameplayStatics::SpawnSound2D(this,Sound,Audio.Volume,AudioPitch(),0.f,Audio.Concurrency,false,true);
    else
        Voice=UGameplayStatics::SpawnSoundAtLocation(this,Sound,GetOwner()->GetActorLocation(),FRotator::ZeroRotator,Audio.Volume,
            AudioPitch(),0.f,Audio.ImpactAttenuation,Audio.Concurrency,true);
    if (Voice) { Voice->bIsUISound=false;ReloadVoices.Add(Voice); }
}
void UWeaponAudioComponent::PlayReload(USoundBase* FallbackSound)
{
    CancelReloadAudio();
    if (!WeaponDefinition) { PlayWeaponSound(FallbackSound);return; }
    const auto& Audio=WeaponDefinition->Audio;
    PlayReloadStage(Audio.ReloadStartSound ? Audio.ReloadStartSound.Get() : FallbackSound);
    const float Duration=FMath::Max(.1f,WeaponDefinition->ReloadTime);
    const float Insert=FMath::Clamp(Audio.ReloadInsertFraction,.05f,.8f);
    const float End=FMath::Clamp(Audio.ReloadEndFraction,Insert+.05f,.98f);
    GetWorld()->GetTimerManager().SetTimer(ReloadInsertTimer,this,&UWeaponAudioComponent::PlayReloadInsert,Duration*Insert,false);
    GetWorld()->GetTimerManager().SetTimer(ReloadEndTimer,this,&UWeaponAudioComponent::PlayReloadEnd,Duration*End,false);
}
void UWeaponAudioComponent::PlayReloadInsert() { if (WeaponDefinition) PlayReloadStage(WeaponDefinition->Audio.ReloadInsertSound); }
void UWeaponAudioComponent::PlayReloadEnd() { if (WeaponDefinition) PlayReloadStage(WeaponDefinition->Audio.ReloadEndSound); }
