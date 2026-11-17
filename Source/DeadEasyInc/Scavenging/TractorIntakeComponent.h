#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TractorIntakeComponent.generated.h"

class ABorderZombie;
class AK17CollectorTractor;
class UBoxComponent;
class USceneComponent;

/** Owns suction reservations and observes freely simulated cage cargo. */
UCLASS(ClassGroup=(Scavenging),meta=(BlueprintSpawnableComponent))
class DEADEASYINC_API UTractorIntakeComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UTractorIntakeComponent();
    void Configure(UBoxComponent* Area,USceneComponent* Target,USceneComponent* Spawn,UBoxComponent* CargoArea,USceneComponent* Outlet,float Interval);
    void SetIntakeEnabled(bool bEnabled);
    void OnUseEnded();
    void FindCandidates(TArray<AActor*>& OutActors) const;
    bool BeginDischarge();
    UFUNCTION(BlueprintPure,Category="Tractor Intake") int32 GetCargoCount() const;
    UFUNCTION(BlueprintPure,Category="Tractor Intake") bool IsIntakeEnabled() const { return bIntakeEnabled; }
    UFUNCTION(BlueprintPure,Category="Tractor Intake") bool IsDischarging() const { return bDischarging; }

    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Intake|Area",meta=(ClampMin="1")) float IntakeDistance=500.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Intake|Area",meta=(ClampMin="1")) float IntakeWidth=300.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Intake|Area",meta=(ClampMin="1")) float IntakeHeight=200.f;
    // Acceleration in cm/s^2, applied to every simulated ragdoll body.
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Intake|Physics",meta=(ClampMin="1")) float IntakeAcceleration=1400.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Intake|Physics",meta=(ClampMin="1")) float MaxIntakeSpeed=700.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Intake|Physics",meta=(ClampMin="1")) float CompletionDistance=100.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Intake|Physics") FName PullBone=TEXT("pelvis");
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Intake|Physics",meta=(ClampMin="0.5")) float MaxPullSeconds=8.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Intake|Cargo",meta=(ClampMin="1")) int32 MaxCargo=20;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Intake|Cargo",meta=(ClampMin="0")) float CargoExitMargin=30.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Intake|Cargo",meta=(ClampMin="1",ToolTip="Debug marker radius only. Nearby bodies and obstacles do not block cargo insertion.")) float CargoSpawnClearance=40.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Intake|Cargo",meta=(ClampMin="0.05")) float CargoSpawnInterval=.15f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Intake|Cargo") bool bUseCargoCCD=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Intake|Discharge",meta=(ClampMin="1")) float DischargeSpacing=110.f;
    // Velocity-change impulse (cm/s), independent of corpse mass.
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Intake|Discharge",meta=(ClampMin="0")) float DischargeImpulse=450.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Intake|Discharge",meta=(ClampMin="0")) float DischargeUpImpulse=80.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Intake|Debug") bool bDebugDraw=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Intake|Debug") int32 LastCandidateCount=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Intake|Debug") int32 LastObstructedCount=0;
    virtual void TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick) override;
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    struct FPulling
    {
        TWeakObjectPtr<ABorderZombie> Corpse;
        float Age=0.f;
        float StalledTime=0.f;
        float BestDistance=MAX_flt;
        float NextStoreAttempt=0.f;
    };
    struct FStored
    {
        TWeakObjectPtr<ABorderZombie> Corpse;
        TArray<FName> EnabledCCDBones;
    };
    TWeakObjectPtr<AK17CollectorTractor> Tractor;
    TWeakObjectPtr<UBoxComponent> IntakeArea;
    TWeakObjectPtr<USceneComponent> IntakeTarget;
    TWeakObjectPtr<USceneComponent> CageSpawnPoint;
    TWeakObjectPtr<UBoxComponent> CageCargoArea;
    TWeakObjectPtr<USceneComponent> DischargePoint;
    TArray<FPulling> Pulling;
    TArray<FStored> Stored;
    TMap<TWeakObjectPtr<ABorderZombie>,float> RetryAfter;
    FTimerHandle ScanTimer;
    float ScanInterval=.15f;
    float DischargeTime=0.f;
    float NextCargoSpawnTime=0.f;
    bool bIntakeEnabled=false;
    bool bDischarging=false;
    bool bReleasedBatch=false;
    UFUNCTION() void HandleCorpseDestroyed(AActor* Actor);
    void Scan();
    void UpdateActivity();
    bool CanClaim(ABorderZombie* Corpse) const;
    FName ResolvePullBone(const ABorderZombie* Corpse) const;
    FVector CorpsePosition(ABorderZombie* Corpse) const;
    void ReleasePull(ABorderZombie* Corpse,bool bRetryDelay);
    bool StoreCorpse(ABorderZombie* Corpse);
    void RefreshStored();
    void ReleaseStoredOwnership(FStored& State);
    void TeleportCorpse(ABorderZombie* Corpse,const FVector& Position);
    void RestoreCorpse(FStored& State,const FVector& Position,bool bImpulse);
    void ReleaseBatch();
    void DrawDebug() const;
};
