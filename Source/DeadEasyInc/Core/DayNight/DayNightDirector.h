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
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<class ULevelSequence> OpeningSequence;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Transition Camera") TObjectPtr<class UCameraComponent> OverviewCamera;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Transition Camera") TObjectPtr<class AActor> OverviewViewTarget;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Transition Camera") TObjectPtr<class AActor> OverviewFocus;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transition Camera") bool bAutoFrameShelter=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transition Camera") FVector OverviewOffset=FVector(-2200.f,-2200.f,1400.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transition Camera",meta=(ClampMin="0")) float CameraBlendTime=.5f;
    bool AreNightLightsOn() const { return bNightLightsOn; }
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite) TObjectPtr<class ADirectionalLight> Sun;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite) TObjectPtr<class ASkyLight> Sky;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite) TArray<TObjectPtr<class ALight>> NightLights;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite) TArray<TObjectPtr<class ULightComponent>> NightLightComponents;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp) float TransitionAlpha = 0.f;
    UPROPERTY(BlueprintAssignable) FDayNightTransitionFinished OnTransitionFinished;
    void BeginTransition(class ABorderGate* Gate, float Duration);
    void BeginDayTransition(class ABorderGate* Gate, float Duration);
    void ResetDay();
    void SetNight();
    virtual void Tick(float DeltaSeconds) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void ApplyLighting();
    void StartTransition(class ABorderGate* Gate,float Duration,bool bToNight);
    UFUNCTION() void CompleteTransition();
    UPROPERTY() TObjectPtr<class UDayNightCinematicComponent> Cinematic;
    TWeakObjectPtr<class ABorderGate> ActiveGate;
    float DaySunIntensity = 10.f;
    float DaySkyIntensity = 1.f;
    FRotator DaySunRotation;
    float StartTime = 0.f;
    float TransitionDuration = 5.f;
    bool bActive = false;
    bool bTransitionToNight=true;
    bool bNightLightsOn=false;
};
