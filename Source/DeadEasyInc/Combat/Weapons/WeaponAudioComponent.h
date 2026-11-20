#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "WeaponAudioComponent.generated.h"

class UAudioComponent;
class USoundBase;
class UWeaponDefinition;

/** Plays weapon sounds and owns the timed reload voices. The weapon decides when actions occur. */
UCLASS()
class DEADEASYINC_API UWeaponAudioComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWeaponAudioComponent();
    void Configure(UWeaponDefinition* Definition);
    void PlayWeaponSound(USoundBase* Sound);
    void PlayImpact(const FHitResult& Hit, bool bOrganic);
    void PlayEquip();
    void PlayUnequip();
    void PlayDryFire();
    void PlayReload(USoundBase* FallbackSound);
    void CancelReloadAudio();

private:
    void PlayReloadStage(USoundBase* Sound);
    void PlayReloadInsert();
    void PlayReloadEnd();
    float AudioPitch() const;

    UPROPERTY(Transient) TObjectPtr<UWeaponDefinition> WeaponDefinition;
    UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> ReloadVoices;
    FTimerHandle ReloadInsertTimer;
    FTimerHandle ReloadEndTimer;
};
