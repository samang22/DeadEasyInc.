#pragma once

#include "CoreMinimal.h"
#include "Core/DeadEasyIncGameMode.h"
#include "BorderGameMode.generated.h"

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
};

UCLASS()
class DEADEASYINC_API ABorderGameMode : public ADeadEasyIncGameMode
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings")
    TObjectPtr<class UBorderRulesSettings> Settings;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Day Night")
    float DayDuration = 180.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Day Night")
    float NightDuration = 120.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Day Night")
    float RetryDelay = 2.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Day Night")
    int32 DayNumber = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Day Night")
    bool bIsNight = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Game Over")
    bool bGameOver = false;
    UFUNCTION(BlueprintCallable, Category="Day Night") void StartDay();
    UFUNCTION(BlueprintCallable, Category="Day Night") void StartNight();
    UFUNCTION(BlueprintCallable, Category="Game Over") void RetryFromDawn();
    UFUNCTION(BlueprintImplementableEvent, Category="Day Night") void OnPhaseChanged(bool IsNight, int32 Day);
    UFUNCTION(BlueprintImplementableEvent, Category="Game Over") void OnGameOver();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void HandleDefeat();
    void CaptureDawn();
    void BindDefeatConditions();
    void ApplyPhase();
    bool IsCheckpointActor(AActor* Actor) const;
    UPROPERTY() TArray<FBorderCheckpointActor> Dawn;
    FRotator DawnView;
    int32 CheckpointDay = 1;
    FTimerHandle PhaseTimer;
    FTimerHandle RetryTimer;
};
