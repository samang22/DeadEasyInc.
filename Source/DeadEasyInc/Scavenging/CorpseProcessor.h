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
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) float ProcessingEffectSeconds=1.5f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) TObjectPtr<class UScavengingSettings> ScavengingSettings;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UBoxComponent> Intake;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class USceneComponent> ConsumePoint;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UCorpseFeedComponent> FeedProcess;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditInstanceOnly,BlueprintReadWrite) TObjectPtr<class ACentralBoiler> Boiler;
    UFUNCTION() void ResetProcessing();
    UFUNCTION(BlueprintCallable,CallInEditor) bool SnapHousingToGround();
    UFUNCTION(BlueprintImplementableEvent) void OnProcessed(int32 Count);
private:
    UPROPERTY() TObjectPtr<class UPhysicsHandleComponent> FeedMotor;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};
