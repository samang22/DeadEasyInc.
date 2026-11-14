#pragma once
#include "CoreMinimal.h"
#include "Buildings/Building.h"
#include "CorpseProcessor.generated.h"

UCLASS()
class DEADEASYINC_API ACorpseProcessor : public ABuilding
{
    GENERATED_BODY()
public:
    ACorpseProcessor();
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UStaticMeshComponent> Rotor;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float ProcessingEffectSeconds=1.5f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) TObjectPtr<class UScavengingSettings> ScavengingSettings;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UBoxComponent> Intake;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class USceneComponent> ConsumePoint;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<TObjectPtr<class UStaticMeshComponent>> HopperParts;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UStaticMeshComponent> FeedChute;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<TObjectPtr<class UStaticMeshComponent>> ChuteSides;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite) TObjectPtr<class ACentralBoiler> Boiler;
    UFUNCTION() void ResetProcessing();
    UFUNCTION(BlueprintCallable,CallInEditor) bool SnapHousingToGround();
    UFUNCTION(BlueprintImplementableEvent) void OnProcessed(int32 Count);
private:
    float EffectRemaining=0.f;
    UPROPERTY() TObjectPtr<class UPhysicsHandleComponent> FeedMotor;
    UPROPERTY() TObjectPtr<class ABorderZombie> ProcessingCorpse;
    FVector FeedStart;
    float FeedTime=0.f;
    float NextIntakeScan=0.f;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};
