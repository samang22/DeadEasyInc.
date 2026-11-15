#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Handcart.generated.h"

UCLASS()
class DEADEASYINC_API AHandcart : public AActor
{
    GENERATED_BODY()
public:
    AHandcart();
    virtual void PostLoad() override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UBoxComponent> Collision;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UBoxComponent> GrabTarget;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class USceneComponent> BedPivot;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class USceneComponent> WheelAxle;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class USceneComponent> TailgatePivot;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UStaticMeshComponent> Tailgate;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UBoxComponent> TailgateCollision;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UBoxComponent> TailgateClosingArea;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UBoxComponent> CargoFloor;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UBoxComponent> CargoVolume;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<TObjectPtr<class UBoxComponent>> CargoWalls;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UStaticMeshComponent> Bed;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UStaticMeshComponent> HandleMesh;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<TObjectPtr<class UStaticMeshComponent>> Wheels;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) TObjectPtr<class UScavengingSettings> Settings;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class APawn> Driver;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UCorpseCargoComponent> CargoStorage;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UHandcartMovementComponent> CartMovement;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UHandcartDischargeComponent> Discharge;
    virtual bool StartPush(APawn* Player);
    virtual void StopPush();
    bool LoadCorpse(class ABorderZombie* Corpse,int32 Slot=INDEX_NONE);
    void DetachCorpse(class ABorderZombie* Corpse);
    void ReleaseCargo();
    int32 CargoCount() const;
    void RefreshCargo();
    virtual bool Dump();
    UFUNCTION(BlueprintCallable,CallInEditor) virtual bool SnapToGround();
    virtual void Tick(float DeltaSeconds) override;
    virtual void AdvanceDriverMovement(float DeltaSeconds);
    virtual void UpdateWheelVisuals();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void HandleCargoDetached(class ABorderZombie* Corpse);
    void HandleCargoAttached(class ABorderZombie* Corpse);
};
