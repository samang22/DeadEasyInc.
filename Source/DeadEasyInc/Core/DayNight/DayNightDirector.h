#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataAsset.h"
#include "DayNightDirector.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDayNightTransitionFinished);
UCLASS(BlueprintType)
class DEADEASYINC_API UDayNightLightingSettings : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float NightSunIntensity = .05f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float NightSkyIntensity = .15f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float NightSunPitch = -25.f;
};

UCLASS()
class DEADEASYINC_API ADayNightDirector : public AActor
{
    GENERATED_BODY()
public:
    ADayNightDirector();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UDayNightLightingSettings> Settings;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<class ULevelSequence> ClosingSequence;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGuid DirectorBinding;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite) TObjectPtr<class ADirectionalLight> Sun;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite) TObjectPtr<class ASkyLight> Sky;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite) TArray<TObjectPtr<class ALight>> NightLights;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite) TArray<TObjectPtr<class ULightComponent>> NightLightComponents;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp) float TransitionAlpha = 0.f;
    UPROPERTY(BlueprintAssignable) FDayNightTransitionFinished OnTransitionFinished;
    void BeginTransition(class ABorderGate* Gate, float Duration);
    void ResetDay();
    void SetNight();
    virtual void Tick(float DeltaSeconds) override;
protected:
    virtual void BeginPlay() override;
private:
    void ApplyLighting();
    UFUNCTION() void CompleteTransition();
    UPROPERTY() TObjectPtr<class ULevelSequencePlayer> SequencePlayer;
    UPROPERTY() TObjectPtr<class ALevelSequenceActor> SequenceActor;
    TWeakObjectPtr<class ABorderGate> ActiveGate;
    float DaySunIntensity = 10.f;
    float DaySkyIntensity = 1.f;
    FRotator DaySunRotation;
    float StartTime = 0.f;
    float TransitionDuration = 5.f;
    bool bActive = false;
};
