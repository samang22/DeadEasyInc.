#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZombieLineSpawner.generated.h"

class ABorderGameMode;
class ABorderZombie;
class UZombieWaveSettings;

UCLASS()
class DEADEASYINC_API AZombieLineSpawner : public AActor
{
    GENERATED_BODY()
public:
    AZombieLineSpawner();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawner") TObjectPtr<UZombieWaveSettings> Settings;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawner") TSubclassOf<ABorderZombie> ZombieClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner") bool bEnabled = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spawner") TObjectPtr<class UBoxComponent> LinePreview;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spawner") TObjectPtr<class UArrowComponent> DirectionArrow;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spawner") int32 SpawnedThisNight = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spawner") int32 AliveFromThisSpawner = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spawner") FString LastSpawnIssue;
    virtual void OnConstruction(const FTransform& Transform) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void HandlePhase(bool IsNight,int32 Day);
    UFUNCTION() void StopSpawning();
    UFUNCTION() void UpdateAliveCount();
    UFUNCTION() void HandleZombieDestroyed(AActor* Actor);
    void StartWave(int32 Day);
    void SpawnStep();
    void Schedule(float Delay);
    bool FindSpawnPoint(FVector& Position);
    void ShuffleSlots();
    TWeakObjectPtr<ABorderGameMode> Mode;
    TSet<TWeakObjectPtr<ABorderZombie>> Alive;
    FTimerHandle SpawnTimer;
    FRandomStream Random;
    TArray<int32> Slots;
    int32 Cursor = 0;
    int32 GroupSpawned = 0;
    int32 StartedDay = INDEX_NONE;
    bool bRunning = false;
};
