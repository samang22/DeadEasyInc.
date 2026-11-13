#pragma once

#include "CoreMinimal.h"
#include "Core/DeadEasyIncGameMode.h"
#include "Progression/Data/LoadoutData.h"
#include "BorderGameMode.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBorderPhaseStateChanged,bool,IsNight,int32,Day);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBorderRunFailed);

USTRUCT()
struct FBorderCheckpointActor
{
    GENERATED_BODY()
    UPROPERTY() TWeakObjectPtr<AActor> Original;
    UPROPERTY() TSubclassOf<AActor> ActorClass;
    UPROPERTY() FTransform Transform;
    UPROPERTY() TArray<FName> Tags;
    UPROPERTY() TMap<FName, FTransform> PhysicsBodies;
    UPROPERTY() float Health = 0.f;
    UPROPERTY() float MaxHealth = 0.f;
    UPROPERTY() int32 Wood = 0;
    UPROPERTY() bool bCorpse = false;
    UPROPERTY() TObjectPtr<AActor> WallTarget;
    UPROPERTY() bool bHasLoadout = false;
    UPROPERTY() FLoadoutState LoadoutState;
    UPROPERTY() float RestorationProgress = 0.f;
};

UCLASS()
class DEADEASYINC_API ABorderGameMode : public ADeadEasyIncGameMode
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable) FBorderPhaseStateChanged OnPhaseStateChanged;
    UPROPERTY(BlueprintAssignable) FBorderRunFailed OnRunFailed;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings")
    TObjectPtr<class UBorderRulesSettings> Settings;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Day Night")
    float TransitionDuration = 5.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Day Night")
    float DawnDelay = 2.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Day Night")
    float RetryDelay = 2.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Day Night")
    int32 DayNumber = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Day Night")
    bool bIsNight = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Day Night") bool bTransitioning = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Day Night") int32 RemainingZombies = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Day Night") bool bWaveRegistered = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Game Over")
    bool bGameOver = false;
    UFUNCTION(BlueprintCallable, Category="Day Night") void StartDay();
    UFUNCTION(BlueprintCallable, Category="Day Night") void StartNight();
    UFUNCTION(BlueprintCallable, Category="Game Over") void RetryFromDawn();
    UFUNCTION(BlueprintCallable) bool BeginNight(class ABorderGate* Gate);
    UFUNCTION(BlueprintPure) bool CanBeginNight(class ABorderGate* Gate) const;
    UFUNCTION(BlueprintCallable) void RegisterWaveSource(AActor* Source);
    UFUNCTION(BlueprintCallable) void ReportSpawningFinished(AActor* Source);
    UFUNCTION(BlueprintCallable) void RegisterNightZombie(class ABorderZombie* Zombie);
    UFUNCTION(BlueprintImplementableEvent, Category="Day Night") void OnPhaseChanged(bool IsNight, int32 Day);
    UFUNCTION(BlueprintImplementableEvent, Category="Game Over") void OnGameOver();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void HandleDefeat();
    UFUNCTION() void FinishNightTransition();
    UFUNCTION() void CheckNightCompletion();
    UFUNCTION() void OnNightZombieDestroyed(AActor* Actor);
    void LockTransitionInput(bool bLocked);
    void CaptureDawn();
    void BindDefeatConditions();
    void ApplyPhase();
    bool IsCheckpointActor(AActor* Actor) const;
    UPROPERTY() TArray<FBorderCheckpointActor> Dawn;
    FRotator DawnView;
    int32 CheckpointDay = 1;
    FTimerHandle PhaseTimer;
    FTimerHandle RetryTimer;
    UPROPERTY() TObjectPtr<class ADayNightDirector> Director;
    TWeakObjectPtr<class ABorderGate> ActiveGate;
    TSet<TWeakObjectPtr<class ABorderZombie>> NightZombies;
    TSet<TWeakObjectPtr<AActor>> WaveSources;
    TSet<TWeakObjectPtr<AActor>> FinishedSources;
};
